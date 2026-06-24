# Process Optimizer v2 Sprint 5 계획

## 목표

승인된 변경 항목만 Unreal command payload로 변환하고, 실행 기록을 저장할 준비를 한다.

LLM이 임의 명령을 만들지 못하도록 명령 schema와 허용 목록은 코드로 고정한다.

## 구현 범위

```text
- 허용 명령 enum/schema 정의
- 승인된 change를 command payload로 변환
- command whitelist 검증
- execution record schema 정의
- 실행 전 record 생성
- execution result 수신 schema 정의
```

## 추가 또는 수정 파일

```text
backend/src/agents/process_optimizer/commands.py
backend/src/agents/process_optimizer/execution_record.py
backend/src/agents/process_optimizer/schemas.py
backend/src/agents/process_optimizer/nodes.py
backend/tests/test_process_optimizer_commands.py
backend/tests/test_process_optimizer_execution_record.py
```

## 허용 명령

```text
- set_recipe
- set_machine_enabled
- connect_conveyor
- disconnect_conveyor
- move_machine
- place_machine
- remove_machine
```

## Graph 흐름

```text
validate_selected_changes
-> build_unreal_commands
-> validate_command_payloads
-> create_execution_record
-> return_command_payload
```

## 성공 기준

```text
- 허용 목록 밖의 command는 생성되지 않는다.
- command payload는 schema 검증을 통과한다.
- record에는 plan_id, change_id, before, after, revision이 저장된다.
- 같은 change_id를 중복 실행하지 않도록 record 조회 기준이 마련된다.
```

## 테스트 계획

```text
- 허용 command 생성 성공
- 허용되지 않은 command 차단
- command schema 누락 필드 차단
- execution record 생성
- 중복 change_id 감지
```

## 완료 후 확인 질문

```text
- LLM 출력 없이도 command 생성이 가능한가?
- 명령 생성과 실행 기록이 분리되어 있는가?
- Unreal이 바로 소비할 수 있는 payload 형태인가?
```

