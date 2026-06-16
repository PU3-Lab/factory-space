# 물질 이름·희귀도·속성·상태 결정론화 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 신물질 생성 시 속성·category·state·희귀도를 입력 재료로부터 결정론적으로 산출하고, 이름은 LLM이 화학물질 스타일로 짓되 규칙 검증하며, `material_hash`를 합성 정체성 기반으로 재정의한다.

**Architecture:** 새 모듈 `material_properties.py`(기초 재료 속성 테이블)와 `derivation.py`(조합·공정보정·category·state·rarity 산출)를 추가한다. LangGraph에 `derive` 노드를 끼워 LLM 호출 전에 결정론 값을 계산하고, LLM은 이름·설명만 생성한 뒤 구조화 값을 derived 결과로 덮어쓴다. `generate_material_hash`는 (장비+정규화 입력+공정조건) 기반으로 재정의한다.

**Tech Stack:** Python 3.12, Pydantic, SQLAlchemy, Alembic, LangGraph, pytest.

**테스트 실행 명령(모든 Task 공통):**
```bash
cd backend && source .venv/bin/activate && python -m pytest <경로> -v
```
**린트(코드 변경 후 매번):** `cd backend && source .venv/bin/activate && ruff check --fix . && ruff format .`

---

## 파일 구조

| 파일 | 책임 |
|------|------|
| `src/agents/material_generation/material_properties.py` (신규) | 기초 재료 기준 속성 테이블, item_id 접미사 정규화, 속성/금속군 조회 |
| `src/agents/material_generation/derivation.py` (신규) | 속성 조합, 공정 보정, category·state·rarity 결정론 산출 |
| `src/agents/material_generation/schemas.py` | `state` 필드 추가 |
| `src/db/models.py` | `GeneratedMaterialModel.state` 컬럼 |
| `migrations/versions/0003_add_material_state.py` (신규) | state 컬럼 마이그레이션 |
| `src/agents/material_generation/normalizer.py` | `generate_material_hash` 재정의 |
| `src/agents/material_generation/result_validator.py` | 이름·id_hint 검증으로 축소 |
| `src/agents/material_generation/proposal_generator.py` | 프롬프트 명명 가이드 + derived 힌트 + fallback state |
| `src/agents/material_generation/graph_state.py` | `derived` 상태 키 |
| `src/agents/material_generation/graph.py` | `derive` 노드 배선 |
| `src/agents/material_generation/nodes.py` | derive 노드, 구조화 값 override, hash 호출, state 저장/응답 |

---

## Task 1: 기초 재료 속성 테이블 모듈

**Files:**
- Create: `src/agents/material_generation/material_properties.py`
- Test: `tests/agents/material_generation/test_material_properties.py`

- [ ] **Step 1: 실패하는 테스트 작성**

`tests/agents/material_generation/test_material_properties.py`:
```python
"""기초 재료 속성 테이블 조회 단위 테스트입니다."""

from __future__ import annotations

from agents.material_generation.material_properties import (
    METALS,
    NEUTRAL_PROPERTIES,
    base_key,
    get_base_properties,
)


def test_base_key_strips_known_suffixes() -> None:
    assert base_key("iron_ingot") == "iron"
    assert base_key("coal_dust") == "coal"
    assert base_key("iron_ore") == "iron"
    assert base_key("copper_powder") == "copper"
    assert base_key("sulfur") == "sulfur"


def test_get_base_properties_returns_table_value() -> None:
    assert get_base_properties("tungsten_ingot") == (10.0, 4.0, 9.0, 1.0)
    assert get_base_properties("gold_powder") == (2.0, 10.0, 10.0, 1.0)


def test_get_base_properties_unknown_is_neutral() -> None:
    assert get_base_properties("unobtainium_ingot") == NEUTRAL_PROPERTIES


def test_metals_membership() -> None:
    assert "iron" in METALS
    assert "sulfur" not in METALS
```

- [ ] **Step 2: 테스트 실패 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_material_properties.py -v`
Expected: FAIL (`ModuleNotFoundError: material_properties`)

- [ ] **Step 3: 모듈 구현**

`src/agents/material_generation/material_properties.py`:
```python
"""기초 재료의 기준 속성 테이블 및 조회 유틸리티입니다."""

from __future__ import annotations

# (strength, conductivity, stability, reactivity), 각 0.0~10.0
BASE_MATERIAL_PROPERTIES: dict[str, tuple[float, float, float, float]] = {
    "iron": (7.0, 5.0, 7.0, 4.0),
    "copper": (4.0, 9.0, 6.0, 4.0),
    "zinc": (3.0, 5.0, 5.0, 6.0),
    "lead": (2.0, 4.0, 6.0, 3.0),
    "tin": (3.0, 5.0, 6.0, 3.0),
    "aluminum": (4.0, 8.0, 6.0, 5.0),
    "nickel": (6.0, 5.0, 7.0, 3.0),
    "tungsten": (10.0, 4.0, 9.0, 1.0),
    "titanium": (8.0, 3.0, 9.0, 2.0),
    "magnesium": (3.0, 6.0, 4.0, 8.0),
    "gold": (2.0, 10.0, 10.0, 1.0),
    "silver": (3.0, 10.0, 8.0, 2.0),
    "charcoal": (1.0, 2.0, 4.0, 6.0),
    "coal": (1.0, 1.0, 4.0, 6.0),
    "sulfur": (1.0, 1.0, 3.0, 8.0),
    "wood": (1.0, 1.0, 3.0, 4.0),
}

NEUTRAL_PROPERTIES: tuple[float, float, float, float] = (5.0, 5.0, 5.0, 5.0)

METALS: frozenset[str] = frozenset(
    {
        "iron",
        "copper",
        "zinc",
        "lead",
        "tin",
        "aluminum",
        "nickel",
        "tungsten",
        "titanium",
        "magnesium",
        "gold",
        "silver",
    }
)

# 탄소·목재 계열(유기물군)
ORGANIC_GROUP: frozenset[str] = frozenset({"charcoal", "coal", "wood"})

_SUFFIXES: tuple[str, ...] = ("_ingot", "_powder", "_dust", "_ore")


