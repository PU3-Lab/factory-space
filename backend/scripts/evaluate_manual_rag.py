"""operator_guide RAG 검색 품질 평가 스크립트.

초보자용 설명:
    이 파일은 플레이어 질문을 RAG 검색기에 넣어 보고, 기대한 문서가 검색되는지
    채점한 뒤 마크다운 보고서를 만든다. RAG는 LLM이 답하기 전에 관련 문서를 먼저
    찾는 구조이고, pgvector는 PostgreSQL에서 문장 임베딩 벡터를 검색하게 해 주는
    확장 기능이다. Hit@1은 1순위 결과가 정답인지, Hit@5는 상위 5개 안에 정답이
    있는지를 뜻한다.
"""

from __future__ import annotations

import json
import os
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Protocol, cast


@dataclass(frozen=True)
class EvaluationThresholds:
    """RAG 평가가 통과하기 위한 기준값 묶음."""

    max_low_confidence_score: float = 0.35
    min_pass_rate: float = 1.0


@dataclass(frozen=True)
class EvaluationCaseResult:
    """평가 질문 1개의 채점 결과."""

    index: int
    question: str
    expected_doc_id: str | None
    expected_behavior: str
    actual_top_doc_id: str | None
    top_score: float
    expected_confidence: str | None
    actual_confidence: str
    is_hit1: bool
    is_hit5: bool
    is_confidence_match: bool
    is_passed: bool
    failure_reason: str


@dataclass(frozen=True)
class EvaluationReport:
    """전체 평가 질문셋을 채점한 요약 결과."""

    results: list[EvaluationCaseResult]
    total_cases: int
    expected_doc_count: int
    hit1_count: int
    hit5_count: int
    confidence_match_count: int
    total_passed: int
    hit1_rate: float
    hit5_rate: float
    confidence_match_rate: float
    avg_top_score: float
    pass_rate: float


class SearchDocument(Protocol):
    """RAG 검색 결과 문서가 갖춰야 하는 최소 필드."""

    doc_id: str
    score: float


class RetrievalResult(Protocol):
    """하위 질문 하나에 대한 RAG 검색 결과 묶음."""

    results: list[SearchDocument]


class SubQuestionResult(Protocol):
    """분해된 하위 질문 하나의 검색 결과."""

    retrieval: RetrievalResult


class RagResult(Protocol):
    """MultiQuestionRagRetriever가 반환하는 결과의 최소 인터페이스."""

    sub_question_results: list[SubQuestionResult]
    metadata: dict[str, object]


class RagRuntime(Protocol):
    """평가 스크립트가 기대하는 RAG runtime의 최소 인터페이스."""

    def retrieve(self, question: str) -> RagResult:
        """질문을 받아 RAG 검색 결과를 반환한다."""


def load_env_file(env_file: Path) -> None:
    """env 파일을 읽어 환경 변수로 넣는다.

    초보자용 설명:
        `.env.prod` 같은 파일에는 DB 주소나 API 키 이름이 들어 있다. 이미 터미널
        환경에 값이 있으면 그 값을 우선 사용하고, 비어 있을 때만 파일 값을 채운다.
    """

    if not env_file.exists():
        return

    for raw_line in env_file.read_text(encoding="utf-8-sig").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue

        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()
        if not key:
            continue
        if len(value) >= 2 and value[0] == value[-1] and value[0] in {"'", '"'}:
            value = value[1:-1]
        os.environ.setdefault(key, value)


def load_eval_questions(path: Path) -> list[dict[str, object]]:
    """평가 질문 JSON 파일을 읽어 리스트로 반환한다."""

    return json.loads(path.read_text(encoding="utf-8"))


