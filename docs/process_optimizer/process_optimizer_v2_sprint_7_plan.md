# Process Optimizer v2 Sprint 7 계획

## 목표

적용 전후 지표를 비교해 예상 효과와 실제 효과를 구분해서 반환한다.

효과가 낮거나 악화되어도 자동으로 undo하지 않고, 최신 상태 기준 재분석을 안내한다.

## 구현 범위

```text
- measurement 요청 schema 정의
- 관찰 시간 검증
- production cycle 검증
- before/after metrics 계산
- 예상 효과와 실제 효과 비교
- 개선 성공/미달/악화 분류
- 재분석 안내 응답
```

## 추가 또는 수정 파일

```text
backend/src/agents/process_optimizer/effect_measurement.py
backend/src/agents/process_optimizer/nodes.py
backend/src/agents/process_optimizer/schemas.py
backend/tests/test_process_optimizer_effect_measurement.py
```

## Measure Graph 흐름

```text
START
-> validate_measurement_window
-> load_execution_record
-> calculate_before_after_metrics
-> compare_expected_and_actual_effects
-> classify_effect_result
-> return_measurement_summary
-> END
```

## 관찰 조건

```text
- 적용 후 최소 30초 경과
- 최소 3 production cycle 경과
```

## 성공 기준

```text
- 관찰 시간이 부족하면 measurement_not_ready를 반환한다.
- production cycle이 부족하면 measurement_not_ready를 반환한다.
- 예상 효과와 실제 효과를 분리해 반환한다.
- 악화된 경우 next_action: "reanalyze"를 반환한다.
- 악화되어도 자동 undo command를 만들지 않는다.
```

## 테스트 계획

```text
- 관찰 시간 부족
- production cycle 부족
- 개선 성공
- 개선 미달
- 악화 후 reanalyze 안내
- 자동 undo 미생성
```

## 완료 후 확인 질문

```text
- 예상과 실제를 명확히 구분하는가?
- 측정 준비 전에는 성급히 결론 내리지 않는가?
- 악화 시에도 플레이어 승인 없는 자동 변경을 막는가?
```