def base_key(item_id: str) -> str:
    """item_id에서 형태 접미사(_ingot/_powder/_dust/_ore)를 제거한 기초 원소 키를 반환합니다."""
    key = item_id.strip().lower()
    for suffix in _SUFFIXES:
        if key.endswith(suffix):
            return key[: -len(suffix)]
    return key


def get_base_properties(item_id: str) -> tuple[float, float, float, float]:
    """기초 재료의 기준 속성을 반환합니다. 테이블에 없으면 중립값(5.0)을 반환합니다."""
    return BASE_MATERIAL_PROPERTIES.get(base_key(item_id), NEUTRAL_PROPERTIES)
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_material_properties.py -v`
Expected: PASS (4 passed)

- [ ] **Step 5: 린트 + 커밋**

```bash
cd backend && source .venv/bin/activate && ruff check --fix . && ruff format .
git add src/agents/material_generation/material_properties.py tests/agents/material_generation/test_material_properties.py
git commit -m "feat(material): 기초 재료 속성 테이블 모듈 추가"
```

---

## Task 2: 속성 조합 공식 (수량 가중 평균)

**Files:**
- Create: `src/agents/material_generation/derivation.py`
- Test: `tests/agents/material_generation/test_derivation.py`

- [ ] **Step 1: 실패하는 테스트 작성**

`tests/agents/material_generation/test_derivation.py`:
```python
"""결정론적 물질 속성 산출 단위 테스트입니다."""

from __future__ import annotations

from agents.material_generation.derivation import combine_properties


def test_combine_single_input() -> None:
    # iron = (7,5,7,4)
    result = combine_properties([{"item_id": "iron_ingot", "qty": 1}])
    assert result == (7.0, 5.0, 7.0, 4.0)


def test_combine_qty_weighted_average() -> None:
    # iron=(7,5,7,4) x1, copper=(4,9,6,4) x1 -> 평균 (5.5,7,6.5,4)
    result = combine_properties(
        [
            {"item_id": "iron_ingot", "qty": 1},
            {"item_id": "copper_ingot", "qty": 1},
        ]
    )
    assert result == (5.5, 7.0, 6.5, 4.0)


def test_combine_respects_quantity_weight() -> None:
    # iron x3, copper x1 -> strength (7*3+4*1)/4 = 6.25
    result = combine_properties(
        [
            {"item_id": "iron_ingot", "qty": 3},
            {"item_id": "copper_ingot", "qty": 1},
        ]
    )
    assert result[0] == 6.25
```

- [ ] **Step 2: 테스트 실패 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_derivation.py -v`
Expected: FAIL (`ModuleNotFoundError: derivation`)

- [ ] **Step 3: 모듈 + 함수 구현**

`src/agents/material_generation/derivation.py`:
```python
"""입력 재료로부터 물질 속성·category·state·rarity를 결정론적으로 산출합니다."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from agents.material_generation.material_properties import (
    METALS,
    ORGANIC_GROUP,
    base_key,
    get_base_properties,
)
from agents.material_generation.schemas import (
    MaterialProperties,
    ProcessConditionsSchema,
)


@dataclass
class DerivedAttributes:
    """결정론적으로 산출된 물질의 구조화 속성 묶음입니다."""

    properties: MaterialProperties
    category: str
    state: str
    rarity: str


def _clamp(value: float) -> float:
    return max(0.0, min(10.0, value))


def combine_properties(
    normalized_inputs: list[dict[str, Any]],
) -> tuple[float, float, float, float]:
    """입력 재료들의 기준 속성을 수량 가중 평균으로 합성합니다."""
    total_qty = sum(item["qty"] for item in normalized_inputs)
    if total_qty <= 0:
        return (5.0, 5.0, 5.0, 5.0)

    sums = [0.0, 0.0, 0.0, 0.0]
    for item in normalized_inputs:
        props = get_base_properties(item["item_id"])
        for i in range(4):
            sums[i] += props[i] * item["qty"]

    return (
        sums[0] / total_qty,
        sums[1] / total_qty,
        sums[2] / total_qty,
        sums[3] / total_qty,
    )
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_derivation.py -v`
Expected: PASS (3 passed)

- [ ] **Step 5: 린트 + 커밋**

```bash
cd backend && source .venv/bin/activate && ruff check --fix . && ruff format .
git add src/agents/material_generation/derivation.py tests/agents/material_generation/test_derivation.py
git commit -m "feat(material): 속성 수량 가중 평균 조합 함수 추가"
```

---

## Task 3: 공정 조건 보정

**Files:**
- Modify: `src/agents/material_generation/derivation.py`
- Test: `tests/agents/material_generation/test_derivation.py`

- [ ] **Step 1: 실패하는 테스트 추가** (`test_derivation.py` 상단 import에 `apply_process`·`ProcessConditionsSchema` 추가, 아래 함수 추가)

```python
from agents.material_generation.derivation import apply_process, combine_properties
from agents.material_generation.schemas import ProcessConditionsSchema


def test_apply_process_high_temp() -> None:
    # base (5,5,5,5), high temp -> reactivity+1, stability-1
    out = apply_process((5.0, 5.0, 5.0, 5.0), ProcessConditionsSchema(temperature="high"))
    assert out == (5.0, 5.0, 4.0, 6.0)


def test_apply_process_catalyst_and_pressure() -> None:
    out = apply_process(
        (5.0, 5.0, 5.0, 5.0),
        ProcessConditionsSchema(pressure="high", catalyst="platinum"),
    )
    # strength+1, stability+1, reactivity+1
    assert out == (6.0, 5.0, 6.0, 6.0)


def test_apply_process_clamps() -> None:
    out = apply_process((10.0, 10.0, 0.5, 9.5), ProcessConditionsSchema(temperature="high"))
    # stability 0.5-1 -> clamp 0, reactivity 9.5+1 -> clamp 10
    assert out == (10.0, 10.0, 0.0, 10.0)


def test_apply_process_default_is_noop() -> None:
    out = apply_process((5.0, 5.0, 5.0, 5.0), ProcessConditionsSchema())
    assert out == (5.0, 5.0, 5.0, 5.0)
```