def evaluate_questions(
    questions: list[dict[str, object]],
    rag_runtime: RagRuntime,
    *,
    thresholds: EvaluationThresholds | None = None,
) -> EvaluationReport:
    """질문셋을 RAG runtime에 넣고 Hit@1, Hit@5, confidence를 채점한다.

    초보자용 설명:
        실제 LLM 답변은 만들지 않는다. 질문을 RAG 검색기에 넣은 뒤 검색 결과만
        평가한다. 문서 ID가 있는 질문은 정답 문서가 상위 결과에 있는지 보고,
        문서 ID가 없는 애매한 질문은 검색기가 너무 확신하지 않는지도 함께 본다.
    """

    active_thresholds = thresholds or EvaluationThresholds()
    results: list[EvaluationCaseResult] = []
    hit1_count = 0
    hit5_count = 0
    expected_doc_count = 0
    confidence_match_count = 0
    total_top_score = 0.0
    score_count = 0

    for idx, item in enumerate(questions):
        question = str(item["question"])
        expected_doc_id = _optional_str(item.get("expected_doc_id"))
        expected_confidence = _optional_str(item.get("expected_confidence"))
        expected_behavior = str(
            item.get(
                "expected_behavior",
                "document_match" if expected_doc_id else "ambiguous_low_confidence",
            )
        )

        print(f" -> [{idx + 1}/{len(questions)}] 질의: {question}")
        rag_result = rag_runtime.retrieve(question)
        all_docs_sorted = _sorted_retrieved_docs(rag_result)
        actual_confidence = _representative_confidence(rag_result)

        top_doc_id = None
        top_score = 0.0
        if all_docs_sorted:
            top_doc_id = str(all_docs_sorted[0].doc_id)
            top_score = float(all_docs_sorted[0].score)
            total_top_score += top_score
            score_count += 1

        is_hit1 = False
        is_hit5 = False
        if expected_doc_id:
            expected_doc_count += 1
            is_hit1 = bool(
                all_docs_sorted and all_docs_sorted[0].doc_id == expected_doc_id
            )
            top5_doc_ids = [str(doc.doc_id) for doc in all_docs_sorted[:5]]
            is_hit5 = expected_doc_id in top5_doc_ids
            hit1_count += int(is_hit1)
            hit5_count += int(is_hit5)

        is_confidence_match = actual_confidence == expected_confidence
        confidence_match_count += int(is_confidence_match)
        is_passed, failure_reason = _evaluate_case_pass(
            expected_behavior=expected_behavior,
            expected_doc_id=expected_doc_id,
            is_hit5=is_hit5,
            is_confidence_match=is_confidence_match,
            actual_confidence=actual_confidence,
            top_score=top_score,
            thresholds=active_thresholds,
        )

        results.append(
            EvaluationCaseResult(
                index=idx + 1,
                question=question,
                expected_doc_id=expected_doc_id,
                expected_behavior=expected_behavior,
                actual_top_doc_id=top_doc_id,
                top_score=top_score,
                expected_confidence=expected_confidence,
                actual_confidence=actual_confidence,
                is_hit1=is_hit1,
                is_hit5=is_hit5,
                is_confidence_match=is_confidence_match,
                is_passed=is_passed,
                failure_reason=failure_reason,
            )
        )

    total_cases = len(questions)
    total_passed = sum(1 for result in results if result.is_passed)
    return EvaluationReport(
        results=results,
        total_cases=total_cases,
        expected_doc_count=expected_doc_count,
        hit1_count=hit1_count,
        hit5_count=hit5_count,
        confidence_match_count=confidence_match_count,
        total_passed=total_passed,
        hit1_rate=(hit1_count / expected_doc_count) if expected_doc_count else 1.0,
        hit5_rate=(hit5_count / expected_doc_count) if expected_doc_count else 1.0,
        confidence_match_rate=(confidence_match_count / total_cases)
        if total_cases
        else 1.0,
        avg_top_score=(total_top_score / score_count) if score_count else 0.0,
        pass_rate=(total_passed / total_cases) if total_cases else 1.0,
    )


def evaluation_exit_code(report: EvaluationReport, *, min_pass_rate: float) -> int:
    """평가 결과가 기준 미달이면 프로세스 실패 코드 1을 반환한다."""

    return 0 if report.pass_rate >= min_pass_rate else 1


def write_markdown_report(report: EvaluationReport, report_path: Path) -> None:
    """평가 결과를 사람이 읽기 쉬운 마크다운 문서로 저장한다."""

    report_path.parent.mkdir(exist_ok=True)
    report_path.write_text(render_markdown_report(report), encoding="utf-8")


