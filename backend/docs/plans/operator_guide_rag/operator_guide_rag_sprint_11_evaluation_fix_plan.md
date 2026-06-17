# operator_guide RAG Sprint 11 평가 보강 계획

## 목표

Sprint 11에서 만든 RAG 평가 스크립트가 단순히 현재 결과를 기록하는 수준을 넘어, 검색 품질 회귀를 실제로 잡을 수 있게 보강한다.

## 포함 범위

- `expected_doc_id`가 없는 질문의 평가 기준을 명확히 한다.
- `expected_behavior` 필드를 추가해 애매한 질문과 범위 밖 질문을 구분한다.
- 평가 기준 미달 시 스크립트가 실패 exit code를 반환하게 한다.
- 실제 OpenAI/DB 없이 fake RAG runtime으로 채점 로직을 테스트한다.
- `evaluate_manual_rag.py`의 ruff 오류를 정리한다.

## 설계 방향

```text
rag_eval_questions.json
-> expected_doc_id / expected_confidence / expected_behavior 로드
-> RAG 검색 결과를 score 기준으로 병합 정렬
-> 문서 기대값이 있으면 Hit@5 + Confidence 기준으로 평가
-> 문서 기대값이 없으면 expected_behavior 기준으로 평가
-> pass_rate, hit@1, hit@5, confidence match 산출
-> 기준 미달이면 보고서를 생성한 뒤 exit code 1 반환
```

## expected_behavior 기준

```text
document_match
- 기대 문서가 있는 일반 질문
- expected_doc_id가 Hit@5 안에 있고 confidence가 일치해야 통과

ambiguous_low_confidence
- 질문이 너무 애매해서 확신하면 안 되는 질문
- confidence가 low이고 top_score가 낮은 범위에 있어야 통과

out_of_scope_low_confidence
- operator_guide 범위 밖 질문
- confidence가 low이고 top_score가 낮은 범위에 있어야 통과
```

## 테스트 전략

```text
개발 중:
- fake RAG 결과로 scoring 함수 단위 테스트

구현 완료 직후:
- evaluate_manual_rag.py ruff check
- 평가 스크립트 단위 테스트

커밋 전:
- Sprint 10 debug router 테스트와 Sprint 11 평가 테스트 함께 실행
```

## 작업 로그

- 2026-06-15: Sprint 11 리뷰에서 발견된 평가 기준 약점을 보완하기 위해 계획 문서를 추가했다.
- 2026-06-15: 평가 채점 로직을 `evaluate_questions` 함수로 분리하고 fake RAG runtime 기반 테스트를 추가했다.
- 2026-06-15: `expected_behavior` 기준을 데이터셋에 추가해 문서 매칭 질문, 애매한 질문, 범위 밖 질문을 구분했다.
- 2026-06-15: 평가 기준 미달 시 `evaluation_exit_code`가 실패 코드 1을 반환하도록 보강했다.
- 2026-06-15: 평가 리포트에 기대 행동과 실패 이유 컬럼을 추가했다.

## 트러블슈팅 로그

- 2026-06-15: `expected_doc_id`가 없는 질문이 confidence만 맞으면 PASS되는 문제가 있어, `expected_behavior` 기준으로 보강하기로 했다.
- 2026-06-15: 평가 실패에도 스크립트가 성공 exit code로 끝날 수 있어, 기준 미달 시 `1`을 반환하도록 보강하기로 했다.
- 2026-06-15: `ruff check`에서 `Any` 타입과 내부 import 정렬 문제가 발생해, RAG runtime/result 인터페이스를 `Protocol`로 분리하고 lint를 통과시켰다.

## 검증 로그

- 2026-06-15: `uv run pytest tests/test_evaluate_manual_rag.py -q` 통과.
- 2026-06-15: `uv run pytest tests/test_evaluate_manual_rag.py tests/test_operator_guide_debug_router.py -q` 통과.
- 2026-06-15: `uv run pytest tests/test_operator_guide_multi_question_rag_retriever.py tests/test_operator_guide_rag_runtime_integration.py -q` 통과.
- 2026-06-15: `uv run ruff check scripts/evaluate_manual_rag.py tests/test_evaluate_manual_rag.py` 통과.
- 2026-06-15: `uv run --env-file .env.prod python scripts/evaluate_manual_rag.py` 실행 성공, 리포트 재생성 완료.
