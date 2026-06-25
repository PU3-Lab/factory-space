# Process Optimizer v2 Sprint 6 계획

## 목표

플레이어가 되돌리기를 요청했을 때 실행 기록과 현재 상태를 비교해, 안전한 경우에만 inverse command를 만든다.

전체 공장 snapshot 복원이 아니라 변경 항목 단위 되돌리기를 사용한다.

## 구현 범위

```text
- undo 요청 schema 정의
- execution record 조회
- 현재 상태와 recorded after 비교
- undo conflict 판단
- inverse command 생성
- conflict 시 command 생성 차단
```

## 추가 또는 수정 파일

```text
backend/src/agents/process_optimizer/undo.py
backend/src/agents/process_optimizer/execution_record.py
backend/src/agents/process_optimizer/nodes.py
backend/src/agents/process_optimizer/schemas.py
backend/tests/test_process_optimizer_undo.py
```

## Undo Graph 흐름

```text
START
-> load_execution_record
-> validate_undo_request
-> compare_current_state_with_recorded_after_state
-> conflict_check
-> build_inverse_commands
-> return_undo_command_payload
-> END
```

## 충돌 기준

```text
현재 상태 == recorded after
-> inverse command 생성

현재 상태 != recorded after
-> undo_conflict 반환
-> command 생성 안 함
```

## 성공 기준

```text
- 기록이 없는 plan_id는 record_not_found를 반환한다.
- 현재 상태가 after와 같으면 before로 되돌리는 command를 만든다.
- 현재 상태가 after와 다르면 undo_conflict를 반환한다.
- conflict가 있으면 Unreal command를 만들지 않는다.
```

## 테스트 계획

```text
- undo 성공
- undo_conflict
- 없는 execution record
- 일부 change만 undo 가능
- conflict 시 command payload 없음
```

## 완료 후 확인 질문

```text
- 플레이어 직접 수정 이후 자동 되돌리기를 막는가?
- inverse command가 원래 command의 before 값을 사용하고 있는가?
- 강제 복구 없이 안전하게 실패하는가?
```

