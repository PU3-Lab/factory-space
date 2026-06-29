# Process Optimizer Subquest Sprint 2: Target Analyze

## 1. 목적

특정 기계나 컨베이어에서 플레이어가 “최적화 분석”을 요청했을 때, 해당 대상을 중심으로 preview가 나오도록 개선한다.

Sprint 1이 주기 상태 기반 alert를 만드는 단계라면, Sprint 2는 플레이어가 특정 대상에서 직접 최적화 요청을 눌렀을 때 응답 품질을 높이는 단계다.

## 2. 사용자 흐름

```text
플레이어가 smelter_1과 상호작용
-> "이 기계 최적화 분석" 버튼 클릭
-> Unreal이 target을 포함해 analyze 요청
-> process_optimizer가 전체 공장을 분석하되 smelter_1 관련 문제를 우선 표시
-> preview 응답 반환
-> Unreal이 smelter_1과 관련 연결을 highlight
```

## 3. 구현 범위

### 3.1 Target 입력 명시화

Sprint 1에서 추가한 `target` 필드를 `analyze` 흐름에서 실제로 사용한다.

권장 요청:

```json
{
  "operation": "analyze",
  "goal": "balance",
  "request_source": "machine_interaction",
  "target": {
    "type": "machine",
    "id": "smelter_1"
  },
  "factoryRevision": 12,
  "factory_state": {}
}
```

### 3.2 Suggestion 우선순위 조정

현재 suggestion은 분석 결과와 goal 기준으로 정렬된다.

Sprint 2에서는 target이 있으면 다음 규칙을 추가한다.

```text
1. target.id와 직접 일치하는 suggestion을 먼저 보여 준다.
2. target과 관련 없는 suggestion은 기존 goal 우선순위를 유지한다.
3. target 문제가 없으면 전체 공장 기준 suggestion을 반환하되, summary에 target 문제가 명확하지 않다고 안내한다.
```

구현 후보:

```text
Option A: OptimizationSuggestionTool.generate_suggestions(report, target=None)
Option B: nodes.py에서 suggestions 생성 후 target 관련 항목을 앞으로 재정렬
```

권장: Option B
이유는 기존 suggestion tool의 책임을 크게 바꾸지 않고, graph node에서 요청 맥락만 반영할 수 있기 때문이다.

### 3.3 UI hint 보강

target이 있는 analyze 요청에서는 `ui_hints.highlight_targets`에 target id를 포함한다.

```text
target.id가 이미 highlight_targets에 있음
-> 그대로 유지

target.id가 없음
-> highlight_targets 앞쪽에 추가
```

이렇게 하면 target에 직접적인 병목이 없더라도 Unreal UI에서 플레이어가 요청한 대상이 표시된다.

### 3.4 응답 문구 보강

LLM 설명 또는 fallback summary에서 target 요청임을 자연스럽게 드러낸다.

예시:

```text
smelter_1을 기준으로 확인한 결과, 입력 재고 부족이 가장 먼저 해결할 문제입니다.
```

또는 target 문제가 없을 때:

```text
smelter_1 자체의 직접 병목은 크지 않지만, 연결된 출력 라인에서 혼잡이 감지되었습니다.
```

## 4. 제외 범위

이번 Sprint에 포함하지 않는다.

```text
- state_update alert 생성 로직 변경
- quest_generator 연동
- Unreal 실제 UI 구현
- command schema 변경
- 자동 apply
```

## 5. 테스트 계획

추가 또는 보강할 테스트:

```text
backend/tests/test_process_optimizer_target_analyze.py
```

테스트 케이스:

| 테스트 | 기대 결과 |
| --- | --- |
| target 장비에 입력 부족 있음 | 해당 target suggestion이 첫 번째 |
| target 장비에 출력 적체 있음 | 해당 target suggestion이 첫 번째 |
| target과 무관한 다른 장비 문제 있음 | 기존 전체 공장 suggestion 반환 |
| target이 highlight_targets에 없음 | 응답에 target id 포함 |
| target이 잘못된 구조 | payload validation error 또는 안전한 fallback |

## 6. 완료 기준

```text
- 특정 기계 analyze 요청에서 target 관련 문제가 우선 노출된다.
- target id가 ui_hints.highlight_targets에 포함된다.
- target이 없는 기존 analyze 요청 동작은 유지된다.
- preview는 여전히 command를 만들지 않는다.
- 승인 후 apply 흐름은 기존과 동일하다.
```

## 7. Unreal 공유 포인트

Sprint 2 완료 후 Unreal에 공유할 내용:

```text
- 특정 기계 UI에서 analyze 요청을 보낼 때 payload.target을 포함한다.
- target 중심 요청이어도 factory_state는 최신 상태를 함께 보내는 것이 좋다.
- 응답의 ui_hints.highlight_targets를 월드 highlight에 사용한다.
- preview를 보여 준 뒤 플레이어 승인 전까지 공장 상태를 변경하지 않는다.
```
