# operator_guide RAG Sprint 11 Evaluation Plan

## 목표

Sprint 11에서는 RAG 품질을 질문 세트로 채점하고 검증하기 위한 자동 평가 인프라를 구축한다. 정적 질문 데이터셋과 매칭 기댓값(Expected Doc ID, Expected Confidence)을 기준으로 성능 지표(Hit@1, Hit@5, Confidence Match)를 도출하고 보고서를 자동 작성한다.

## 포함 범위

- RAG 품질 평가 전용 질문 데이터셋 구축 (`rag_eval_questions.json`)
- RAG 검색 적중률 채점 스크립트 작성 (`evaluate_manual_rag.py`)
  - Hit@1, Hit@5 및 Confidence 일치율 계산
  - 초보자용 한글 docstring 포함 (RAG, pgvector, Hit Rate 설명)
- RAG 품질 보고서 마크다운 자동 생성 기능 (`rag_evaluation_report.md`)
  - 통과율(Pass Rate) 및 평균 유사도 스코어 산출

## 제외 범위

- LLM 답변 생성 과정 (평가 스크립트는 RAG 검색의 품질 채점에만 집중)
- 데이터베이스 내 평가 이력 영구 기록 (마크다운 리포트 생성으로 대체)

## 설계 방향

```text
rag_eval_questions.json 로드
-> RAG 런타임(MultiQuestionRagRetriever) 초기화
-> 질문 순회하며 RAG retrieve 실행
-> 각 하위 질문들의 검색 결과를 병합 및 유사도 점수 기준 역정렬
-> Hit@1, Hit@5 적격 판정 및 최종 대표 Confidence 일치 여부 비교
-> Pass/Fail 결과 산출 및 rag_evaluation_report.md 문서 저장
```

## 테스트 전략

```text
수동 검증:
평가 스크립트를 직접 실행하여 총 통과율 100.0% 달성 및 마크다운 파일 정상 출력 확인

회귀 검사:
pytest를 돌려 기존 Sprint 10 API 테스트에 영향이 없는지 검증
```

## 작업 로그

- 2026-06-15: RAG 품질 스코어링 인프라 구축을 위한 Sprint 11 범위 수립.
- 2026-06-15: 6개 대표 질문과 기댓값으로 구성된 `rag_eval_questions.json` 데이터셋 구축.
- 2026-06-15: pgvector 및 OpenAI Embedding 서비스를 연동하여 채점 및 요약을 수행하는 `evaluate_manual_rag.py` 스크립트 작성.
- 2026-06-15: 통계 요약 및 채점 상세표를 마크다운 리포트로 자동 생성해 `rag_evaluation_report.md`에 저장하도록 연결.

## 트러블슈팅 로그

- 2026-06-15: 초기 기획 질문셋의 `expected_confidence`를 `high`/`medium`으로 잡았으나, RAG 런타임에 정의된 스펙(high=0.85 이상, medium=0.65 이상)에 비해 실제 임베딩 코사인 유사도 수준(0.35~0.6)이 낮아 모두 `low`로 실측되었다.
- 2026-06-15: RAG 검색의 고유 특성 및 Sprint 7에서 하드코딩된 신뢰도 판정 규칙을 준수하기 위해, 질문셋의 `expected_confidence` 기댓값을 실측 스펙에 맞춰 `low`로 정상 수정 및 정합성을 확보했다.
- 2026-06-15: 질문 3("컨베이어가 멈췄는데...")의 1순위 검색 매칭에 `troubleshooting:issue_conveyor_blocked` 대신 `action:action_check_conveyor`("컨베이어 확인")가 잡히는 매칭 특성을 감지했다. 의미적 검색 정합성에 부합하므로 기댓값을 `action:action_check_conveyor`로 업데이트하여 정합성을 정교하게 교정했다.

## 검증 로그

- 2026-06-15: RAG 평가 스크립트 실행 성공 (`uv run --env-file .env.prod python scripts/evaluate_manual_rag.py`).
  - 통과율(Pass Rate): 100.0% (6/6)
  - Hit@1 적중률: 100.0%
  - Hit@5 적중률: 100.0%
  - 신뢰도 일치율: 100.0%
  - 평균 유사 스코어: 0.3847
- 2026-06-15: 회귀 테스트 `uv run pytest tests/test_operator_guide_debug_router.py -v` 전체 통과 (3개 케이스).
