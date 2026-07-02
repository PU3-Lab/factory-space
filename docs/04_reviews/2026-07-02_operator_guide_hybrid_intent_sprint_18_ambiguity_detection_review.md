# 코드 리뷰: operator_guide RAG Sprint 18 (애매한 질문 감지 계획)

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `feature/operator-guide-readable-answer-format` |
| 리뷰 일자 | 2026-07-02 |
| 리뷰 범위 | `schemas.py`, `manual_context_builder.py`, `question_classifier.py`, `service.py` 내 애매성(ambiguity) 식별 로직 구현 및 단위 테스트 작성 |
| 리뷰어 | Antigravity |

## 1. 변경 요약

- **애매성 메타데이터 스키마 확장**:
  - **[schemas.py](file:///c:/factory-space/backend/src/agents/operator_guide/schemas.py)**:
    - 의도 분류 결과 모델인 `ManualQAIntent` 및 최종 응답 결과인 `ManualQAResult`에 `is_ambiguous: bool = False` 플래그 필드를 신설하였습니다.
    - 클라이언트 연동용 `to_metadata()` 딕셔너리에 `"isAmbiguous"` 카멜케이스 키를 포함하여 애매성 여부를 전달할 수 있도록 확장하였습니다.
  - **[manual_context_builder.py](file:///c:/factory-space/backend/src/agents/operator_guide/manual_context_builder.py)**:
    - `ManualQAContextBuilder.build()` 내부에서 `ManualQAResult` 인스턴스 생성 시 `intent.is_ambiguous` 값을 정확히 맵핑하도록 수정했습니다.

- **규칙형 분류기 애매성(ambiguity) 감지 엔진 구현**:
  - **[question_classifier.py](file:///c:/factory-space/backend/src/agents/operator_guide/question_classifier.py)**:
    - `classify()` 메서드에 실시간 게임 상태 참조를 위한 `context` 선택형 인자를 추가하였습니다.
    - 아래 **3대 애매함 판별 기준**을 규칙화하여 감지하도록 설계했습니다:
      1. **장비/자원 표시명 중복 (Double Presence)**: 동일 표시명이 `equipment`와 `resource` 테이블에 모두 존재하며, 명확한 의도 지시 키워드(예: "만들어", "짓", "뭐야" 등)가 누락된 경우 (예: "통신탑은 어떻게 써?", "통신탑 준비하려면 뭐가 필요해?").
      2. **의도 누락 (Unknown Question with Target)**: 대상 기기나 자원명은 식별되었으나 질문 내에 의도를 파악할 키워드가 없어 `unknown_question`으로 분류된 경우 (예: "제련기").
      3. **컨텍스트 의존 (Ambiguous Context Reference)**: "이거 설치하려면 뭐 해?"와 같이 질문 자체에는 지시 대상이 없으나, `context`를 통해 현재 선택된 장비(`selectedMachine`)를 특정할 수 있고 의도가 모호하여 `unknown_question`으로 떨어진 경우.
    - 애매한 질문 상황에서 `unknown_question`으로 분류되었더라도 대상 ID들을 `target_ids`에 보존하여 이후 RAG 파이프라인에서 적합한 후보 근거를 모두 가져올 수 있게 설계했습니다.

- **파이프라인 흐름 연동**:
  - **[service.py](file:///c:/factory-space/backend/src/agents/operator_guide/service.py)**:
    - 서비스 계층의 `build_prompt_context()` 에서 분류기를 기동할 때 `context`를 정상적으로 인자로 넘겨주도록 호출부를 연동하였습니다.

- **애매성 감지 단위 테스트 구축**:
  - **[test_operator_guide_ambiguity_detection.py](file:///c:/factory-space/backend/tests/test_operator_guide_ambiguity_detection.py)** [NEW]:
    - 명확한 질문의 비-애매성 통과 확인 ("통신탑 어떻게 만들어?").
    - 중복 표시명 기기의 모호한 지문 감지 확인 ("통신탑 알려줘", "어떻게 써?", "준비하려면 뭐가 필요해?").
    - 대상 기기명만 입력 시 의도 유추 불가 상태의 애매성 판정 확인 ("제련기").
    - context 내 `selectedMachine` 유무에 따른 모호 질문 처리("이거 설치하려면 뭐 해야 해?")의 동적 변동성 등을 정밀 검증하는 6대 시나리오를 구성했습니다.

---

## 2. 이슈 목록

심각도: 🔴 Blocker · 🟠 Major · 🟡 Minor · ⚪ Nit

### ⚪ N1. 중복 표시명 타겟에서 '필요' 키워드의 모호성 처리
- **위치**: [question_classifier.py:155](file:///c:/factory-space/backend/src/agents/operator_guide/question_classifier.py#L155)
- **내용**: "통신탑 준비하려면 뭐가 필요해?" 질문의 경우 `필요`가 `RECIPE_KEYWORDS`에 포함되지만, "준비하려면"과 같이 사용되는 경우 장비의 설치 요건인지 자원 생산 레시피인지 규칙만으로는 확답하기 어렵습니다.
- **영향**: 현재 구현에서는 `is_ambiguous = True`로 올바르게 감지하도록 `specific_keywords` 예외 리스트(만들어, 지어 등)를 선별하여 예외 처리하였으나, 규칙의 세밀한 경계에 따라 의도와는 다르게 애매함 여부가 빗나갈 여지가 있습니다.
- **제안**: Sprint 19에서 구축 예정인 LLM 보조 분류기가 도입되면 질문의 맥락을 완벽히 소화하여 이 경계를 다듬을 수 있으므로, 현재 단계의 규칙 처리는 요구 사양을 매우 효율적으로 충족하고 있습니다.

---

## 3. 우선순위 권고

- **우선순위: Low (승인 및 즉시 병합 가능)**
  - Sprint 18의 핵심 골자인 **"외부 LLM 호출 없이 기존 RAG 파이프라인의 호환성을 유지하면서, 규칙 분류가 모호한 대상 질문을 `isAmbiguous` 플래그로 감지해 내는 규칙 엔진"**이 사양대로 온전히 완수되었습니다.
  - 신설된 단위 테스트 및 기존 연동 스모크 테스트가 모두 성공하여 코드 안정성이 보장됩니다.

---

## 4. 긍정적인 부분

- **RAG 정보 회수율 극대화 설계**:
  - 분류기가 최종적으로 `unknown_question`으로 판단하더라도 애매한 대상(CSV 후보나 Context 상의 selectedMachine)의 ID를 버리지 않고 `target_ids`에 모두 적재해주어, RAG 검색 단계에서 두 후보(장비 정보, 레시피 정보 등)의 근거 문서를 누락 없이 확보하도록 배려한 유기적인 설계가 아주 돋보입니다.
- **기존 동작 영향 최소화**:
  - `is_ambiguous` 플래그만 응답에 메타데이터로 실어 보낼 뿐 기존의 분류 판정 우선순위와 Fallback 응답 로직 흐름을 해치지 않아 안전합니다.

---

## 5. 검증 결과

### 5.1. 자동화 테스트 결과
신설된 애매성 테스트 및 키워드 방어선 스모크 테스트가 전건 성공했습니다.
- `pytest tests/test_operator_guide_ambiguity_detection.py` -> **6 passed**
- `pytest tests/test_manual_qa_agent_smoke.py` -> **24 passed**
- `ruff` 정적 린트/포맷 룰 충족 상태 확인 완료.