- [ ] **Step 2: 테스트 실패 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_derivation.py -k apply_process -v`
Expected: FAIL (`ImportError: cannot import name 'apply_process'`)

- [ ] **Step 3: 함수 구현** (`derivation.py`에 추가)

```python
def apply_process(
    props: tuple[float, float, float, float],
    conditions: ProcessConditionsSchema,
) -> tuple[float, float, float, float]:
    """공정 조건(온도/압력/촉매)에 따라 속성을 보정하고 0~10으로 클램핑합니다."""
    strength, conductivity, stability, reactivity = props

    temp = (conditions.temperature or "").strip().lower()
    pressure = (conditions.pressure or "").strip().lower()

    if temp == "high":
        reactivity += 1.0
        stability -= 1.0
    elif temp == "low":
        reactivity -= 1.0
        stability += 1.0

    if pressure == "high":
        strength += 1.0
        stability += 1.0

    if conditions.catalyst:
        reactivity += 1.0

    return (
        _clamp(strength),
        _clamp(conductivity),
        _clamp(stability),
        _clamp(reactivity),
    )
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_derivation.py -k apply_process -v`
Expected: PASS (4 passed)

- [ ] **Step 5: 린트 + 커밋**

```bash
cd backend && source .venv/bin/activate && ruff check --fix . && ruff format .
git add src/agents/material_generation/derivation.py tests/agents/material_generation/test_derivation.py
git commit -m "feat(material): 공정 조건 속성 보정 함수 추가"
```

---

## Task 4: category 결정론 산출

**Files:**
- Modify: `src/agents/material_generation/derivation.py`
- Test: `tests/agents/material_generation/test_derivation.py`

- [ ] **Step 1: 실패하는 테스트 추가** (import에 `derive_category` 추가)

```python
def test_derive_category_all_metals_is_alloy() -> None:
    inputs = [{"item_id": "iron_ingot", "qty": 1}, {"item_id": "copper_ingot", "qty": 1}]
    assert derive_category(inputs) == "alloy"


def test_derive_category_organic() -> None:
    inputs = [{"item_id": "charcoal_dust", "qty": 1}, {"item_id": "wood", "qty": 1}]
    assert derive_category(inputs) == "organic"


def test_derive_category_chemical() -> None:
    inputs = [{"item_id": "sulfur_powder", "qty": 1}, {"item_id": "coal_dust", "qty": 1}]
    assert derive_category(inputs) == "chemical"


def test_derive_category_composite() -> None:
    inputs = [{"item_id": "iron_ingot", "qty": 1}, {"item_id": "sulfur_powder", "qty": 1}]
    assert derive_category(inputs) == "composite"
```

- [ ] **Step 2: 테스트 실패 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_derivation.py -k derive_category -v`
Expected: FAIL (`ImportError`)

- [ ] **Step 3: 함수 구현** (`derivation.py`에 추가)

```python
def derive_category(normalized_inputs: list[dict[str, Any]]) -> str:
    """입력 재료 구성으로 category를 판정합니다 (alloy/organic/chemical/composite)."""
    keys = {base_key(item["item_id"]) for item in normalized_inputs}
    metals = keys & METALS
    nonmetals = keys - METALS

    if not nonmetals:
        return "alloy"
    if not metals:
        if keys <= ORGANIC_GROUP:
            return "organic"
        return "chemical"
    return "composite"
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_derivation.py -k derive_category -v`
Expected: PASS (4 passed)

- [ ] **Step 5: 린트 + 커밋**

```bash
cd backend && source .venv/bin/activate && ruff check --fix . && ruff format .
git add src/agents/material_generation/derivation.py tests/agents/material_generation/test_derivation.py
git commit -m "feat(material): category 결정론 산출 함수 추가"
```

---

## Task 5: state 결정론 산출 (solid/liquid/gas/plasma)

**Files:**
- Modify: `src/agents/material_generation/derivation.py`
- Test: `tests/agents/material_generation/test_derivation.py`

- [ ] **Step 1: 실패하는 테스트 추가** (import에 `derive_state` 추가)

```python
def test_derive_state_plasma() -> None:
    # reactivity>=9, stability<=1
    assert derive_state((5.0, 5.0, 1.0, 9.0)) == "plasma"


def test_derive_state_gas() -> None:
    assert derive_state((5.0, 5.0, 3.0, 7.0)) == "gas"


def test_derive_state_liquid() -> None:
    assert derive_state((5.0, 5.0, 5.0, 5.0)) == "liquid"


def test_derive_state_solid() -> None:
    assert derive_state((7.0, 5.0, 7.0, 4.0)) == "solid"
```

- [ ] **Step 2: 테스트 실패 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_derivation.py -k derive_state -v`
Expected: FAIL (`ImportError`)

- [ ] **Step 3: 함수 구현** (`derivation.py`에 추가)

```python
def derive_state(props: tuple[float, float, float, float]) -> str:
    """최종 속성으로 물리적 상태를 판정합니다. plasma를 가장 먼저 평가합니다."""
    _, _, stability, reactivity = props
    if reactivity >= 9.0 and stability <= 1.0:
        return "plasma"
    if reactivity >= 7.0 and stability <= 3.0:
        return "gas"
    if reactivity >= 5.0 and stability <= 5.0:
        return "liquid"
    return "solid"
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_derivation.py -k derive_state -v`
Expected: PASS (4 passed)

- [ ] **Step 5: 린트 + 커밋**

```bash
cd backend && source .venv/bin/activate && ruff check --fix . && ruff format .
git add src/agents/material_generation/derivation.py tests/agents/material_generation/test_derivation.py
git commit -m "feat(material): state(solid/liquid/gas/plasma) 결정론 산출 추가"
```

---

## Task 6: 희귀도 결정론 공식

**Files:**
- Modify: `src/agents/material_generation/derivation.py`
- Test: `tests/agents/material_generation/test_derivation.py`

- [ ] **Step 1: 실패하는 테스트 추가** (import에 `compute_rarity` 추가)

```python
def test_compute_rarity_common() -> None:
    # avg=3, peak=3 -> score 3.0
    assert compute_rarity((3.0, 3.0, 3.0, 3.0)) == "common"


def test_compute_rarity_uncommon_boundary() -> None:
    # avg=5, peak=5 -> score 5.0
    assert compute_rarity((5.0, 5.0, 5.0, 5.0)) == "uncommon"


