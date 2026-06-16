"""operator_guide RAG 평가 스크립트 채점 규칙 테스트."""

from __future__ import annotations

from dataclasses import dataclass

from scripts.evaluate_manual_rag import (
    EvaluationThresholds,
    evaluate_questions,
    evaluation_exit_code,
)


@dataclass
class FakeSearchResult:
    doc_id: str
    score: float


@dataclass
class FakeRetrieval:
    results: list[FakeSearchResult]


@dataclass
class FakeSubQuestionResult:
    retrieval: FakeRetrieval


@dataclass
class FakeRagResult:
    sub_question_results: list[FakeSubQuestionResult]
    metadata: dict[str, object]


class FakeRagRuntime:
    def __init__(self, result: FakeRagResult) -> None:
        self.result = result

    def retrieve(self, question: str) -> FakeRagResult:
        _ = question
        return self.result


def test_evaluate_questions_sorts_sub_question_results_by_score() -> None:
    runtime = FakeRagRuntime(
        FakeRagResult(
            sub_question_results=[
                FakeSubQuestionResult(FakeRetrieval([FakeSearchResult("doc:low", 0.25)])),
                FakeSubQuestionResult(FakeRetrieval([FakeSearchResult("doc:expected", 0.80)])),
            ],
            metadata={"confidence_counts": {"high": 1, "medium": 0, "low": 0}},
        ),
    )

    report = evaluate_questions(
        [
            {
                "question": "기어는 어떻게 만들어?",
                "expected_doc_id": "doc:expected",
                "expected_confidence": "high",
                "expected_behavior": "document_match",
            }
        ],
        runtime,
    )

    assert report.results[0].actual_top_doc_id == "doc:expected"
    assert report.results[0].is_hit1 is True
    assert report.pass_rate == 1.0


def test_evaluate_questions_fails_ambiguous_case_when_score_is_too_high() -> None:
    runtime = FakeRagRuntime(
        FakeRagResult(
            sub_question_results=[
                FakeSubQuestionResult(FakeRetrieval([FakeSearchResult("action:too_specific", 0.55)])),
            ],
            metadata={"confidence_counts": {"high": 0, "medium": 0, "low": 1}},
        ),
    )

    report = evaluate_questions(
        [
            {
                "question": "라인이 이상해",
                "expected_doc_id": None,
                "expected_confidence": "low",
                "expected_behavior": "ambiguous_low_confidence",
            }
        ],
        runtime,
        thresholds=EvaluationThresholds(max_low_confidence_score=0.35),
    )

    assert report.results[0].is_passed is False
    assert report.results[0].failure_reason == "top_score_exceeded_low_confidence_limit"
    assert report.pass_rate == 0.0


def test_evaluation_exit_code_fails_when_pass_rate_is_below_threshold() -> None:
    runtime = FakeRagRuntime(
        FakeRagResult(
            sub_question_results=[
                FakeSubQuestionResult(FakeRetrieval([FakeSearchResult("doc:wrong", 0.70)])),
            ],
            metadata={"confidence_counts": {"high": 1, "medium": 0, "low": 0}},
        ),
    )
    report = evaluate_questions(
        [
            {
                "question": "제련기는 뭐야?",
                "expected_doc_id": "doc:expected",
                "expected_confidence": "high",
                "expected_behavior": "document_match",
            }
        ],
        runtime,
    )

    assert evaluation_exit_code(report, min_pass_rate=1.0) == 1
