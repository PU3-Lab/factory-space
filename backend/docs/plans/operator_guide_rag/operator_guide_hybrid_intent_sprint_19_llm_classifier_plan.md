# Sprint 19. LLM 보조 의도 분류기 계획

## 목표

Sprint 18에서 감지한 ambiguous 또는 unknown 질문에 한해서 LLM이 질문 의도를 보정하게 한다.

규칙형 분류는 기본 경로로 유지하고, LLM은 보조 판단자 역할만 한다.

## 포함 범위

- `LLMIntentClassifier` 또는 동등한 보조 분류 컴포넌트 설계
- LLM에게 전달할 후보 intent와 후보 target 제한
- JSON output contract 정의
- LLM 응답 validation
- 실패 시 규칙형 fallback 유지
- fake/mock LLM adapter 기반 테스트

## 제외 범위

- 모든 질문에 LLM 의도 분류 적용
- LLM에게 CSV 전체를 무제한 전달
- 최종 답변 생성 prompt 대규모 변경
- 외부 API가 필요한 테스트

## LLM 입력 개념

```json
{
  "question": "통신탑 어떻게 지어야 해?",
  "candidate_intents": [
    "equipment_question",
    "resource_question",
    "recipe_question",
    "troubleshooting_question",
    "unknown_question"
  ],
  "candidate_targets": [
    {
      "id": "resource_TeleCommunicationTower",
      "type": "resource",
      "title": "통신탑"
    },
    {
      "id": "recipe_make_telecommunication_tower",
      "type": "recipe",
      "title": "통신탑 제작 공정"
    },
    {
      "id": "equipment_telecommunication_tower",
      "type": "equipment",
      "title": "통신탑"
    }
  ]
}
```

## LLM 출력 개념

```json
{
  "question_type": "resource_question",
  "target_ids": [
    "resource_TeleCommunicationTower",
    "recipe_make_telecommunication_tower"
  ],
  "confidence": "high",
  "reason": "질문이 통신탑의 제작 방법을 묻고 있습니다."
}
```

## 실패 처리

아래 상황에서는 LLM 보정 결과를 사용하지 않는다.

- JSON 파싱 실패
- 허용되지 않은 `question_type`
- 존재하지 않는 `target_ids`
- provider timeout
- quota 부족
- 빈 응답

## 완료 기준

- ambiguous 질문에서만 LLM 보조 분류기가 호출된다.
- mock LLM 결과로 최종 intent가 보정된다.
- LLM 실패 시 기존 규칙형 결과나 안전한 fallback이 사용된다.
- 외부 네트워크 없이 테스트가 가능하다.
