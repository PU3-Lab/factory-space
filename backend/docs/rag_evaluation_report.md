# RAG 검색 품질 평가 보고서 (RAG Evaluation Report)

* **평가 일시**: 2026-06-15 19:01:10

## 품질 지표 요약 (Summary)

| 지표명 | 수치 | 설명 |
| --- | --- | --- |
| **총 평가 케이스 수** | 6 | 테스트 질문의 총합 |
| **최종 통과율 (Pass Rate)** | 100.0% (6/6) | 케이스별 기대 행동 충족률 |
| **Hit@1 적중률 (Hit@1 Rate)** | 100.0% (4/4) | 1순위 검색 결과 일치도 |
| **Hit@5 적중률 (Hit@5 Rate)** | 100.0% (4/4) | 상위 5개 문서 내 정답 포함율 |
| **신뢰도 매칭율 (Confidence Match)** | 100.0% (6/6) | RAG 판정 신뢰도와 기댓값의 일치도 |
| **평균 최상위 유사도 점수 (Avg Top Score)** | 0.3847 | 매칭된 최상위 문서들의 평균 Cosine 유사도 점수 |

## 질문 케이스별 채점 상세 (Detailed Results)

| 번호 | 질문 | 기대 행동 | 기대 문서 ID | 실측 탑 문서 ID | 최고 점수 | 기대 신뢰도 | 실측 신뢰도 | Hit@1 | Hit@5 | 신뢰도 일치 | 최종 판정 | 실패 이유 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 제련기는 뭐야? | `document_match` | `equipment:equipment_smelter` | `equipment:equipment_smelter` | 0.4211 | `low` | `low` | O | O | O | **PASS** | `-` |
| 2 | 철괴는 어떻게 만들어? | `document_match` | `resource:resource_iron_ingot` | `resource:resource_iron_ingot` | 0.4076 | `low` | `low` | O | O | O | **PASS** | `-` |
| 3 | 컨베이어가 멈췄는데 뭘 확인해야 해? | `document_match` | `action:action_check_conveyor` | `action:action_check_conveyor` | 0.5989 | `low` | `low` | O | O | O | **PASS** | `-` |
| 4 | 생산이 느린데 왜 그래? | `document_match` | `troubleshooting:issue_production_bottleneck` | `troubleshooting:issue_production_bottleneck` | 0.3521 | `low` | `low` | O | O | O | **PASS** | `-` |
| 5 | 라인이 이상해 | `ambiguous_low_confidence` | `N/A` | `action:action_check_downstream_route` | 0.2546 | `low` | `low` | - | - | O | **PASS** | `-` |
| 6 | 우주 엘리베이터 업그레이드는 어떻게 해? | `out_of_scope_low_confidence` | `N/A` | `action:action_check_build_cost` | 0.2739 | `low` | `low` | - | - | O | **PASS** | `-` |
