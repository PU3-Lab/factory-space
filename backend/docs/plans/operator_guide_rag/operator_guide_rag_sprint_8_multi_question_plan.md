# operator_guide RAG Sprint 8 Multi-question Plan

## 목표

플레이어가 한 번에 여러 질문을 입력해도 operator_guide가 질문을 안전하게 나눠서 처리할 수 있게 한다.

예시:

```text
분쇄기가 뭐야? 그리고 철괴를 만들려면 어떻게 해야 돼?
```

이 입력은 실제로 두 질문이다.

```text
1. 분쇄기가 뭐야?
2. 철괴를 만들려면 어떻게 해야 돼?
```

Sprint 8에서는 전체 runtime 연결 전에 질문 분해 단계를 먼저 만든다. 이렇게 하면 각 sub-question을 별도로 RAG 검색하고, 이후 LLM 답변도 번호를 나눠 구성할 수 있다.

## 포함 범위

- `Question Decomposer` 모듈 추가
- 한 입력에서 최대 3개의 sub-question 추출
- `?`, `그리고`, `또`, `그다음`, `하고` 같은 자연스러운 연결 신호 처리
- 질문이 하나면 그대로 하나의 sub-question으로 반환
- 분해된 질문 metadata 제공
- 초보자용 한글 docstring 추가

## 제외 범위

- LLM이 직접 질문을 분해하는 구조
- 각 sub-question별 RAG 검색 실행
- 최종 답변을 섹션별로 합치는 LLM prompt 연결
- Unreal 최종 응답 JSON schema 확장

위 항목은 Sprint 8 runtime integration 후속 작업에서 연결한다.

## 설계 방향

```text
player question
-> Question Decomposer
   - original_question
   - is_multi_question
   - sub_questions[]
-> 각 sub-question을 RAG Retriever Tool에 전달
-> Source Formatter Tool
-> LLM Answer Generator
```

## 응답 설계 예시

```json
{
  "original_question": "분쇄기가 뭐야? 그리고 철괴를 만들려면 어떻게 해야 돼?",
  "is_multi_question": true,
  "sub_questions": [
    {
      "index": 1,
      "question": "분쇄기가 뭐야?"
    },
    {
      "index": 2,
      "question": "철괴를 만들려면 어떻게 해야 돼?"
    }
  ],
  "metadata": {
    "sub_question_count": 2,
    "max_sub_questions": 3
  }
}
```

## 검증 계획

- 테스트를 먼저 작성한다.
- `? 그리고` 형태의 질문이 2개로 분해되는지 확인한다.
- 질문이 하나면 그대로 반환되는지 확인한다.
- 3개를 초과하는 질문은 최대 3개까지만 반환되는지 확인한다.
- 관련 테스트와 ruff를 실행한다.

## 작업 로그

- 2026-06-15: Sprint 8 runtime integration 전에 multi-question handling을 별도 작은 단위로 분리하기로 했다.
- 2026-06-15: `question_decomposer.py`를 추가해 한 입력을 최대 3개의 sub-question으로 분해할 수 있게 했다.
- 2026-06-15: 단일 질문, 한국어 연결어 포함 질문, 최대 3개 제한 케이스를 테스트로 검증했다.

## 트러블슈팅 로그

- 2026-06-15: 여러 질문을 하나의 embedding query로만 검색하면 top-k에서 한쪽 근거가 누락될 수 있어, 질문 분해 후 각 질문별 검색이 가능하도록 설계했다.
- 2026-06-15: 이 단계에서는 답변을 생성하거나 RAG를 직접 호출하지 않고, runtime integration에서 재사용 가능한 전처리 모듈로 범위를 제한했다.
