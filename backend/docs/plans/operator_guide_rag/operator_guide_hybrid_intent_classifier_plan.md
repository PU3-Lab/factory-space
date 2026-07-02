# operator_guide Hybrid Intent Classifier 최종 기획

## 목적

operator_guide의 질문 의도 분류를 현재의 키워드 + CSV 매칭 기반 구조에서 하이브리드 구조로 확장한다.

하이브리드 구조의 목적은 빠르고 안정적인 규칙형 분류를 기본으로 유지하면서, 규칙형 분류가 놓치는 자연어 표현이나 애매한 대상만 LLM이 보정하게 만드는 것이다.

## 배경

현재 `question_classifier.py`는 플레이어 질문을 키워드와 CSV 매칭으로 분류한다.

이 방식은 빠르고 테스트하기 쉽지만, 아래와 같은 경우에는 오분류가 발생할 수 있다.

- 질문 표현이 키워드 목록에 없는 경우
- 하나의 표시명이 resource와 equipment 양쪽에 있는 경우
- `통신탑`처럼 제작 후 배치되는 설치물이 resource 테이블에도 존재하는 경우
- `지어야 해`, `건설 재료`, `조립 방법`처럼 제작 의도를 우회적으로 표현하는 경우

## 현재 구조

```text
Player Question
→ question_classifier.py
→ ManualQAIntent
→ CSV/RAG context builder
→ LLM final answer
```

## 목표 구조

```text
Player Question
→ Rule-based question_classifier.py
→ confident intent이면 그대로 진행
→ unknown 또는 ambiguous이면 LLMIntentClassifier 호출
→ 최종 ManualQAIntent 확정
→ CSV/RAG context builder
→ LLM final answer
```

## 핵심 정책

### 규칙형 분류 우선

자주 나오는 질문은 지금처럼 규칙형 분류기로 처리한다.

예:

```text
분쇄기가 뭐야?
철근은 어디에 써?
통신탑 어떻게 만들어?
제련기가 작동을 안 해.
```

### 애매할 때만 LLM 보정

LLM 의도 분류는 아래 조건에서만 사용한다.

- 기존 분류 결과가 `unknown_question`인 경우
- 같은 표시명이 equipment와 resource 양쪽에서 발견되는 경우
- 질문에는 대상명이 있지만 제작/역할/고장 의도가 명확하지 않은 경우
- CSV/RAG 후보는 있는데 confidence가 낮은 경우

### 실패 시 안전한 fallback

LLM 의도 분류가 실패해도 답변 흐름이 멈추면 안 된다.

실패 상황:

- LLM quota 부족
- provider timeout
- JSON 파싱 실패
- 허용되지 않은 intent 반환

이 경우 기존 규칙형 분류 결과 또는 `unknown_question` fallback을 사용한다.

## 통신탑 기준 예시

통신탑은 게임 데이터에서 두 의미를 가진다.

- 배치된 뒤에는 equipment
- 제작 레시피 결과물로는 resource

따라서 질문 표현에 따라 아래처럼 분류한다.

| 질문 | 기대 의도 |
| --- | --- |
| 통신탑이 뭐야? | equipment 또는 resource 설명 |
| 통신탑 어디에 써? | equipment 또는 resource 설명 |
| 통신탑 어떻게 만들어? | resource/recipe 질문 |
| 통신탑 어떻게 지어야 해? | resource/recipe 질문 |
| 통신탑 건설 재료 알려줘 | resource/recipe 질문 |
| 통신탑 고장났어 | troubleshooting 질문 |

## 완료 기준

- 기존 키워드 기반 분류 테스트가 유지된다.
- 통신탑 제작 질문 변형이 제작 레시피로 분류된다.
- ambiguous 후보 감지 테스트가 추가된다.
- LLM intent classifier는 mock/fake adapter로 테스트 가능하다.
- LLM 실패 시에도 기존 fallback이 동작한다.
- agent-test와 Unreal 요청 예시 문서에 의도 분류 흐름이 반영된다.

## Sprint 분리

- Sprint 17: 키워드 방어선 보강
- Sprint 18: 애매한 질문 감지
- Sprint 19: LLM 보조 의도 분류기
- Sprint 20: 회귀 테스트와 운영 문서