def test_compute_rarity_rare_boundary() -> None:
    # avg=7, peak=7 -> score 7.0
    assert compute_rarity((7.0, 7.0, 7.0, 7.0)) == "rare"


def test_compute_rarity_epic_boundary() -> None:
    # avg=8.5, peak=8.5 -> score 8.5
    assert compute_rarity((8.5, 8.5, 8.5, 8.5)) == "epic"
```

- [ ] **Step 2: 테스트 실패 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_derivation.py -k compute_rarity -v`
Expected: FAIL (`ImportError`)

- [ ] **Step 3: 함수 구현** (`derivation.py`에 추가)

```python
def compute_rarity(props: tuple[float, float, float, float]) -> str:
    """속성으로부터 희귀도를 결정론적으로 계산합니다."""
    strength, conductivity, stability, reactivity = props
    avg = (strength + conductivity + stability) / 3.0
    peak = max(strength, conductivity, stability, reactivity)
    score = 0.7 * avg + 0.3 * peak

    if score >= 8.5:
        return "epic"
    if score >= 7.0:
        return "rare"
    if score >= 5.0:
        return "uncommon"
    return "common"
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_derivation.py -k compute_rarity -v`
Expected: PASS (4 passed)

- [ ] **Step 5: 린트 + 커밋**

```bash
cd backend && source .venv/bin/activate && ruff check --fix . && ruff format .
git add src/agents/material_generation/derivation.py tests/agents/material_generation/test_derivation.py
git commit -m "feat(material): 희귀도 결정론 공식 추가"
```

---

## Task 7: 통합 산출 함수 derive_material_attributes

**Files:**
- Modify: `src/agents/material_generation/derivation.py`
- Test: `tests/agents/material_generation/test_derivation.py`

- [ ] **Step 1: 실패하는 테스트 추가** (import에 `derive_material_attributes` 추가)

```python
def test_derive_material_attributes_integration() -> None:
    inputs = [{"item_id": "iron_ingot", "qty": 1}, {"item_id": "copper_ingot", "qty": 1}]
    attrs = derive_material_attributes(inputs, ProcessConditionsSchema())
    # 속성 (5.5,7,6.5,4)
    assert attrs.properties.strength == 5.5
    assert attrs.properties.conductivity == 7.0
    assert attrs.category == "alloy"
    assert attrs.state == "solid"
    # avg=(5.5+7+6.5)/3=6.333..., peak=7 -> 0.7*6.333+0.3*7=6.533 -> uncommon
    assert attrs.rarity == "uncommon"


def test_derive_material_attributes_is_deterministic() -> None:
    inputs = [{"item_id": "magnesium_powder", "qty": 1}, {"item_id": "sulfur_powder", "qty": 1}]
    a1 = derive_material_attributes(inputs, ProcessConditionsSchema(catalyst="x"))
    a2 = derive_material_attributes(inputs, ProcessConditionsSchema(catalyst="x"))
    assert a1 == a2
```

- [ ] **Step 2: 테스트 실패 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_derivation.py -k derive_material_attributes -v`
Expected: FAIL (`ImportError`)

- [ ] **Step 3: 함수 구현** (`derivation.py`에 추가)

```python
def derive_material_attributes(
    normalized_inputs: list[dict[str, Any]],
    conditions: ProcessConditionsSchema,
) -> DerivedAttributes:
    """입력과 공정으로부터 속성·category·state·rarity를 한 번에 산출합니다."""
    combined = combine_properties(normalized_inputs)
    adjusted = apply_process(combined, conditions)
    properties = MaterialProperties(
        strength=adjusted[0],
        conductivity=adjusted[1],
        stability=adjusted[2],
        reactivity=adjusted[3],
    )
    return DerivedAttributes(
        properties=properties,
        category=derive_category(normalized_inputs),
        state=derive_state(adjusted),
        rarity=compute_rarity(adjusted),
    )
```

- [ ] **Step 4: 테스트 통과 확인 (전체 derivation)**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_derivation.py -v`
Expected: PASS (전체 통과)

- [ ] **Step 5: 린트 + 커밋**

```bash
cd backend && source .venv/bin/activate && ruff check --fix . && ruff format .
git add src/agents/material_generation/derivation.py tests/agents/material_generation/test_derivation.py
git commit -m "feat(material): derive_material_attributes 통합 산출 함수 추가"
```

---

## Task 8: 스키마에 state 필드 추가

**Files:**
- Modify: `src/agents/material_generation/schemas.py`
- Test: `tests/agents/material_generation/test_schemas_state.py`

- [ ] **Step 1: 실패하는 테스트 작성**

`tests/agents/material_generation/test_schemas_state.py`:
```python
"""state 필드 스키마 단위 테스트입니다."""

from __future__ import annotations

from agents.material_generation.schemas import (
    MaterialCreationResponse,
    MaterialProperties,
    MaterialProposalResult,
)


def test_proposal_result_state_defaults_solid() -> None:
    result = MaterialProposalResult(
        id_hint="x",
        name="테스트물질",
        category="alloy",
        rarity="common",
        description="d",
        properties=MaterialProperties(
            strength=1.0, conductivity=1.0, stability=1.0, reactivity=1.0
        ),
        visual_prompt="p",
    )
    assert result.state == "solid"


def test_response_state_optional() -> None:
    resp = MaterialCreationResponse(result_type="new_material", experiment_hash="h")
    assert resp.state is None
```

- [ ] **Step 2: 테스트 실패 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_schemas_state.py -v`
Expected: FAIL (`AttributeError: ... 'state'`)

- [ ] **Step 3: 스키마 수정**

`schemas.py` — `MaterialProposalResult`의 `category: str` 다음 줄에 추가:
```python
    state: str = "solid"
