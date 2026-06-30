# Process Optimizer Power Sprint 4: Subquest Integration

## 1. 목표

Sprint 4의 목표는 전력망 최적화 제안을 자동 실행 명령으로 처리하지 않고, 플레이어가 직접 해결할 수 있는 서브퀘스트 후보로 연결하는 것이다.

이 Sprint까지 완료하면 전력망 최적화는 1차 완성으로 본다.

```text
전력망 문제 감지
-> optimization_alert 생성
-> suggested_subquest 생성
-> Unreal UI가 서브퀘스트 후보 표시
-> 플레이어가 직접 전선 또는 발전기 연결
-> 다음 state_update/analyze에서 해결 여부 재확인
```

## 2. 핵심 원칙

```text
- AI가 전선을 자동으로 연결하지 않는다.
- 백엔드는 connect_power_line 명령을 만들지 않는다.
- 플레이어 승인만으로 송전탑/발전기를 자동 연결하지 않는다.
- 서브퀘스트는 플레이어에게 문제 위치와 목표를 알려주는 제안이다.
```

## 3. 작업 범위

```text
- state_update에서 전력망 문제가 감지되면 optimization_alert 생성
- isolated_power_nodes를 suggested_subquest로 변환
- disconnected_generators를 suggested_subquest로 변환
- unpowered_machines를 objective 문구에 포함
- suggested_subquest.next_request에 request_source="subquest" 포함
- target에 power_pole / generator / machine 식별자 포함
- ui_hints.highlight_targets에 문제 송전탑, 발전기, 영향받는 설비 포함
- agent-test용 state_update -> subquest -> analyze 예시 추가
- subquest flow 테스트 추가
```

## 4. 서브퀘스트 응답 예시

```json
{
  "optimization_alert": {
    "needed": true,
    "severity": "medium",
    "reason": "pole_30이 주 전력망과 연결되어 있지 않아 smelter_1이 전력을 받지 못하고 있습니다.",
    "target": {
      "type": "power_pole",
      "id": "pole_30"
    },
    "suggested_subquest": {
      "title": "고립된 송전탑 확인",
      "objective": "pole_30 주변의 전력 연결을 확인하고 smelter_1에 전력이 공급되도록 직접 연결하세요.",
      "target": {
        "type": "power_pole",
        "id": "pole_30"
      },
      "severity": "medium",
      "next_request": {
        "agent": "process_optimizer",
        "operation": "analyze",
        "goal": "power_saving",
        "request_source": "subquest",
        "target": {
          "type": "power_pole",
          "id": "pole_30"
        }
      }
    }
  }
}
```

## 5. 완료 기준

```text
- 주기 state_update만으로 전력망 문제 alert가 생성된다.
- 고립 송전탑 문제가 서브퀘스트 제목과 목표로 변환된다.
- 미연결 발전기 문제가 서브퀘스트 제목과 목표로 변환된다.
- 전력 미공급 설비가 objective에 포함된다.
- 서브퀘스트에서 넘어온 analyze 요청이 target 중심으로 preview를 반환한다.
- Unreal UI가 target과 highlight_targets를 사용해 문제 위치를 표시할 수 있다.
- 전력망 문제는 자동 실행되지 않고, 플레이어가 직접 해결하는 제안형 최적화로 유지된다.
```

## 6. 예상 테스트

```text
- state_update에서 pole_30 고립 문제가 있으면 optimization_alert.needed=true를 반환한다.
- suggested_subquest.target.type이 power_pole인 응답이 생성된다.
- generator_5 미연결 문제가 있으면 generator 대상 subquest가 생성된다.
- suggested_subquest.next_request.request_source가 subquest로 설정된다.
- subquest analyze 요청이 target 중심 summary와 preview를 반환한다.
- commands에 자동 전력 연결 명령이 포함되지 않는다.
```

## 7. Sprint 4 이후 운영 흐름

```text
1. Unreal이 주기적으로 factory_state를 보낸다.
2. 백엔드가 전력망 그래프를 분석한다.
3. 고립 송전탑 또는 미연결 발전기가 있으면 optimization_alert를 반환한다.
4. Unreal이 이를 서브퀘스트 UI로 표시한다.
5. 플레이어가 직접 전선을 연결하거나 발전기를 송전탑에 연결한다.
6. 다음 state_update 또는 analyze에서 문제가 해결되었는지 다시 확인한다.
```

## 8. 이번 Sprint에 포함하지 않는 것

```text
- quest_generator의 보상/분류 로직 직접 재사용
- 자동 전선 연결
- 자동 발전기 연결
- 플레이어 승인만으로 전력망을 변경하는 기능
```

전력망 최적화는 Sprint 4에서 제안형 서브퀘스트 흐름으로 마무리한다.
