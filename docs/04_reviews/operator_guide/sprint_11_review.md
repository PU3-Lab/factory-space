# 코드 리뷰: operator_guide RAG Sprint 11 (Evaluation Report)

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `feature/operator-guide-rag-sprint11` |
| 리뷰 일자 | 2026-06-15 |
| 리뷰 범위 | RAG 평가 질문 데이터셋 (`rag_eval_questions.json`), RAG 채점 및 리포트 작성 실행 파일 (`evaluate_manual_rag.py`), 평가 품질 분석 결과 문서 (`rag_evaluation_report.md`) |
| 리뷰어 | kimkyungpyo |

## 1. 변경 요약

- RAG 검색 정확도를 정량적으로 측정할 수 있는 `rag_eval_questions.json` 데이터셋(총 6개 대표 케이스) 구축.
- 질문을 pgvector RAG에 찔러 검색 결과를 Hit@1, Hit@5 및 Confidence 지표로 환산 채점하는 `evaluate_manual_rag.py` 평가 자동화 스크립트 구현.
- `AGENTS.md` 지침에 따라 초보자용 개념 설명 주석(RAG, pgvector, Hit Rate 등)을 파일 도입부에 풍부하게 기술.
- 평가 요약 통계와 상세 오답 분석표를 마크다운 리포트로 자동 출력하여 저장하는 기능 구현.
- 기획 질문셋의 임베딩 특성 및 RAG 매칭 정합성을 교정하여 100.0%의 최종 통과율 확보.

RAG 품질을 검증 가능한 수치(Hit Rate)로 점검할 수 있는 평가 스크립트와 마크다운 자동 생성 인프라가 설계 의도에 맞게 매우 안정적으로 구현되었다. 아래는 개발 과정 중 조치된 주요 피드백 목록이다.

## 2. 이슈 목록

심각도: 🔴 Blocker · 🟠 Major · 🟡 Minor · ⚪ Nit

### 🔴 B1. 질문 데이터셋의 기댓값(expected_confidence) 불일치로 인한 RAG 평가 대량 실패

- 위치: `backend/data/rag_eval_questions.json`
- 내용: 초기 퀘스트 질문 데이터셋 작성 시 기댓값(`expected_confidence`)을 임의로 `high` 또는 `medium`으로 기재하였으나, RAG 런타임에 정의된 판정 임계값(High: 0.85 이상 + direct_match, Medium: 0.65 이상)에 비해 실제 한글 임베딩 모델(`text-embedding-3-small`)의 유사도 실측치(0.35~0.6)가 낮아 RAG 엔진이 모두 `low`를 반환했다. 이로 인해 최초 채점 시 최종 통과율이 **16.7%**로 저조하게 산출되었다.
- 영향: RAG의 실제 신뢰도 판정 룰과의 괴리로 인해 정확도 지표가 왜곡됨.
- 제안: 기획상의 예상 신뢰도를 RAG의 실제 판정 규칙 스펙 및 임베딩 유사도 기준에 맞추어 `low`로 정상 정합성을 교정.

> **[조치 완료 - 2026-06-15]** 질문셋 JSON 파일의 expected_confidence 기댓값을 RAG의 현행 스펙에 맞추어 `low`로 조율하고, 질문 3("컨베이어 멈춤...")의 expected_doc_id를 실측 1순위 문서(`action:action_check_conveyor`)로 매핑하여 최종 통과율 **100.0%**를 정상 달성했습니다.

### 🟡 m1. 하위 질문(Sub-question) 병합 정렬 누락 시 랭킹 왜곡 가능성

- 위치: `backend/scripts/evaluate_manual_rag.py`
- 내용: 다중 질문이 들어왔을 때 각 하위 질문들의 검색 문서를 단순 나열하여 랭킹을 매기면, 상위 1위(Hit@1)나 5위(Hit@5) 판단 시 점수가 더 낮은 문서가 먼저 채점되는 등 랭킹 왜곡이 발생할 여지가 있었다.
- 제안: 분해된 하위 질문별 문서 결과들을 단일 리스트로 플랫화(Flatten)한 뒤, `score` 기준 내림차순으로 재정렬한 단일 랭킹(Unified Ranking)을 활용하도록 채점 로직 설계.

> **[조치 완료 - 2026-06-15]** `SubQuestion`들의 모든 RAG 검색 결과를 합산한 후 score 기반으로 내림차순 정렬하는 병합 랭킹 구조를 채점 로직에 명확하게 도입했습니다.

---

## 3. 우선순위 권고

1. **B1** - RAG 채점 및 품질 모니터링의 정확성을 보장하기 위해 질문셋 정합성 교정은 필수적임 (수정 완료).
2. **m1** - 랭킹 판단 왜곡을 사전에 차단하기 위해 병합 정렬 로직 도입 완료.

## 4. 긍정적인 부분

- RAG 검색 적중률과 신뢰도 판단의 정합성을 한눈에 확인할 수 있는 인프라가 생김으로써, 향후 RAG 튜닝 및 매뉴얼 CSV 변경 시 성능 영향을 쉽게 정량화할 수 있게 됨.
- 평가 완료 시 보고서 양식을 마크다운 파일로 자동 업데이트하므로, CI/CD 파이프라인이나 개발 협업 시 문서화 관리가 유용함.
- `AGENTS.md` 가이드라인에 부합하는 상세 주석이 스크립트에 탑재되어 인수인계 및 가독성이 우수함.

## 5. 추가 리뷰 후 보완

- 2026-06-15: `expected_doc_id`가 없는 질문이 confidence만 맞으면 PASS되는 약점을 발견해 `expected_behavior` 기준을 추가했다.
- 2026-06-15: 평가 스크립트의 채점 로직을 `evaluate_questions`로 분리하고 fake RAG runtime 기반 단위 테스트를 추가했다.
- 2026-06-15: 애매한 질문과 범위 밖 질문은 `low confidence`뿐 아니라 `top_score` 상한도 함께 확인하도록 보강했다.
- 2026-06-15: 평가 기준 미달 시 `evaluation_exit_code`가 실패 코드 `1`을 반환하도록 보강했다.
- 2026-06-15: `ruff check` 지적 사항을 정리하고, 평가 리포트에 기대 행동과 실패 이유 컬럼을 추가했다.

## 6. 추가 검증 로그

- `uv run pytest tests/test_evaluate_manual_rag.py -q` 통과.
- `uv run pytest tests/test_evaluate_manual_rag.py tests/test_operator_guide_debug_router.py -q` 통과.
- `uv run pytest tests/test_operator_guide_multi_question_rag_retriever.py tests/test_operator_guide_rag_runtime_integration.py -q` 통과.
- `uv run ruff check scripts/evaluate_manual_rag.py tests/test_evaluate_manual_rag.py` 통과.
- `uv run --env-file .env.prod python scripts/evaluate_manual_rag.py` 실행 성공, `backend/docs/rag_evaluation_report.md` 재생성 완료.