```
`schemas.py` — `MaterialCreationResponse`의 `rarity: str | None = None` 다음 줄에 추가:
```python
    state: str | None = None
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_schemas_state.py -v`
Expected: PASS (2 passed)

- [ ] **Step 5: 린트 + 커밋**

```bash
cd backend && source .venv/bin/activate && ruff check --fix . && ruff format .
git add src/agents/material_generation/schemas.py tests/agents/material_generation/test_schemas_state.py
git commit -m "feat(material): 스키마에 state 필드 추가"
```

---

## Task 9: GeneratedMaterialModel.state 컬럼 + 마이그레이션

**Files:**
- Modify: `src/db/models.py` (`GeneratedMaterialModel`의 `description` 다음)
- Create: `migrations/versions/0003_add_material_state.py`
- Test: `tests/agents/material_generation/test_material_state_column.py`

- [ ] **Step 1: 실패하는 테스트 작성**

`tests/agents/material_generation/test_material_state_column.py`:
```python
"""GeneratedMaterialModel.state 컬럼 존재 테스트입니다."""

from __future__ import annotations

from db.models import GeneratedMaterialModel


def test_material_model_has_state_column() -> None:
    assert "state" in GeneratedMaterialModel.__table__.columns
    col = GeneratedMaterialModel.__table__.columns["state"]
    assert col.nullable is False
```

- [ ] **Step 2: 테스트 실패 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_material_state_column.py -v`
Expected: FAIL (`AssertionError: 'state' not in columns`)

- [ ] **Step 3: 모델 컬럼 추가**

`src/db/models.py` — `GeneratedMaterialModel`의 `description: Mapped[str | None] = ...` 줄 다음에 추가:
```python
    state: Mapped[str] = mapped_column(
        String(20), default="solid", server_default="solid", nullable=False
    )
```

- [ ] **Step 4: 마이그레이션 작성**

`migrations/versions/0003_add_material_state.py`:
```python
"""add_material_state

Revision ID: 0003_add_material_state
Revises: 0002_create_material_tables
Create Date: 2026-06-15 00:00:00.000000

"""

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "0003_add_material_state"
down_revision: str | None = "0002_create_material_tables"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.add_column(
        "generated_materials",
        sa.Column(
            "state",
            sa.String(length=20),
            server_default="solid",
            nullable=False,
        ),
    )


def downgrade() -> None:
    op.drop_column("generated_materials", "state")
```

- [ ] **Step 5: 테스트 통과 + 마이그레이션 검증**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_material_state_column.py -v`
Expected: PASS (1 passed)
Run: `cd backend && source .venv/bin/activate && alembic upgrade head`
Expected: `0003_add_material_state` 적용, 에러 없음

- [ ] **Step 6: 린트 + 커밋**

```bash
cd backend && source .venv/bin/activate && ruff check --fix . && ruff format .
git add src/db/models.py migrations/versions/0003_add_material_state.py tests/agents/material_generation/test_material_state_column.py
git commit -m "feat(material): generated_materials.state 컬럼 및 마이그레이션 추가"
```

---

## Task 10: material_hash를 합성 정체성 기반으로 재정의

**Files:**
- Modify: `src/agents/material_generation/normalizer.py` (`generate_material_hash`)
- Test: `tests/agents/material_generation/test_normalizer.py` (기존 `test_material_hash_is_stable` 교체)

- [ ] **Step 1: 기존 테스트를 새 시그니처로 교체**

`tests/agents/material_generation/test_normalizer.py`의 `test_material_hash_is_stable` 함수 전체를 아래로 교체한다. (이 함수에서만 쓰이던 `MaterialProposalResult`/`MaterialProperties` import가 다른 테스트에서 안 쓰이면 제거.)
```python
def test_material_hash_is_synthesis_identity() -> None:
    from agents.material_generation.schemas import ProcessConditionsSchema

    inputs = [{"item_id": "copper_ingot", "qty": 1}, {"item_id": "iron_ingot", "qty": 2}]
    h1 = generate_material_hash("Synthesizer", inputs, ProcessConditionsSchema())
    h2 = generate_material_hash("Synthesizer", inputs, ProcessConditionsSchema())
    assert h1 == h2
    assert len(h1) == 64


def test_material_hash_differs_on_inputs() -> None:
    from agents.material_generation.schemas import ProcessConditionsSchema

    a = generate_material_hash(
        "Synthesizer", [{"item_id": "iron_ingot", "qty": 1}], ProcessConditionsSchema()
    )
    b = generate_material_hash(
        "Synthesizer", [{"item_id": "copper_ingot", "qty": 1}], ProcessConditionsSchema()
    )
    assert a != b
```

- [ ] **Step 2: 테스트 실패 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_normalizer.py -k material_hash -v`
Expected: FAIL (`TypeError: generate_material_hash() ... arguments`)

- [ ] **Step 3: 함수 재정의**

`normalizer.py` — `generate_material_hash` 전체를 교체. (상단 import에서 `MaterialProposalResult`가 더 이상 안 쓰이면 제거하고 `ProcessConditionsSchema` 유지.)
```python
def generate_material_hash(
    machine_type: str,
    normalized_inputs: list[dict[str, Any]],
    process_conditions: ProcessConditionsSchema,
) -> str:
    """합성 정체성(장비+정규화 입력+공정조건)에 대한 결정론적 sha256 해시를 생성합니다."""
    inputs_str = "|".join(
        f"{item['item_id']}:{item['qty']}" for item in normalized_inputs
    )
    catalyst_str = process_conditions.catalyst or "none"

    hash_payload = (
        f"{machine_type}|{inputs_str}|"
        f"temp:{process_conditions.temperature}|"
        f"pressure:{process_conditions.pressure}|"
        f"catalyst:{catalyst_str}"
    )

    return hashlib.sha256(hash_payload.encode("utf-8")).hexdigest()
```

- [ ] **Step 4: 테스트 통과 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_normalizer.py -v`
Expected: PASS (전체 통과)

- [ ] **Step 5: 린트 + 커밋**

```bash
cd backend && source .venv/bin/activate && ruff check --fix . && ruff format .
git add src/agents/material_generation/normalizer.py tests/agents/material_generation/test_normalizer.py
git commit -m "refactor(material): material_hash를 합성 정체성 기반으로 재정의"
```

---

## Task 11: result_validator를 이름·id_hint 검증으로 축소

**Files:**
- Modify: `src/agents/material_generation/result_validator.py` (전체 교체)
- Test: `tests/agents/material_generation/test_result_validator.py` (전체 교체)

> 속성 클램핑·rarity 보정은 derivation으로 이전됐으므로 validator에서 제거한다. validator는 이름/id_hint 새니타이징과 후보 개수 제한만 담당한다.

- [ ] **Step 1: 테스트 전체 교체**

`tests/agents/material_generation/test_result_validator.py` 전체를 아래로 교체:
```python
"""MaterialResultValidator(이름/id_hint 검증) 단위 테스트입니다."""