def render_markdown_report(report: EvaluationReport) -> str:
    """평가 결과를 마크다운 문자열로 만든다."""

    report_lines = [
        "# RAG 검색 품질 평가 보고서 (RAG Evaluation Report)",
        f"\n* **평가 일시**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
        "\n## 품질 지표 요약 (Summary)",
        "\n| 지표명 | 수치 | 설명 |",
        "| --- | --- | --- |",
        f"| **총 평가 케이스 수** | {report.total_cases} | 테스트 질문의 총합 |",
        f"| **최종 통과율 (Pass Rate)** | {report.pass_rate:.1%} ({report.total_passed}/{report.total_cases}) | 케이스별 기대 행동 충족률 |",
        f"| **Hit@1 적중률 (Hit@1 Rate)** | {report.hit1_rate:.1%} ({report.hit1_count}/{report.expected_doc_count}) | 1순위 검색 결과 일치도 |",
        f"| **Hit@5 적중률 (Hit@5 Rate)** | {report.hit5_rate:.1%} ({report.hit5_count}/{report.expected_doc_count}) | 상위 5개 문서 내 정답 포함율 |",
        f"| **신뢰도 매칭율 (Confidence Match)** | {report.confidence_match_rate:.1%} ({report.confidence_match_count}/{report.total_cases}) | RAG 판정 신뢰도와 기댓값의 일치도 |",
        f"| **평균 최상위 유사도 점수 (Avg Top Score)** | {report.avg_top_score:.4f} | 매칭된 최상위 문서들의 평균 Cosine 유사도 점수 |",
        "\n## 질문 케이스별 채점 상세 (Detailed Results)",
        "\n| 번호 | 질문 | 기대 행동 | 기대 문서 ID | 실측 탑 문서 ID | 최고 점수 | 기대 신뢰도 | 실측 신뢰도 | Hit@1 | Hit@5 | 신뢰도 일치 | 최종 판정 | 실패 이유 |",
        "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |",
    ]

    for result in report.results:
        expected_doc_id = result.expected_doc_id or "N/A"
        actual_top_doc_id = result.actual_top_doc_id or "N/A"
        hit1_str = "O" if result.is_hit1 else ("X" if result.expected_doc_id else "-")
        hit5_str = "O" if result.is_hit5 else ("X" if result.expected_doc_id else "-")
        conf_str = "O" if result.is_confidence_match else "X"
        pass_str = "PASS" if result.is_passed else "FAIL"
        failure_reason = result.failure_reason or "-"
        report_lines.append(
            f"| {result.index} | {result.question} | `{result.expected_behavior}` | "
            f"`{expected_doc_id}` | `{actual_top_doc_id}` | {result.top_score:.4f} | "
            f"`{result.expected_confidence}` | `{result.actual_confidence}` | "
            f"{hit1_str} | {hit5_str} | {conf_str} | **{pass_str}** | "
            f"`{failure_reason}` |"
        )

    return "\n".join(report_lines) + "\n"


def build_rag_runtime() -> RagRuntime:
    """실제 PostgreSQL/pgvector 기반 RAG runtime을 만든다.

    초보자용 설명:
        평가 스크립트가 직접 검색할 수 있도록 embedding provider, pgvector 저장소,
        retriever를 한 번에 조립한다. 테스트에서는 이 함수를 쓰지 않고 fake runtime을
        넣어서 외부 API 없이 채점 규칙만 검증한다.
    """

    from agents.operator_guide.multi_question_rag_retriever import (
        MultiQuestionRagRetriever,
    )  # noqa: I001
    from agents.operator_guide.rag_embedding import create_embedding_provider
    from agents.operator_guide.rag_retriever import ManualRagRetriever
    from agents.operator_guide.rag_store import SqlAlchemyManualRagStore
    from db.engine import engine

    embedding_provider = create_embedding_provider()
    search_store = SqlAlchemyManualRagStore(engine)
    retriever = ManualRagRetriever(
        embedding_provider=embedding_provider,
        search_store=search_store,
    )
    return MultiQuestionRagRetriever(rag_retriever=retriever)


