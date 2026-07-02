# 코드 리뷰: operator_guide RAG Sprint 17 (제작 질문 키워드 방어선 보강)

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `feature/operator-guide-readable-answer-format` |
| 리뷰 일자 | 2026-07-02 |
| 리뷰 범위 | `question_classifier.py` 키워드 기반 분류기 보강 및 통신탑 제작/건설 질문 매칭 검증 |
| 리뷰어 | Antigravity |

## 1. 변경 요약

- **키워드 기반 분류기 사전 정의 키워드 보강**:
  - **[question_classifier.py](file:///c:/factory-space/backend/src/agents/operator_guide/question_classifier.py)**:
    - 제작/생산 관련 키워드 풀(`RESOURCE_PRODUCTION_KEYWORDS`)에 `지어`, `지으`, `짓`, `건설`, `건축`, `조립` 등의 건설 및 배치 관련 한국어 동사/명사 변형을 추가했습니다.
    - 레시피 질문용 키워드 풀(`RECIPE_KEYWORDS`)에는 `레시피`를 추가했습니다.
    - `만드는`, `만들기`는 제작/생산 표현이므로 `RESOURCE_PRODUCTION_KEYWORDS`에 포함하여 자원 생산 질문으로 처리되도록 했습니다.
- **통신탑(TeleCommunicationTower) 제작 질문 대응**:
  - 통신탑처럼 장비(`equipment_telecommunication_tower`)와 자원(`resource_TeleCommunicationTower`) 속성을 동시에 가진 설치물의 제작법 질문("어떻게 지어?", "건설 재료 알려줘")이 `unknown_question`으로 이탈하지 않고 `resource_question` 또는 `recipe_question` 의도로 정상 수렴하도록 규칙 흐름을 정돈했습니다.
- **스모크 테스트 보강 및 검증**:
  - **[test_manual_qa_agent_smoke.py](file:///c:/factory-space/backend/tests/test_manual_qa_agent_smoke.py)**:
    - "통신탑 어떻게 지어야 해?", "통신탑 건설 재료 알려줘", "통신탑 조립 방법 알려줘" 등 다양한 표현식을 담은 10개 이상의 파라미터화 테스트(parametrize)를 추가/유지하여 키워드 매칭 신뢰도를 검증했습니다.
    - 최종 생성된 답변 및 근거(source)에 합성기, 철근 20개, 구리선 20개, 주석판 20개 등 필수 레시피 정보가 올바르게 녹아드는지 검증 절차를 마쳤습니다.

---

## 2. 이슈 목록

심각도: 🔴 Blocker · 🟠 Major · 🟡 Minor · ⚪ Nit

### ⚪ N1. 키워드 매칭 방식의 한계 (Substring matching)
- **위치**: [question_classifier.py:129](file:///c:/factory-space/backend/src/agents/operator_guide/question_classifier.py#L129) (`_has_any` 함수)
- **내용**: 단순 부분 문자열 매칭(`keyword in question`) 방식을 사용하고 있어, `지어`라는 단순 키워드가 다른 단어의 일부로 포함될 때(예: "지어내다", "지어온" 등 의도치 않은 한국어 표현) 오감지할 소지가 미세하게 존재합니다.
- **영향**: 플레이어의 질문 형태에 따라 드물게 의도 오분류가 발생할 수 있습니다.
- **제안**: 향후 Sprint 19에 예정된 LLM 보조 분류기 도입 시 이 한계점이 근본적으로 해소되므로, 그 전까지는 본 키워드 리스트의 오분류 모니터링 수준으로 유지해도 무방합니다.

### ⚪ N2. 설치 비용 기반 장비(예: 자기장 차폐막)에 대한 건설 질문 지원 미비
- **위치**: [question_classifier.py:56](file:///c:/factory-space/backend/src/agents/operator_guide/question_classifier.py#L56)
- **내용**: `recipes.csv`에 제작 레시피가 따로 존재하지 않고 `equipment.csv` 상의 `build_cost_resources`로만 설치 비용이 정의되는 일반 장비(예: 자기장 차폐막)의 경우, "자기장 차폐막 어떻게 지어?"라는 건설 질문을 던졌을 때 레시피나 자원 매핑 정보가 없으므로 최종적으로 `unknown_question`으로 폴백됩니다.
- **영향**: 통신탑 이외의 일반 건설 설치물에 대한 자연어 조립/건설 정보 안내 퀄리티가 제한됩니다.
- **제안**: 장기적으로 `find_resource_by_question`이 실패하더라도 장비 정보가 검출되고 `건설` 키워드가 들어온 경우, 해당 장비의 `build_cost_resources`를 파싱하여 비용 정보를 반환해 줄 수 있는 별도 분류/응답 템플릿 로직 도입을 검토해 볼 수 있습니다.

---

## 3. 우선순위 권고

- **우선순위: Low (승인 가능)**
  - 이번 Sprint 17의 핵심 목표인 **"통신탑 건설 질문의 의도 정상 식별 및 제작법 정보 반환"**이 기획 문서에 지정된 요구 사항과 완벽히 일치하여 구현되어 있습니다.
  - LLM 보조 분류기로 나아가기 전의 1차 방어선 역할을 하기에 규칙 기반 구성이 탄탄하게 자리 잡았으므로 즉시 병합 및 배포가 가능합니다.

---

## 4. 긍정적인 부분

- **다양한 자연어 변형에 대한 꼼꼼한 테스트 구현**:
  - `어떻게 지어야 해?`, `어떻게 짓는 거야?`, `건설 방법`, `조립 방법`, `건설 재료` 등 실무에서 플레이어가 입력할 가능성이 매우 높은 10가지 이상의 한국어 구어체 변형을 파라미터화 테스트(`@pytest.mark.parametrize`)를 통해 촘촘하게 가둬둔 점이 매우 훌륭합니다.
- **영향 범위 제어**:
  - 분류 키워드를 확장했음에도 기존 장비 설명("제련기는 뭐야?"), 자원 설명, 트러블슈팅 질문이 기존 흐름을 침범하지 않고 100% 정상 작동함을 테스트 스위트로 입증했습니다.

---

## 5. 검증 결과

### 5.1. 자동화 테스트 결과
- `ruff` 정적 린트 및 포맷 정돈 상태 이상 없음 확인.
- 이번 스프린트에서 추가/유지된 통신탑 관련 스모크 테스트는 전체 정상 통과했습니다.
  - `pytest tests/test_manual_qa_agent_smoke.py` -> **24 passed**
- 한편, 전체 테스트 구동 시 기존 `main` 브랜치 기점에서 이미 실패하던 레거시 테스트 24건이 존재함을 확인했습니다. (메시지 스트리밍 텍스트 불일치, 프롬프트 문구 변경, 메모리 구조 변경 등으로 인한 베이스라인 이슈로, 본 PR의 변경 사항과는 무관합니다.)
