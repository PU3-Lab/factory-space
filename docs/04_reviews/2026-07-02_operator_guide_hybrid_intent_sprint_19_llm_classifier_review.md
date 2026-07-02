# 코드 리뷰: operator_guide RAG Sprint 19 (LLM 보조 의도 분류기 계획)

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `feature/operator-guide-readable-answer-format` |
| 리뷰 일자 | 2026-07-02 |
| 리뷰 범위 | `question_classifier.py`, `service.py` 내 `LLMIntentClassifier` 설계 및 연동, 단위 테스트 작성 |
| 리뷰어 | Antigravity |

## 1. 변경 요약

- **LLM 보조 의도 분류기 컴포넌트 설계 및 연동**:
  - **[question_classifier.py](file:///c:/factory-space/backend/src/agents/operator_guide/question_classifier.py)**:
    - `LLMIntentClassifier` 클래스를 신설했습니다.
    - 규칙 기반 분류에서 모호함(`is_ambiguous=True`)이 감지된 경우에만 작동하도록 흐름을 제한했습니다.
    - 전체 CSV 데이터를 LLM에 전달하는 비효율을 방지하고, 룰 기반 결과에 잡힌 대상 매핑 후보군(`candidate_targets`: id, type, title)만 제한 전달하여 응답 품질과 토큰 소모를 극대화로 케어했습니다.
    - JSON 구조화 출력을 위한 프롬프트 바인딩 및 응답 백틱 정리(`_parse_json_response`) 로직을 탑재했습니다.
  - **[service.py](file:///c:/factory-space/backend/src/agents/operator_guide/service.py)**:
    - 서비스 클래스 생성 시 `llm_adapter`를 기반으로 `LLMIntentClassifier`를 초기화합니다.
    - `build_prompt_context()` 내에서 `intent.is_ambiguous`가 True일 때만 `classify_ambiguous()`를 호출하여 의도를 최종 보정합니다.

- **LLM 응답 유효성 검증 및 실패 처리**:
  - LLM이 반환한 데이터에 대해 다음과 같은 견고한 유효성 검사 장치를 마련하여 안전한 폴백을 보장합니다:
    1. **JSON 파싱 실패**: 빈 응답이거나 잘못된 형식이면 원래 규칙 분류 결과를 그대로 유지.
    2. **허용되지 않은 의도 유형**: `question_type`이 지정된 카테고리(`candidate_intents`)를 벗어나면 원래 결과 유지.
    3. **미존재 타겟 ID**: 반환된 `target_ids` 중 실제로 게임 CSV에 등록되지 않은 ID가 포함되거나 적절한 매칭이 없으면 원래 결과 유지.
    4. **호출 예외**: API 타임아웃, 쿼타 한도 초과 등 예외 발생 시 로그 출력 후 원래 규칙 분류 결과로 안전하게 백업.

- **LLM 보조 의도 분류 단위 테스트 구축**:
  - **[test_operator_guide_llm_classifier.py](file:///c:/factory-space/backend/tests/test_operator_guide_llm_classifier.py)** [NEW]:
    - 모의 아답터 `MockLLMAdapter`를 이용해 외부 네트워크 의존이 없는 격리 테스트 환경을 구성했습니다.
    - **5대 시나리오 정밀 검증**:
      1. **정상 보정 케이스**: 모호한 질문("통신탑 준비하려면 뭐가 필요해?")이 주어졌을 때, 올바른 JSON 응답을 통해 `resource_question` 및 통신탑 자원/레시피 타겟들로 정상 보정되는지 검증.
      2. **JSON 파싱 실패 폴백**: 파싱할 수 없는 에러 텍스트 반환 시 규칙 의도가 유지되는지 검증.
      3. **의도 유형 오출력 폴백**: 정의되지 않은 의도 반환 시 규칙 의도가 유지되는지 검증.
      4. **존재하지 않는 타겟 ID 폴백**: 쌩뚱맞은 ID 반환 시 규칙 의도가 유지되는지 검증.
      5. **호출 예외 폴백**: LLM 호출 중 예외 발생 시 안전하게 규칙 의도가 유지되는지 검증.

---

## 2. 이슈 목록

심각도: 🔴 Blocker · 🟠 Major · 🟡 Minor · ⚪ Nit

### ⚪ N1. Free-form JSON 응답 포맷 이탈 가능성
- **위치**: [question_classifier.py:371](file:///c:/factory-space/backend/src/agents/operator_guide/question_classifier.py#L371)
- **내용**: 현재 백틱 제거와 `json.loads` 수준의 정규화를 거쳐 안전하게 검증하고 있지만, LLM이 완전하지 않은 JSON 구조를 돌려주거나 스키마 형식이 뒤틀릴 가능성은 여전히 존재합니다.
- **영향**: JSON 형식이 완전하지 못할 경우 LLM 보정을 포기하고 규칙 기반 폴백을 수행하므로 비즈니스 로직 상의 큰 장애로 이어지지는 않습니다.
- **제안**: 장기적으로 스키마 준수를 강제할 수 있도록 LLM 호출 단계에서 Pydantic을 활용한 Structured Output 옵션을 명시적으로 적용해 주는 방안을 검토해 볼 수 있습니다.

---

## 3. 우선순위 권고

- **우선순위: Low (승인 및 즉시 병합 가능)**
  - Sprint 19의 요구사양인 **"규칙형에서 감지한 ambiguous/unknown 질문을 Mock LLM adapter를 통해 성공적으로 보정하고, 비정상 상황 시 룰 기반 fallback을 안전히 탑재하는 것"**이 빈틈없이 구현 및 연동 완료되었습니다.
  - 신설 테스트 5종 및 기존 스모크/애매성 감지 테스트 30종 전건이 로컬 환경에서 단 1건의 누락 없이 성공했습니다.

---

## 4. 긍정적인 부분

- **RAG 품질의 안정성**:
  - LLM이 기괴한 타겟 ID나 부적절한 의도를 임의 생성하여 가져올 경우 발생할 수 있는 RAG 문서 누락/오폭 문제를 사전 방어하여, RAG 파이프라인 전반의 응답 신뢰도를 유지했습니다.
- **격리된 로컬 테스트성**:
  - Fake/Mock 구조를 온전히 지키며 외부 연결 없이도 로컬에서 파이프라인의 보정 흐름을 테스트할 수 있어 개발 민첩성이 우수합니다.

---

## 5. 검증 결과

### 5.1. 자동화 테스트 결과
- `pytest tests/test_operator_guide_llm_classifier.py` -> **5 passed**
- `pytest tests/test_operator_guide_ambiguity_detection.py` -> **6 passed**
- `pytest tests/test_manual_qa_agent_smoke.py` -> **24 passed**
- 전체 ruff 린트/포맷 정돈 규격 이상 없음 확인.
