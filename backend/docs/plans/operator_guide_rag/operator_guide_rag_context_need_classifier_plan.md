# operator_guide RAG Context Need Classifier 계획

## 목표

operator_guide가 플레이어 질문을 받을 때, 현재 게임 상태가 필요한 질문인지 LLM이 먼저 판단하도록 문서 구조에 반영한다.

중요한 원칙은 rule-based keyword matching이 아니라 LLM-based structured decision이다.

## 문제 정의

모든 질문에 현재 게임 상태가 필요한 것은 아니다.

```text
기어는 어떻게 만들어?
→ 일반 제작법 질문
→ RAG 매뉴얼 근거만으로 답변 가능
→ currentGameState 조회 불필요
```

```text
철괴를 만들려는데 안 만들어져. 왜 안 만들어져?
→ 현재 생산 실패 원인 질문
→ 관련 장비, 입력 자원, 출력 공간, 전력, 레시피, 컨베이어 상태 확인 필요
→ currentGameState 조회 필요
```

## 결정 구조

operator_guide 내부에 `Context Need Classifier` 단계를 둔다.

```text
Player Question
-> Orchestrator
-> operator_guide Agent
-> Leaf Agent 선택
-> Context Need Classifier
-> requiresCurrentGameState 판단
-> 필요 시 Current Game State Tool 호출
-> RAG Retriever Tool
-> Source Formatter Tool
-> LLM final answer
```

## Classifier 출력

LLM 판단 결과는 자연어가 아니라 JSON으로 받는다.

```json
{
  "questionType": "production_troubleshooting",
  "requiresCurrentGameState": true,
  "requiredStateScopes": [
    "selectedMachine",
    "inputInventory",
    "outputInventory",
    "powerStatus",
    "currentRecipe",
    "connectedConveyors"
  ],
  "reason": "플레이어가 철괴 생산 실패 원인을 묻고 있으므로 현재 생산 장비와 자원 흐름 상태가 필요합니다."
}
```

## Current Game State Tool

`requiresCurrentGameState = true`인 경우에만 호출한다.

조회 후보:

- 현재 선택 장비
- 플레이어가 바라보는 설비
- 현재 열린 UI 패널
- 입력 inventory
- 출력 inventory
- 전력 연결/부족 여부
- 현재 recipe
- 연결된 conveyor
- 최근 오류 event
- 진행 중인 quest

## 완료 기준

- 문서에 rule-based가 아닌 LLM-based context need 판단 원칙이 들어간다.
- `Context Need Classifier`와 `Current Game State Tool`이 runtime flow에 포함된다.
- 최종 metadata에 `contextNeed`와 `currentStateUsed`가 포함된다.

## 작업 로그

- 2026-06-10: 사용자의 요구에 따라 LLM이 현재 게임 상태 필요 여부를 판단하는 구조를 계획했다.
- 2026-06-10: `operator_guide_rag_master_plan.md`와 `operator_guide_rag_sprint_plan.md`에 Context Need Classifier, Current Game State Tool, contextNeed/currentGameState metadata를 반영했다.
- 2026-06-10: 변경된 내용을 회색 code block PDF 스타일로 다시 렌더링했다.

## 트러블슈팅 로그

- 2026-06-10: keyword 기반 rule로 오해되지 않도록 `rule-based keyword matching이 아니다`라는 원칙을 명시한다.
