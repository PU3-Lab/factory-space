# Review Feedback - 2026-06-17

## Quest Agent Phase 3 - RuleGenerator (collect_item)

### Review 1

- Reviewer: Code Quality Reviewer (`reviewer`)
- Scope:
  - [rule_generator.py](file:///Users/kimkyungpyo/Workspaces/projests/factory-space/backend/src/agents/quest_generator/rule_generator.py)
  - [test_quest_rule_generator.py](file:///Users/kimkyungpyo/Workspaces/projests/factory-space/backend/tests/test_quest_rule_generator.py)
- Status: No unresolved findings

Findings:

- **[Major] 메인 퀘스트 목표(Objective) 불일치 예외 상황에서의 조기 차단 버그 [✅ 해소]**:
  - `context.known_issues`를 순회하면서 진행 중이 아닌 첫 번째 부족 자원을 먼저 찾고, 루프 밖에서 메인 퀘스트 목표를 찾다 매칭되는 목표가 없으면 즉시 `None`을 반환하는 구조. 이로 인해 뒤쪽에 매칭 가능한 정상 자원이 있더라도 차단됨.
  - **조치**: 부족 자원 탐색 루프 내에서 메인 퀘스트 목표와의 매칭을 함께 검증하도록 수정하여 조기 차단 오류를 해결했습니다.
- **[Minor] Validator의 선행조건 검증(Skip)을 고려하지 않은 단일 초안 리턴 설계 [✅ 해소]**:
  - Validator가 초안 검증 탈락 시 차선책(2순위, 3순위 자원)이 있음에도 퀘스트 생성 로직이 전부 무산될 수 있음.
  - **조치**: 단일 초안 리턴(`generate_draft`) 대신 우선순위 순으로 정렬된 후보 초안 리스트(`generate_drafts`)를 반환하도록 구조를 변경하여 Validator skip 처리에 대비했습니다.
- **[Minor] Edge Case 테스트 커버리지 누락 [✅ 해소]**:
  - 메인 퀘스트 목표 불일치 등의 예외 상황을 검증하는 테스트 케이스 누락.
  - **조치**: `test_generate_drafts_objective_mismatch_recovery` 테스트를 추가하여 예외 상황 및 다음 우선순위 자원으로의 복구 로직을 철저히 검증했습니다.
