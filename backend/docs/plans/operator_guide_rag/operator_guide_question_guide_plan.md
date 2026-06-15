# operator_guide 질문 가이드 계획

## 목표

operator_guide가 아무 질문이나 무제한 처리하는 챗봇처럼 보이지 않도록, 플레이어에게는 좋은 질문 예시를 제공하고 LLM에는 지원 범위와 재질문 기준을 제공한다.

질문 가이드는 질문을 막는 벽이 아니라, 플레이어를 좋은 질문으로 유도하는 안내판이다.

## 적용 범위

질문 가이드는 세 가지 용도로 사용한다.

```text
1. 게임 안 UI 도움말
2. 튜토리얼/초반 안내
3. 문서/포트폴리오용 agent policy 설명
```

## 톤

튜토리얼 NPC 톤과 실용적인 도움말 톤을 섞는다.

```text
막막할 땐 이렇게 물어봐줘.
장비, 자원, 제작법, 고장 원인을 중심으로 질문하면 내가 더 정확히 도와줄 수 있어.
```

너무 캐릭터 대사처럼 과하지 않게 유지하고, UI에 넣어도 어색하지 않은 짧은 문장을 사용한다.

## 질문 카테고리

### 1. 장비 설명

- 제련기는 뭐야?
- 컨베이어는 어디에 써?

### 2. 자원 설명

- 철광석은 어디에 써?
- 구리괴는 어떤 제작에 필요해?

### 3. 제작법/레시피

- 기어는 어떻게 만들어?
- 철괴를 만들려면 어떤 장비가 필요해?

### 4. 전력/물류/저장

- 전력이 부족하면 뭘 확인해야 해?
- 컨베이어가 막혔을 때는 어떻게 해?
- 저장고가 가득 차면 생산이 멈춰?

### 5. 현재 상태 문제 해결

- 지금 이 장비가 왜 작동 안 해?
- 철괴를 만들려는데 안 만들어져. 왜 그럴까?
- 재료가 있는데 생산이 안 돼.

### 6. 진행 방향/다음 목표

- 다음엔 뭘 만들어야 해?
- 지금 단계에서 어떤 장비를 먼저 설치해야 해?

## 범위 밖 질문 처리

범위 밖 질문은 딱 잘라 끝내지 않고, operator_guide가 도울 수 있는 범위를 안내하고 다시 물어볼 예시를 제공한다.

예시:

```text
나는 게임 안 공장 운영과 매뉴얼을 기준으로 도와줄 수 있어.
게임 기준으로 물어보면 더 정확히 안내할게.

예:
- 철괴는 어떻게 만들어?
- 제련기가 작동하지 않을 땐 뭘 확인해야 해?
```

## LLM용 Question Guide Policy

system prompt에는 플레이어용 문구가 아니라 정책 형태로 들어간다.

```text
Use the Question Guide to decide whether the player question is within operator_guide scope.
If the question is in scope, answer using the game manual, retrieved RAG context, and current game state when available.
If the question is ambiguous, ask a short clarifying question or suggest better question examples.
If the question is out of scope, briefly explain the supported scope and suggest 2-3 better question examples.
Do not answer questions outside the game manual, current game state, or supported factory-operation topics.
Do not invent mechanics, recipes, equipment, resources, or quest steps without evidence.
```

## 정책 강도

```text
기본 강도: 중간
- operator_guide 범위 안에서는 최대한 도와준다.
- 애매한 질문은 범위를 좁혀달라고 한다.

안전 규칙: 강함
- 매뉴얼/RAG/current game state 근거가 없으면 지어내지 않는다.
- 게임 외 질문, 개발팀 내부 정보, 치트/우회 요청은 답하지 않는다.
```

## Runtime 연결

```text
Player Question
-> Question Guide Policy
-> Orchestrator
-> operator_guide Agent
-> Leaf Agent
-> Context Need Classifier
-> RAG / Current Game State
-> Final Answer
```

## 완료 기준

- 플레이어용 질문 예시가 6개 카테고리로 정리된다.
- 범위 밖 질문 처리 방식이 문서화된다.
- LLM system prompt에 넣을 수 있는 Question Guide Policy가 정의된다.
- master plan과 sprint plan에 질문 가이드 흐름이 연결된다.

## 작업 로그

- 2026-06-10: 인터뷰 결정사항을 기준으로 operator_guide 질문 가이드 계획을 작성했다.

## 트러블슈팅 로그

- 2026-06-10: 질문 가이드는 플레이어 질문을 강제로 막는 구조가 아니라, 지원 범위 안내와 좋은 질문 예시 제공을 목적으로 한다.
