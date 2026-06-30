# Process Optimizer Power Sprint 5: Snapshot Store

## 1. 목적

Sprint 5의 목표는 Unreal이 주기적으로 보내는 `state_update`를 백엔드가 기억할 수 있게 만드는 것이다.

현재 `process_optimizer`는 요청에 포함된 최신 `factory_state`를 즉시 분석하는 구조다. Sprint 5에서는 session 단위의 최신 공장 snapshot을 저장해, 플레이어가 직접 분석 버튼을 누르지 않아도 최근 상태를 기준으로 이상 징후를 판단할 수 있게 한다.

```text
Unreal 주기 state_update
-> 백엔드가 session_id 기준 최신 factory_state 저장
-> factoryRevision, 수신 시각, source metadata 저장
-> 다음 analyze 또는 subquest 판단에서 최신 snapshot 참조
```

## 2. 범위

```text
- state_update 요청의 factory_state 저장
- session_id + client_id 기준 최신 snapshot 조회
- factoryRevision 저장
- updated_at 저장
- snapshot source 저장
- 기존 process_optimizer_memory와 역할 분리
- 테스트용 in-memory store 우선 구현
```

## 3. 저장할 정보

```json
{
  "session_id": "player-session-001",
  "client_id": "unreal-client",
  "factoryRevision": 42,
  "updated_at": "2026-06-30T12:00:00Z",
  "source": "state_update",
  "factory_state": {}
}
```

## 4. 설계 원칙

```text
- snapshot store는 공장 상태 기억만 담당한다.
- 최적화 판단은 analyzer.py가 담당한다.
- 오래된 snapshot은 최신 factoryRevision보다 우선하지 않는다.
- state_update는 공장을 변경하지 않는다.
- 저장 실패가 있어도 Unreal에 실행 명령을 만들지 않는다.
```

## 5. 완료 기준

```text
- state_update를 보내면 최신 snapshot이 저장된다.
- 같은 session_id의 새 factoryRevision이 들어오면 최신 snapshot으로 갱신된다.
- analyze 요청에 factory_state가 없고 저장된 snapshot이 있으면 최신 snapshot을 사용할 수 있다.
- 저장된 snapshot이 없으면 need_more_state 또는 invalid payload로 안전하게 안내한다.
- 기존 analyze/apply/undo/measure 흐름이 깨지지 않는다.
```

## 6. 테스트 시나리오

```text
1. state_update 저장
-> snapshot store에 factory_state와 factoryRevision이 저장된다.

2. 최신 revision 갱신
-> factoryRevision 42 이후 43이 들어오면 43이 최신 상태가 된다.

3. analyze에서 저장 snapshot 사용
-> payload에 factory_state가 없고 저장된 상태가 있으면 분석에 사용한다.

4. 저장 snapshot 없음
-> factory_state 없이 analyze하면 추가 상태 요청 또는 안전 오류를 반환한다.
```

## 7. 이번 Sprint에 포함하지 않는 것

```text
- 장기 히스토리 분석
- DB 영구 저장
- 플레이어별 장기 최적화 성향 학습
- 창고 재고 분석
- 내구도 분석
```

