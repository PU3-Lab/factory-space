# Process Optimizer v1 -> v2 개선 기록

## 목적

이 문서는 `process_optimizer`가 초기 v1 MVP에서 v2 LangGraph 구조로 전환되며 무엇이 개선되었는지 포트폴리오와 면접에서 설명하기 쉽게 정리한 기록이다.

핵심 메시지는 다음과 같다.

```text
v1은 공장 상태를 읽고 추천 문장을 만드는 분석/제안 MVP였다.
v2는 주기 상태 업데이트, 분석, 승인, 실행 명령 생성, Undo 충돌 검증, 효과 측정을 분리한 안전 실행형 Agent 구조다.
```

## 구조 개선 요약

| 항목 | v1 MVP | v2 LangGraph | 개선 효과 |
| --- | --- | --- | --- |
| 주요 역할 | 분석/제안 응답 | state_update / analyze / apply / undo / measure | 단순 추천에서 실행 흐름 관리로 확장 |
| public 라우팅 | leaf agent 기반 LLM/fallback 경로 | 전용 `process_optimizer_v2_graph` | 상태별 분기와 검증 책임 명확화 |
| 실행 전 승인 | 없음 | `approval_required`로 차단 | 승인 없는 공장 변경 방지 |
| 공장 버전 충돌 | 없음 | `revision_conflict` 검증 | 오래된 preview 실행 방지 |
| 명령 검증 | prompt 규칙 중심 | command whitelist + schema | LLM 임의 명령 생성 위험 축소 |
| 되돌리기 | 없음 | execution record 기반 undo | 전체 snapshot 없이 변경 항목만 복구 |
| Undo 충돌 | 없음 | `undo_conflict` 검증 | 플레이어가 직접 수정한 항목 자동 복구 차단 |
| 효과 측정 | 없음 | `measurement_ready` / `measurement_not_ready` | 예상 효과와 실제 결과 비교 |

## 정량 지표

| 지표 | v1 | v2 | 변화 |
| --- | ---: | ---: | --- |
| 지원 operation 수 | 1개 수준, suggestion 중심 | 5개, state_update/analyze/apply/undo/measure | +400% |
| 실행 안전 게이트 | 1~2개 수준, prompt/fallback 중심 | 7개 이상 | 실행 전 검증 단계 대폭 증가 |
| 변경 계획 제한 | 추천 문장 중심 | 최대 3개 change 제한 | UI 검토 가능 범위로 제한 |
| 승인 없는 실행 차단 | 미지원 | 지원 | `approval_required` |
| revision 충돌 차단 | 미지원 | 지원 | `revision_conflict` |
| undo 충돌 차단 | 미지원 | 지원 | `undo_conflict` |
| 효과 측정 준비 조건 | 미지원 | 30초 + 3 production cycle | 성급한 평가 차단 |
| 관련 테스트 통과 기록 | v1 직접 legacy 테스트 | v2 graph/pipeline/smoke 포함 | 최근 검증: 82 passed |

최근 확인한 테스트 명령:

```powershell
$env:PYTHONDONTWRITEBYTECODE='1'
.\.venv\Scripts\python.exe -m pytest tests/test_pipeline_edges.py tests/test_process_optimizer.py tests/test_process_optimizer_graph.py tests/test_process_optimizer_apply.py tests/test_process_optimizer_undo.py tests/test_process_optimizer_effect_measurement.py tests/test_process_optimizer_smoke.py -q
```

결과:

```text
82 passed
```

## v2의 안전 게이트

v2는 LLM이 공장을 직접 조작하지 않도록 실행 단계 앞뒤에 결정론적 검증을 둔다.

```text
1. factory_state / factoryRevision 입력 검증
2. 분석 지표 계산
3. 최대 3개 preview change 생성
4. 허용 command whitelist 검증
5. player approval 검증
6. preview plan 만료 및 revision 충돌 검증
7. 선택 change id 검증
8. execution record 저장
9. undo 시 recorded after 상태와 현재 상태 비교
10. measure 시 최소 관찰 시간과 production cycle 검증
```

이 구조 덕분에 악의적 입력이나 LLM 응답이 있더라도 실제 실행 명령은 코드 검증을 통과한 항목만 생성된다.

## 현재 v1의 위치

v1 `ProcessOptimizerAgent`는 삭제하지 않고 다음 용도로만 남긴다.

```text
- 초기 MVP와 v2 구조를 비교하는 reference
- legacy 동작을 직접 검증하는 테스트 대상
- 포트폴리오에서 "처음 만든 구조의 한계와 개선 과정"을 설명하는 근거
```

public `agent: "process_optimizer"` 요청의 메인 경로는 v1이 아니라 v2다.
기본 agent router에도 v1을 등록하지 않으므로 WebSocket/agent-test의 주요 요청은 v1을 거치지 않는다.

```text
process_optimizer request
-> validate_process_payload
-> process_optimizer_v2_graph
-> state_update / analyze / apply / undo / measure
```

v2의 Tool, Middleware, System Prompt 책임은 `process_optimizer_v2_runtime_contract.md`에 별도로 정리한다.

## 면접 설명 예시

```text
처음에는 process_optimizer를 v1 MVP로 만들어 공장 snapshot을 분석하고 추천 문장을 반환하게 했습니다.
하지만 실제 게임에서 공장을 바꾸려면 승인, 버전 충돌, 부분 실패, Undo 충돌, 효과 측정 같은 상태 전이가 필요했습니다.
그래서 v2에서는 전용 LangGraph로 state_update/analyze/apply/undo/measure를 분리했고,
LLM은 설명 역할로 제한하고 명령 후보, 승인 검증, revision 검증, Undo 충돌 검증은 코드가 맡도록 바꿨습니다.
그 결과 지원 operation은 1개 수준에서 5개로 늘었고, 주기 상태 업데이트, 승인 없는 실행, revision 충돌, undo 충돌, 측정 준비 미달 같은 안전 시나리오를 테스트로 검증할 수 있게 되었습니다.
```

## 포트폴리오 한 줄 요약

```text
LLM 추천형 MVP를 플레이어 승인, 버전 충돌 검증, 실행 기록 기반 Undo, 효과 측정까지 포함한 LangGraph 기반 안전 실행 Agent로 확장했다.
```

## 남은 보강 포인트

```text
- 실제 Unreal 데모에서 preview highlight와 selected apply 확인
- 실제 Unreal command result를 받아 partial success 기록 확인
- v2 설명용 LLM node를 붙일 경우, 명령 생성 권한 없이 요약만 담당하도록 제한
- v1 legacy 테스트 이름을 legacy_v1 기준으로 더 명확히 정리
```