from __future__ import annotations

from agents.material_generation.result_validator import (
    FALLBACK_NAME,
    MaterialResultValidator,
)
from agents.material_generation.schemas import (
    MaterialProperties,
    MaterialProposal,
    MaterialProposalResult,
)


def _make(
    name: str,
    id_hint: str = "alloy_x",
    candidates: list[str] | None = None,
) -> MaterialProposal:
    return MaterialProposal(
        proposal_type="new_material",
        confidence=0.9,
        reason="test",
        result=MaterialProposalResult(
            id_hint=id_hint,
            name=name,
            category="alloy",
            rarity="common",
            description="d",
            properties=MaterialProperties(
                strength=5.0, conductivity=5.0, stability=5.0, reactivity=5.0
            ),
            next_recipe_candidates=candidates or [],
            visual_prompt="p",
        ),
    )


def test_sanitize_strips_forbidden_keywords() -> None:
    out = MaterialResultValidator.validate_and_correct(_make("Invalid Trash 합금"))
    assert out.result is not None
    assert "trash" not in out.result.name.lower()
    assert "invalid" not in out.result.name.lower()


def test_sanitize_removes_special_chars() -> None:
    out = MaterialResultValidator.validate_and_correct(_make("초강 #합금@! 정"))
    assert out.result is not None
    assert "#" not in out.result.name
    assert "@" not in out.result.name
    assert "!" not in out.result.name


def test_empty_name_falls_back() -> None:
    out = MaterialResultValidator.validate_and_correct(_make("   "))
    assert out.result is not None
    assert out.result.name == FALLBACK_NAME


def test_symbol_only_name_falls_back() -> None:
    out = MaterialResultValidator.validate_and_correct(_make("###@@@"))
    assert out.result is not None
    assert out.result.name == FALLBACK_NAME


def test_too_long_name_is_truncated() -> None:
    out = MaterialResultValidator.validate_and_correct(_make("가" * 40))
    assert out.result is not None
    assert len(out.result.name) == 24


def test_id_hint_sanitized() -> None:
    out = MaterialResultValidator.validate_and_correct(
        _make("정상물질", id_hint="alloy-Test!! Dummy")
    )
    assert out.result is not None
    assert out.result.id_hint == "alloy_alloy_alloy"


def test_candidates_capped_at_five() -> None:
    out = MaterialResultValidator.validate_and_correct(
        _make("정상물질", candidates=["c1", "c2", "c3", "c4", "c5", "c6", "c7"])
    )
    assert out.result is not None
    assert out.result.next_recipe_candidates == ["c1", "c2", "c3", "c4", "c5"]
```

- [ ] **Step 2: 테스트 실패 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_result_validator.py -v`
Expected: FAIL (`ImportError: FALLBACK_NAME`)

- [ ] **Step 3: validator 전체 교체**

`src/agents/material_generation/result_validator.py` 전체를 아래로 교체:
```python
"""LLM이 제안한 재료의 이름·id_hint를 검증·새니타이징합니다.

속성·희귀도·category·state는 derivation 모듈에서 결정론적으로 산출되어
주입되므로 본 검증기는 자유 텍스트(이름/id_hint)와 후보 개수만 다룬다.
"""

from __future__ import annotations

import re

from agents.material_generation.schemas import MaterialProposal

FORBIDDEN_KEYWORDS = {"trash", "error", "test", "dummy", "fuck", "shit", "invalid"}

FALLBACK_NAME = "미상의 합성물"
MIN_NAME_LEN = 2
MAX_NAME_LEN = 24
MAX_CANDIDATES = 5

# 한글·영숫자·공백·하이픈만 허용
_DISALLOWED_NAME_CHARS = re.compile(r"[^0-9A-Za-z가-힣\s\-]")
_HAS_VALID_CHAR = re.compile(r"[0-9A-Za-z가-힣]")
# id_hint: 소문자·숫자·언더스코어만
_DISALLOWED_ID_CHARS = re.compile(r"[^a-z0-9_]")


class MaterialResultValidator:
    """이름·id_hint 새니타이징 및 후보 개수 제한을 수행합니다."""

    @classmethod
    def _sanitize_name(cls, name: str) -> str:
        cleaned = name.strip()
        if not cleaned:
            return FALLBACK_NAME

        for term in FORBIDDEN_KEYWORDS:
            cleaned = re.sub(term, "합금", cleaned, flags=re.IGNORECASE)

        cleaned = _DISALLOWED_NAME_CHARS.sub("", cleaned).strip()

        if not _HAS_VALID_CHAR.search(cleaned) or len(cleaned) < MIN_NAME_LEN:
            return FALLBACK_NAME
        if len(cleaned) > MAX_NAME_LEN:
            cleaned = cleaned[:MAX_NAME_LEN].strip()
        return cleaned

    @classmethod
    def _sanitize_id_hint(cls, id_hint: str) -> str:
        cleaned = id_hint.strip().lower()
        for term in FORBIDDEN_KEYWORDS:
            cleaned = cleaned.replace(term, "alloy")
        cleaned = _DISALLOWED_ID_CHARS.sub("_", cleaned)
        cleaned = re.sub(r"_+", "_", cleaned).strip("_")
        return cleaned or "material"

    @classmethod
    def validate_and_correct(cls, proposal: MaterialProposal) -> MaterialProposal:
        """이름·id_hint를 새니타이징하고 후보 개수를 제한합니다."""
        if not proposal.result:
            return proposal

        result = proposal.result
        result.name = cls._sanitize_name(result.name)
        result.id_hint = cls._sanitize_id_hint(result.id_hint)

        if len(result.next_recipe_candidates) > MAX_CANDIDATES:
            result.next_recipe_candidates = result.next_recipe_candidates[
                :MAX_CANDIDATES
            ]

        return proposal
```

> 참고: `test_id_hint_sanitized`의 입력 `"alloy-Test!! Dummy"` → 소문자화 `"alloy-test!! dummy"` → 금지어 치환(`test→alloy`, `dummy→alloy`) → `"alloy-alloy!! alloy"` → 비허용 문자(`-`,`!`,공백)를 `_`로 치환·축약·트림 → `"alloy_alloy_alloy"`.

