# Sprint 18. 애매한 질문 감지 계획

## 목표

규칙형 분류기가 바로 확정하기 어려운 질문을 감지한다.

이 Sprint에서는 LLM을 호출하지 않고, 어떤 질문이 LLM 보정 대상으로 넘어가야 하는지만 판단한다.

## 포함 범위

- 같은 표시명이 equipment와 resource 양쪽에 존재하는 대상 감지
- `unknown_question`이지만 CSV/RAG 후보가 존재하는 경우 감지
- ambiguous 여부를 내부 metadata 또는 분류 결과에 표시하는 방식 설계
- 통신탑처럼 제작 가능한 설치물의 의도 충돌 케이스 테스트

## 제외 범위

- LLM intent classifier 호출
- LLM prompt 작성
- 최종 답변 문구 변경
- Unreal UI 수정

## 애매한 질문 예시

```text
통신탑 알려줘
통신탑은 어떻게 써?
통신탑 준비하려면 뭐가 필요해?
이거 설치하려면 뭐 해야 해?
```

## 판단 기준

아래 중 하나라도 해당하면 ambiguous 후보로 본다.

- 동일 표시명이 equipment와 resource 양쪽에 있다.
- 질문에는 대상명이 있지만 제작/역할/고장 의도가 불명확하다.
- 규칙형 결과가 `unknown_question`인데 CSV 후보가 존재한다.
- candidate target이 2개 이상이고 우선순위가 확실하지 않다.

## 완료 기준

- ambiguous 후보를 테스트에서 확인할 수 있다.
- 명확한 질문은 ambiguous로 과도하게 표시되지 않는다.
- 아직 LLM을 호출하지 않아도 기존 답변 흐름은 유지된다.
