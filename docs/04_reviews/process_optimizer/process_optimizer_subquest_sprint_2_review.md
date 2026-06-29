# 코드 리뷰 보고서: Process Optimizer Subquest Sprint 2 (Target Analyze)

이 문서는 **Process Optimizer Subquest Sprint 2: Target Analyze** 구현 건에 대한 코드 리뷰 보고서입니다.

---

## 1. 변경 요약

- **타겟 최우선 정렬 및 강조**:
  - [nodes.py](file:///c:/factory-space/backend/src/agents/process_optimizer/nodes.py)의 `build_optimization_candidates` 함수를 수정하여, 요청 payload에 `target`이 명시된 경우 해당 target ID와 일치하는 최적화 제안(`OptimizationSuggestion`)을 제안 목록의 맨 앞으로 이동시킵니다.
  - 해당 target ID가 `ui_hints.highlight_targets` 목록에 없는 경우 맨 처음에 삽입하여 Unreal Engine UI에서 항상 우선적으로 하이라이트 되도록 보강했습니다.
- **동적 요약문(Summary) 생성**:
  - [nodes.py](file:///c:/factory-space/backend/src/agents/process_optimizer/nodes.py)의 `return_preview_plan` 함수에서 target 여부 및 target 병목 일치 여부에 따라 동적인 `summary_text`를 빌드합니다.
  - target에 직접적인 병목(입력 부족, 출력 적체, 컨베이어 혼잡 등)이 감지되면 해당 이슈명을 활용한 맞춤 요약문(`"smelter_1을 기준으로 확인한 결과, 입력 재고 부족이..."`)을 작성합니다.
  - target에 병목이 없는 경우, `"smelter_1 자체의 직접 병목은 크지 않지만, 전체 공장 기준의..."` 와 같이 자연스럽게 Fallback 안내를 포함합니다.
- **안정적 스키마 유효성 검사**:
  - `target` 필드가 비정상적인 형태(예: 잘못된 type)인 경우, Sprint 1에서 정의한 `TargetDescriptor` 스키마 제약조건에 의해 Pydantic 검증 단계에서 즉각 걸러지고 `INVALID_REQUEST_PAYLOAD` 오류 응답을 안전하게 반환합니다.
- **테스트 케이스 추가**:
  - `backend/tests/test_process_optimizer_target_analyze.py` 파일을 생성하고 5가지 시나리오(입력 부족 타겟, 출력 적체 타겟, 무관한 타겟, 하이라이트 추가, 잘못된 타겟 구조)에 대한 단위 및 통합 테스트를 작성해 전원 성공시켰습니다.

---

## 2. 이슈 목록

### 🟡 Minor: 제안 ID 기반의 이슈 유형 추출 분기
- **위치**: `backend/src/agents/process_optimizer/nodes.py:333-339`
- **내용**: `first_sug.id` 문자열에 특정 단어("input", "output", "conveyor")가 포함되어 있는지 확인하여 이슈 한글 설명(`issue_desc`)을 맵핑하는 규칙이 하드코딩되어 있습니다.
- **영향**: 나중에 다른 규칙을 사용하는 최적화 제안 ID 구조가 생성되거나 새로운 이슈 카테고리가 추가될 경우 요약 코멘트 빌더가 예기치 않게 "공정상 문제"로 Fallback될 수 있습니다.
- **제안**: 장기적으로 `OptimizationSuggestion` 스키마 자체에 `category: Literal["input_shortage", "output_blocked", "conveyor_congestion"]` 와 같은 정적 구분자 필드를 신설해 데이터 정합성을 확실히 지킬 수 있도록 고도화할 것을 권장합니다.

### ⚪ Nit: 기획 명세에 부합하는 UI 하이라이트 유지 로직
- **위치**: `backend/src/agents/process_optimizer/nodes.py:214`
- **내용**: target ID가 `highlight_targets`에 이미 존재하는 경우에는 순서를 건드리지 않고 그대로 유지하도록 구성되어 있습니다.
- **영향**: 만약 target ID가 다른 제안의 영향으로 하이라이트 리스트의 후순위에 존재하고 있었다면, 플레이어가 직접 클릭해서 분석한 대상임에도 최우선 강조 순서로 올라오지 않을 수 있습니다.
- **제안**: 기획서 3.3절의 사양("target.id가 이미 highlight_targets에 있음 -> 그대로 유지")을 충실히 반영했으나, 추후 플레이어 경험(UX) 테스트 시 시각적 반응성이 무디다고 판단되면 기존 항목을 제거 후 맨 앞으로 인출하도록 Unreal 개발팀과 협의하여 조정 가능합니다.

---

## 3. 우선순위 권고

- **즉시 조치 필요 없음**: 현재 구현은 Sprint 2 완료 기준을 완벽하게 만족하며, 기존 회귀 테스트도 깨뜨리지 않습니다.
- **차기 스프린트 조치 권장**: Sprint 1 백로그(subquest_mode 기본값 협의, 한글 이름 매핑 분리)와 함께 **🟡 Minor** (제안 ID 문자열 파싱 우회) 개선안을 통합 백로그로 관리하여 일괄 개선을 추진할 것을 권장합니다.

---

## 4. 긍정적인 부분

- **정밀한 외과적 변경**: 기존 `OptimizationSuggestionTool`의 고유 정렬 및 제안 생성 책임을 오염시키지 않고, 기획 권장안대로 LangGraph의 노드(`nodes.py`) 계층 내부에서 타겟 관련 정렬과 보강만 깔끔하게 추가하여 구조적 독립성을 지켰습니다.
- **예외 복원력**: 잘못된 타겟 페이로드를 전달할 시에도 Pydantic validation을 통해 시스템 예외 없이 정규 에러 응답(`INVALID_REQUEST_PAYLOAD`)으로 우아하게 대응함을 검증했습니다.

---

## 5. 검증 결과 요약

신규 추가된 5건의 타겟 분석 테스트를 포함한 총 33건의 테스트를 모두 통과했습니다.

```text
backend\tests\test_process_optimizer.py ...........                      [ 33%]
backend\tests\test_process_optimizer_state_update_alert.py ........      [ 57%]
backend\tests\test_process_optimizer_target_analyze.py .....             [ 72%]
backend\tests\test_process_optimizer_analyzer.py .........               [100%]

============================= 33 passed in 4.47s ==============================
```