- [ ] **Step 4: 테스트 통과 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_result_validator.py -v`
Expected: PASS (7 passed)

- [ ] **Step 5: 린트 + 커밋**

```bash
cd backend && source .venv/bin/activate && ruff check --fix . && ruff format .
git add src/agents/material_generation/result_validator.py tests/agents/material_generation/test_result_validator.py
git commit -m "refactor(material): result_validator를 이름/id_hint 검증으로 축소"
```

---

## Task 12: proposal_generator — 명명 가이드 + derived 힌트 + fallback state

**Files:**
- Modify: `src/agents/material_generation/proposal_generator.py`
- Test: `tests/agents/material_generation/test_proposal_generator.py`

- [ ] **Step 1: 실패하는 테스트 작성**

`tests/agents/material_generation/test_proposal_generator.py`:
```python
"""MaterialProposalGenerator 프롬프트·fallback 단위 테스트입니다."""

from __future__ import annotations

from agents.material_generation.proposal_generator import MaterialProposalGenerator
from agents.material_generation.schemas import ProcessConditionsSchema


def test_prompt_includes_state_and_naming_guide() -> None:
    gen = MaterialProposalGenerator()
    prompt = gen._build_prompt(
        machine_type="Synthesizer",
        normalized_inputs=[{"item_id": "iron_ingot", "qty": 1}],
        process_conditions=ProcessConditionsSchema(),
        similar_experiments=[],
        derived_state="liquid",
        derived_category="alloy",
    )
    assert "liquid" in prompt
    assert "화학" in prompt  # 화학 물질 스타일 명명 가이드


def test_fallback_proposal_has_state() -> None:
    gen = MaterialProposalGenerator()
    proposal = gen.get_fallback_proposal([{"item_id": "iron_ingot", "qty": 1}])
    assert proposal.result is not None
    assert proposal.result.state == "solid"
```

- [ ] **Step 2: 테스트 실패 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_proposal_generator.py -v`
Expected: FAIL (`_build_prompt() got an unexpected keyword argument 'derived_state'`)

- [ ] **Step 3: 구현 수정**

`proposal_generator.py` `_build_prompt` 시그니처를 변경한다:
```python
    def _build_prompt(
        self,
        machine_type: str,
        normalized_inputs: list[dict[str, Any]],
        process_conditions: ProcessConditionsSchema,
        similar_experiments: list[dict[str, Any]],
        derived_state: str = "solid",
        derived_category: str = "alloy",
    ) -> str:
```
`_build_prompt` 본문에서 `prompt = (...)` 조립 직전에 명명 가이드를 정의한다:
```python
        naming_guide = (
            f"이 물질의 분류는 '{derived_category}', 물리적 상태는 '{derived_state}'입니다.\n"
            "이름은 실제 화학 물질·신소재처럼, 해당 상태가 연상되는 한글 명칭으로 지으십시오.\n"
            "- 고체(solid): '~정', '~합금', '~석'\n"
            "- 액체(liquid): '~용액', '~유', '~액'\n"
            "- 기체(gas): '~기체', '~가스', '~증기'\n"
            "- 플라즈마(plasma): '~플라즈마', '~이온체', '~화염체'\n"
        )
```
그리고 `prompt` f-string 조립에서 `f"{similar_context}\n"` 다음에 `f"{naming_guide}\n"`를 끼워 넣는다.

`generate_proposal` 시그니처에도 동일 파라미터를 추가하고 `_build_prompt`로 전달한다:
```python
    def generate_proposal(
        self,
        machine_type: str,
        normalized_inputs: list[dict[str, Any]],
        process_conditions: ProcessConditionsSchema,
        similar_experiments: list[dict[str, Any]],
        derived_state: str = "solid",
        derived_category: str = "alloy",
    ) -> MaterialProposal:
        prompt = self._build_prompt(
            machine_type,
            normalized_inputs,
            process_conditions,
            similar_experiments,
            derived_state,
            derived_category,
        )
```
(이하 `try/except` 본문은 기존 유지.)

`get_fallback_proposal`에서 이름을 짧게 바꾸고 state를 추가한다:
- `name=f"{compound_name} 합금 (Fallback)"` → `name=f"{compound_name} 합금"`
- `MaterialProposalResult(...)` 인자에 `state="solid",` 추가

- [ ] **Step 4: 테스트 통과 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_proposal_generator.py -v`
Expected: PASS (2 passed)

- [ ] **Step 5: 린트 + 커밋**

```bash
cd backend && source .venv/bin/activate && ruff check --fix . && ruff format .
git add src/agents/material_generation/proposal_generator.py tests/agents/material_generation/test_proposal_generator.py
git commit -m "feat(material): 프롬프트 화학 명명 가이드·derived 힌트·fallback state 추가"
```

---

## Task 13: 그래프 배선 — derive 노드 + override + hash + state 전달

**Files:**
- Modify: `src/agents/material_generation/graph_state.py`
- Modify: `src/agents/material_generation/graph.py`
- Modify: `src/agents/material_generation/nodes.py`
- Modify: `tests/agents/material_generation/test_agent.py`

- [ ] **Step 1: 통합 테스트 보강** — `test_agent.py`의 `test_agent_synthesize_new_material_fallback_path` 마지막 assert 블록을 아래로 교체한다:
```python
        res = agent.synthesize(db_session, req)
        assert res.result_type == "new_material"
        assert res.material_id is not None
        assert res.name  # 비어있지 않음
        assert res.rarity in {"common", "uncommon", "rare", "epic"}
        assert res.state in {"solid", "liquid", "gas", "plasma"}
        assert res.visual_status == "pending"

        # 저장된 물질의 결정론 값 검증 (iron_ore+iron_ingot @ Synthesizer)
        stored = db_session.execute(
            select(GeneratedMaterialModel).where(
                GeneratedMaterialModel.id == res.material_id
            )
        ).scalar_one()
        assert stored.state == "solid"
        assert stored.rarity == "uncommon"
