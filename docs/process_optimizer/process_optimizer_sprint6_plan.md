# Process Optimizer Agent 스프린트 6 기획서

이 문서는 `process_optimizer` Agent의 **스프린트 6. 통합 Smoke Test와 발표용 정리**를 위한 기획서입니다.

## 1. 목표
- 백엔드에 구축된 전체 에이전트 파이프라인의 통합 스모크 테스트 러너(`smoke_agent_pipeline.py`)에 맞춰 `process_optimizer` 연동 검증을 최신화하고 보완합니다.
- 자동 실행 명령이 차단된 안전한 제안 응답 규격이 완벽하게 유지되는지 최종 점검합니다.
- 포트폴리오 발표 및 데모 시나리오를 효과적으로 전달할 수 있도록 설명 문서를 보강하고 정리합니다.

## 2. 주요 작업 범위

### A. Smoke Runner (`smoke_agent_pipeline.py`) 최신화
- **이슈**: 기존 스모크 스크립트에 탑재된 `process_optimizer` 요청 메시지 payload는 `factory_state` 없이 `machines`를 바로 전달하고 있어, Sprint 5에서 최종 확정한 Unreal WebSocket 연동 규격(payload 하위에 `factoryRevision` 및 `factory_state` 배치)과 불일치합니다.
- **해결 방안**: 스모크 테스트용 payload를 최종 계약 규격 사양에 완벽하게 일치시킵니다.
  ```json
  "payload": {
    "operation": "analyze",
    "goal": "balance",
    "factoryRevision": 1,
    "factory_state": {
      "machines": [
        {
          "id": "smelter_1",
          "type": "smelter",
          "status": "operating",
          "operating_rate": 0.5,
          "inputs": [{"item_id": "iron_ore", "amount": 0.0}]
        }
      ]
    }
  }
  ```

### B. 발표용 데모 가이드 최신화 (`process_optimizer_demo_guide.md`)
- **내용**: 기존 [process_optimizer_demo_guide.md](file:///c:/factory-space/docs/process_optimizer/process_optimizer_demo_guide.md) 문서를 점검하여 최신화된 WebSocket 연동 스펙(payload 하위 데이터 배치 등)을 시나리오 메시지 예시에 정확히 반영합니다.
- 발표 및 포트폴리오 작성 시 면접관/검수자에게 process_optimizer의 차별화 요소(결정론적 분석 기반 AI 윤색 모델 및 강한 프롬프트 인젝션 방어선)를 효과적으로 어필할 수 있는 설명을 정돈합니다.

---

## 3. 구현 및 수정 대상
- **[MODIFY]** `backend/scripts/smoke_agent_pipeline.py`:
  - `process_optimizer` 테스트 케이스의 `payload` 데이터를 정식 연동 스펙으로 최신화.
- **[MODIFY]** `docs/process_optimizer/process_optimizer_demo_guide.md`:
  - 변경된 계약 스펙을 기반으로 데모 가이드 문서의 JSON 예시 및 시나리오 갱신.

---

## 4. 검증 계획
- 로컬 개발 환경 서버 구동 후 `python scripts/smoke_agent_pipeline.py local` 혹은 mock 테스트 환경에서 전체 스모크 스크립트 실행이 성공하는지 확인합니다.
- `ruff check` 검사를 통과시킵니다.