def run() -> int:
    """평가 스크립트를 실행하고 성공/실패 exit code를 반환한다."""

    backend_root = Path(__file__).resolve().parents[1]
    load_env_file(backend_root / ".env.prod")

    src_path = str(backend_root / "src")
    if src_path not in sys.path:
        sys.path.insert(0, src_path)

    print("[RAG Evaluator] RAG 시스템 초기화 시작...")
    try:
        rag_runtime = build_rag_runtime()
        print("[RAG Evaluator] RAG 시스템 초기화 성공!")
    except Exception as exc:
        print(f"[RAG Evaluator] RAG 시스템 초기화 실패: {exc}")
        return 1

    questions_path = backend_root / "data" / "rag_eval_questions.json"
    if not questions_path.exists():
        print(
            f"[RAG Evaluator] 에러: 평가 데이터셋 파일이 존재하지 않습니다: {questions_path}"
        )
        return 1

    questions = load_eval_questions(questions_path)
    print(f"[RAG Evaluator] 총 {len(questions)}개의 평가 질문 케이스를 로드했습니다.")
    print("[RAG Evaluator] RAG 검색 품질 채점 진행 중...")
    thresholds = EvaluationThresholds()
    report = evaluate_questions(questions, rag_runtime, thresholds=thresholds)

    report_path = backend_root / "docs" / "rag_evaluation_report.md"
    write_markdown_report(report, report_path)
    _print_summary(report, report_path)
    return evaluation_exit_code(report, min_pass_rate=thresholds.min_pass_rate)


def _optional_str(value: object) -> str | None:
    if value is None:
        return None
    return str(value)


def _sorted_retrieved_docs(rag_result: RagResult) -> list[SearchDocument]:
    all_docs: list[SearchDocument] = []
    for sub_res in rag_result.sub_question_results:
        all_docs.extend(sub_res.retrieval.results)
    return sorted(all_docs, key=lambda item: float(item.score), reverse=True)


def _representative_confidence(rag_result: RagResult) -> str:
    counts = cast(dict[str, int], rag_result.metadata.get("confidence_counts", {}))
    if counts.get("low", 0) > 0:
        return "low"
    if counts.get("medium", 0) > 0:
        return "medium"
    return "high"


def _evaluate_case_pass(
    *,
    expected_behavior: str,
    expected_doc_id: str | None,
    is_hit5: bool,
    is_confidence_match: bool,
    actual_confidence: str,
    top_score: float,
    thresholds: EvaluationThresholds,
) -> tuple[bool, str]:
    if expected_behavior == "document_match" or expected_doc_id:
        if not is_hit5:
            return False, "expected_doc_not_found_in_top5"
        if not is_confidence_match:
            return False, "confidence_mismatch"
        return True, ""

    if expected_behavior in {"ambiguous_low_confidence", "out_of_scope_low_confidence"}:
        if not is_confidence_match or actual_confidence != "low":
            return False, "confidence_mismatch"
        if top_score > thresholds.max_low_confidence_score:
            return False, "top_score_exceeded_low_confidence_limit"
        return True, ""

    return False, f"unknown_expected_behavior:{expected_behavior}"


def _print_summary(report: EvaluationReport, report_path: Path) -> None:
    print("\n" + "=" * 60)
    print("                 RAG 검색 품질 평가 완료")
    print("=" * 60)
    print(f"평가 일시: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"총 케이스: {report.total_cases} 개")
    print(
        f"최종 통과율: {report.pass_rate:.1%} ({report.total_passed}/{report.total_cases})"
    )
    print(f"Hit@1 적중률: {report.hit1_rate:.1%}")
    print(f"Hit@5 적중률: {report.hit5_rate:.1%}")
    print(f"신뢰도 일치율: {report.confidence_match_rate:.1%}")
    print(f"평균 유사 스코어: {report.avg_top_score:.4f}")
    print("=" * 60)
    print(f"상세 리포트가 작성되었습니다: {report_path.resolve()}")
    print("=" * 60)


def main() -> None:
    """명령줄에서 실행할 때 평가를 수행하고 exit code를 반환한다."""

    sys.exit(run())


if __name__ == "__main__":
    main()