```
(`test_agent.py` 상단에 이미 `from sqlalchemy import func, select`가 있으므로 그대로 사용.)

- [ ] **Step 2: 테스트 실패 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_agent.py::test_agent_synthesize_new_material_fallback_path -v`
Expected: FAIL (derive 미배선·state 미저장으로 `stored.state`/`rarity` 불일치)

- [ ] **Step 3a: graph_state에 derived 키 추가**

`graph_state.py` import 블록에 추가:
```python
from agents.material_generation.derivation import DerivedAttributes
```
`MaterialGraphState`에 필드 추가:
```python
    derived: DerivedAttributes | None
```

- [ ] **Step 3b: nodes.py 수정**

상단 import 추가:
```python
from agents.material_generation.derivation import (
    DerivedAttributes,
    derive_material_attributes,
)
```
기존 `from agents.material_generation.schemas import (...)`에 `MaterialProposal`을 추가한다:
```python
from agents.material_generation.schemas import (
    MaterialCreationResponse,
    MaterialProposal,
    OutputItemSchema,
)
```

(1) override 헬퍼와 derive 노드를 `llm_propose_node` 정의 위에 추가:
```python
def _apply_derived(proposal: MaterialProposal, derived: DerivedAttributes) -> None:
    """LLM 제안의 구조화 값을 결정론 산출값으로 덮어씁니다 (이름·설명은 유지)."""
    if proposal.result is None:
        return
    proposal.result.properties = derived.properties
    proposal.result.category = derived.category
    proposal.result.state = derived.state
    proposal.result.rarity = derived.rarity


def derive_node(state: MaterialGraphState) -> dict[str, Any]:
    """노드: 입력·공정으로부터 속성·category·state·rarity를 결정론적으로 산출합니다."""
    if state.get("response"):
        return {}
    request = state["request"]
    normalized = state["normalized_inputs"]
    derived = derive_material_attributes(normalized, request.process_conditions)
    return {"derived": derived}
```

(2) `llm_propose_node`를 아래로 교체:
```python
def llm_propose_node(state: MaterialGraphState) -> dict[str, Any]:
    """노드: LLM으로 이름·설명을 제안받고 구조화 값을 derived로 덮어씁니다."""
    if state.get("response"):
        return {}

    request = state["request"]
    normalized = state["normalized_inputs"]
    similar_exps = state.get("similar_context") or []
    derived = state["derived"]
    assert derived is not None

    proposal = get_proposal_generator().generate_proposal(
        request.machine_type,
        normalized,
        request.process_conditions,
        similar_exps,
        derived_state=derived.state,
        derived_category=derived.category,
    )
    _apply_derived(proposal, derived)
    return {"proposal": proposal}
```

(3) `validate_result_node`의 fallback 분기에서 `proposal = get_proposal_generator().get_fallback_proposal(state["normalized_inputs"])` 다음 줄에 추가:
```python
        derived = state.get("derived")
        if derived is not None:
            _apply_derived(proposal, derived)
```

(4) `deduplicate_material_node`의 해시 계산을 교체:
기존
```python
    mat_hash = generate_material_hash(proposal.result)
```
→
```python
    mat_hash = generate_material_hash(
        request.machine_type, state["normalized_inputs"], request.process_conditions
    )
```
같은 함수의 `if existing_mat:` 분기에 `mat_state = existing_mat.state`를, `else:` 분기에 `mat_state = proposal.result.state`를 추가하고, `MaterialCreationResponse(...)` 생성 인자에 `state=mat_state,`를 추가한다.

(5) `lookup_cache_node`의 `cached_experiment` 분기에서 `fallback_icon = mat_model.fallback_icon if mat_model else None` 다음에 추가:
```python
            mat_state = mat_model.state if mat_model else None
```
그리고 해당 `MaterialCreationResponse(...)`에 `state=mat_state,`를 추가한다.

(6) `register_material_node`의 `GeneratedMaterialModel(...)` 생성 인자에 추가:
```python
            state=proposal.result.state,
```

- [ ] **Step 3c: graph.py에 derive 노드 배선**

`graph.py`의 노드 import에 `derive_node`를 추가하고, 노드 등록부에 추가:
```python
    builder.add_node("derive", derive_node)
```
기존 엣지
```python
    builder.add_edge("similarity_context", "llm_propose")
```
를 아래로 교체:
```python
    builder.add_edge("similarity_context", "derive")
    builder.add_edge("derive", "llm_propose")
```

- [ ] **Step 4: 통합 테스트 통과 확인**

Run: `cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation/test_agent.py -v`
Expected: PASS (전체 통과)

- [ ] **Step 5: 전체 스위트 + 린트 + 커밋**

```bash
cd backend && source .venv/bin/activate && python -m pytest tests/agents/material_generation -v
```
Expected: 전체 PASS
```bash
cd backend && source .venv/bin/activate && ruff check --fix . && ruff format .
git add src/agents/material_generation/graph_state.py src/agents/material_generation/graph.py src/agents/material_generation/nodes.py tests/agents/material_generation/test_agent.py
git commit -m "feat(material): derive 노드 배선 및 결정론 값 override·state 전달"
```

---

## Task 14: 회귀 검증 + 문서 동기화

- [ ] **Step 1: 백엔드 전체 테스트 실행**

Run: `cd backend && source .venv/bin/activate && python -m pytest -q`
Expected: 전체 PASS (기존 회귀 없음)

- [ ] **Step 2: 아키텍처 문서 동기화**

`docs/03_architecture/material_generation_current_structure.md`에 derivation 단계(속성·category·state·rarity 결정론 산출)와 `material_hash` 합성 정체성 재정의, `state` 필드를 반영하는 단락을 추가한다.

- [ ] **Step 3: 커밋**

```bash
git add docs/03_architecture/material_generation_current_structure.md
git commit -m "docs: 결정론 산출·state 반영 (아키텍처 문서 동기화)"
```

---

## 완료 기준

- 같은 입력(장비+재료+공정) → 항상 동일한 속성·category·state·rarity·material_hash
- 이름은 LLM 생성 + 화학물질 스타일, 검증 규칙(길이 2~24·특수문자 제거·금지어·fallback) 통과
- `state`가 응답·DB에 영속되고 plasma 포함 4종으로 산출
- 백엔드 전체 테스트 통과, ruff 클린
