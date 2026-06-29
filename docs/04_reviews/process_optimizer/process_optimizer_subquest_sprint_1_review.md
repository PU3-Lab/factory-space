# 코드 리뷰 보고서: Process Optimizer Subquest Sprint 1 (State Update Alert)

이 문서는 **Process Optimizer Subquest Sprint 1: State Update Alert** 구현 건에 대한 코드 리뷰 보고서입니다.

---

## 1. 변경 요약

- **기능 추가**: 주기적인 공장 상태 업데이트(`state_update`) 시 공장에 발생한 문제점을 감지하고 최적화를 제안하는 알림(`optimization_alert`)을 생성하여 반환하도록 기능을 확장했습니다.
- **스키마 확장**:
  - `ProcessOptimizerPayload` 모델에 선택 필드 `request_source`, `target`, `subquest_mode`를 추가했습니다.
  - `SuggestedSubquestNextRequest`, `SuggestedSubquest`, `OptimizationAlert` 스키마를 신설하여 상태 업데이트 확인 응답에 규격화된 알림 데이터가 실리도록 구성했습니다.
  - `ProcessOptimizerResponse`에 `optimization_alert` 필드를 추가했습니다.
- **비즈니스 로직 구현**:
  - `SubquestAlertBuilder` 클래스를 `backend/src/agents/process_optimizer/subquest_alert.py`에 신설했습니다.
  - 전력 부족(high) -> 입력 부족(medium) -> 출력 적체(medium) -> 컨베이어 혼잡(low/medium) 순서로 가장 중요한 단 하나의 이슈를 식별해 알림을 구성합니다.
  - 장비 종류(`MACHINE_TYPE_MAP`) 및 자원 종류(`ITEM_NAME_MAP`)의 한국어 번역 매핑 테이블을 바탕으로, 플레이어 친화적인 알림 메시지와 목표(`objective`)를 자동 빌드합니다.
- **미들웨어 결합**:
  - `middleware.py` 내 `build_state_update_response` 함수가 공장 상태 저장 후 `FactoryStateAnalyzerTool`과 `SubquestAlertBuilder`를 순차 구동하여 응답 payload에 알림 정보를 결합해 반환하도록 수정했습니다.
- **테스트 커버리지 확보**:
  - `backend/tests/test_process_optimizer_state_update_alert.py` 테스트 스위트를 신설하여 유닛 및 통합 테스트 케이스 8건을 작성 및 통과시켰습니다.

---

## 2. 이슈 목록

### 🟡 Minor: 장비 및 자원 한글 이름 매핑의 하드코딩
- **위치**: `backend/src/agents/process_optimizer/subquest_alert.py:16-53`
- **내용**: `MACHINE_TYPE_MAP`과 `ITEM_NAME_MAP` 딕셔너리가 Python 소스 코드 내에 하드코딩되어 있습니다.
- **영향**: 향후 게임 내에 새로운 장비(예: 정제소)나 자원이 추가될 때마다 백엔드 비즈니스 로직 코드를 수정하고 배포해야 하는 유지보수 부담이 발생합니다.
- **제안**: 이 매핑을 공통 설정 파일(예: JSON, YAML)로 분리하여 로드하거나, 별도의 로컬라이제이션 서비스/모듈에서 제공받도록 개선할 것을 권장합니다.

### ⚪ Nit: `subquest_mode` 미지정 시 기본 활성화(Fallback) 정책
- **위치**: `backend/src/agents/process_optimizer/middleware.py:75-77`
- **내용**: 클라이언트가 보낸 payload에 `subquest_mode` 필드가 없을 때 `True`로 처리하고 있습니다.
- **영향**: 상태 업데이트는 매우 빈번하게 들어오는 요청이므로, 클라이언트가 명시하지 않은 경우에도 매번 분석 및 알림 감지 빌더가 실행되어 CPU 리소스를 추가로 소모하게 됩니다.
- **제안**: Unreal 사양과 협의하여, Unreal 측에서 알림 확인이 실제로 필요한 주기 또는 시점에만 `subquest_mode: true`를 명시하도록 구성하고, 기본값을 `False`로 가볍게 처리하는 편이 시스템 최적화 관점에서 유리할 수 있습니다.

---

## 3. 우선순위 권고

- **즉시 조치 필요 없음**: 현재 구현은 Sprint 1의 사양 및 테스트 계획을 완벽히 만족하며, 기존 회귀 테스트 역시 모두 정상 통과되었습니다.
- **차기 스프린트 조치 권장**: 게임 내 새로운 자원이나 장비가 추가되는 시점에 맞춰 **🟡 Minor** (로컬라이제이션 테이블 파일 분리) 리팩터링 작업을 스프린트 백로그에 등록하여 처리하는 것을 권장합니다.

---

## 4. 긍정적인 부분

- **목표 대비 완벽성**: 기획서에 요구된 예외 케이스(정상 공장, 입력 부족, 출력 적체, 전력 부족, 컨베이어 정체, subquest_mode 비활성화)가 완벽히 커버되었으며, 응답에 불필요한 `commands`나 `plan_id` 등이 전혀 포함되지 않도록 보장했습니다.
- **한국어 대응의 고도화**: 단순 기계 ID 표시를 넘어 `MACHINE_TYPE_MAP`과 `ITEM_NAME_MAP`을 기반으로 동적인 문장 구성("제련기 입력 라인 복구", "smelter_1에 철광석 공급이 다시 들어오도록...")을 구현하여 플레이어 사용자 경험(UX) 측면의 마감을 대폭 높였습니다.
- **독립성 유지**: 기존 LangGraph의 최적화 실행(Analyze/Apply/Undo) 흐름을 방해하지 않는 비파괴적(Non-breaking) 방식으로 `state_update` 응답 기능만 외과적으로 확장했습니다.

---

## 5. 검증 결과 요약

신규 작성된 8건의 유닛/통합 테스트와 기존 프로세스 옵티마이저 관련 테스트 20건을 포함한 총 28건의 테스트를 모두 통과했습니다.

```text
backend\tests\test_process_optimizer.py ...........                      [ 39%]
backend\tests\test_process_optimizer_state_update_alert.py ........      [ 67%]
backend\tests\test_process_optimizer_analyzer.py .........               [100%]

============================= 28 passed in 3.74s ==============================
```
