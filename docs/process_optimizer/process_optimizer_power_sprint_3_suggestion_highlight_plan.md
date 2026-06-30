# Process Optimizer Power Sprint 3: Suggestion Highlight

## 1. 목표

Sprint 3의 목표는 Sprint 2에서 계산한 전력망 분석 결과를 플레이어가 이해할 수 있는 preview 제안으로 바꾸는 것이다.

이 단계에서도 자동으로 전선을 연결하지 않는다. 백엔드는 문제 위치와 권장 확인 방향을 제안하고, Unreal은 해당 대상을 하이라이트할 수 있어야 한다.

## 2. 입력 기준

Sprint 3은 `FactoryAnalysisReport`에 아래 전력망 분석 결과가 들어온다는 전제에서 시작한다.

```text
- isolated_power_nodes
- disconnected_generators
- unpowered_machines
```

## 3. 제안 생성 기준

고립 송전탑이 있으면 송전탑 확인 제안을 만든다.

```json
{
  "id": "inspect_power_pole_30",
  "target": {
    "type": "power_pole",
    "id": "pole_30"
  },
  "problem": "pole_30이 주 전력망과 연결되어 있지 않습니다.",
  "recommended_action": "pole_30 주변의 송전탑 연결을 확인하고, 플레이어가 직접 전선을 연결하십시오.",
  "expected_effect": "pole_30에 연결된 설비가 전력을 공급받을 수 있습니다.",
  "risk": "medium"
}
```

미연결 발전기가 있으면 발전기 연결 확인 제안을 만든다.

```json
{
  "id": "inspect_generator_5_power_connection",
  "target": {
    "type": "generator",
    "id": "generator_5"
  },
  "problem": "generator_5가 전력망에 연결되어 있지 않습니다.",
  "recommended_action": "generator_5와 가까운 송전탑의 연결 상태를 확인하고, 플레이어가 직접 연결하십시오.",
  "expected_effect": "generator_5의 생산 전력이 전력망에 반영될 수 있습니다.",
  "risk": "medium"
}
```

전력 미공급 설비가 있으면 영향받는 설비를 설명에 포함한다.

## 4. 작업 범위

```text
- suggestion.py에서 isolated_power_nodes 개선안 생성
- suggestion.py에서 disconnected_generators 개선안 생성
- suggestion.py에서 unpowered_machines 안내 생성
- ui_hints.highlight_targets에 송전탑 ID 포함
- ui_hints.highlight_targets에 발전기 ID 포함
- ui_hints.highlight_targets에 영향받는 설비 ID 포함
- LLM 설명 prompt에 전력망 분석 결과 전달
- agent-test 예시 JSON과 예상 응답 추가
- preview 응답 테스트 추가
```

## 5. 완료 기준

```text
- 고립 송전탑 문제가 preview changes에 표시된다.
- 미연결 발전기 문제가 preview changes에 표시된다.
- 전력 미공급 설비가 설명 또는 expected_effect에 포함된다.
- Unreal UI가 문제 송전탑/발전기/설비를 highlight할 수 있다.
- GPT-5.4-nano는 계산 결과를 설명만 보강한다.
- GPT-5.4-nano가 임의의 전력 연결 명령을 만들지 않는다.
```

## 6. 예상 테스트

```text
- isolated_power_nodes=["pole_30"]이면 inspect_power_pole_30 제안이 생성된다.
- disconnected_generators=["generator_5"]이면 inspect_generator_5_power_connection 제안이 생성된다.
- unpowered_machines=["smelter_1"]이면 highlight_targets에 smelter_1이 포함된다.
- preview changes는 최대 3개 제한을 유지한다.
- commands에 connect_power_line 같은 자동 전력 연결 명령이 포함되지 않는다.
```

## 7. 이번 Sprint에 포함하지 않는 것

```text
- state_update optimization_alert 생성
- suggested_subquest 생성
- quest_generator 직접 연동
- 자동 전선 연결 명령 생성
```

Sprint 3은 preview와 highlight 품질을 담당한다.
