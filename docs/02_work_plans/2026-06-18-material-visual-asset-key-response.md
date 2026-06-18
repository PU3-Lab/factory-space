# Material Visual Asset Key Response 구현 계획

> **agentic worker 필수 지침:** 이 계획을 구현할 때는 superpowers:subagent-driven-development(권장) 또는 superpowers:executing-plans를 사용해 작업 단위별로 진행한다. 단계 추적은 체크박스(`- [ ]`) 문법을 사용한다.

**목표:** material_generation 이미지 생성이 완료된 뒤 클라이언트가 자체 asset 로더에서 사용할 수 있는 asset key를 결과에 함께 반환한다.

**아키텍처:** 이미지 파일을 HTTP로 직접 받는 계약은 만들지 않는다. 백엔드는 DB에 저장된 `materials/{material_id}/icon.png` 같은 asset key만 반환하고, 실제 로딩 경로 변환은 클라이언트 또는 게임 런타임의 asset resolver가 담당한다. 최초 합성 응답은 기존처럼 `visual_status="pending"`이며 asset key는 비어 있고, 백그라운드 이미지 생성 완료 후 visual-status 조회와 cached experiment 응답에서 key가 채워진다.

**기술 스택:** Python 3.12, FastAPI, Pydantic v2, SQLAlchemy, pytest

---

## 파일 맵

| 작업 | 경로 | 목적 |
|------|------|------|
| 수정 | `backend/src/agents/material_generation/schemas.py` | `MaterialCreationResponse`에 선택적 visual asset key 필드 추가 |
| 수정 | `backend/tests/agents/material_generation/test_schemas_state.py` | asset key 필드 기본값이 `None`인지 검증 |
| 수정 | `backend/src/agents/material_generation/nodes.py` | visual-ready인 cached/new-material 응답에 asset key 포함 |
| 수정 | `backend/tests/agents/material_generation/test_agent.py` | cached experiment가 완료된 asset key를 반환하는지 회귀 테스트 |
| 생성 | `backend/tests/agents/material_generation/test_visual_status_router.py` | visual-status 엔드포인트가 asset key 계약을 유지하는지 테스트 |
| 수정 | `docs/03_architecture/material_generation_current_structure.md` | HTTP URL이 아니라 asset key를 반환한다는 계약 문서화 |

---

## 응답 계약

`MaterialCreationResponse`에 다음 선택 필드를 추가한다.

```python
visual_asset_key: str | None = None
texture_asset_key: str | None = None
thumbnail_asset_key: str | None = None
```

기존 `fallback_icon`은 유지한다.

규칙:

- 새 재료 최초 생성 응답은 `visual_status == "pending"`이며 `visual_asset_key`, `texture_asset_key`, `thumbnail_asset_key`는 `None`이다.
- 이미지 생성이 완료되면 DB의 `GeneratedMaterialModel.visual_asset_key`, `texture_asset_key`, `thumbnail_asset_key`가 채워진다.
- `GET /api/v1/materials/{material_id}/visual-status`는 현재처럼 asset key를 반환한다.
- 같은 실험을 다시 실행해 cached experiment가 반환될 때, 해당 material이 이미 `visual_ready`이면 `MaterialCreationResponse`에도 asset key를 포함한다.
- HTTP URL, public URL, 정적 파일 URL 필드는 추가하지 않는다.

---

## 결과 예시

### 1. 최초 생성 직후

```json
{
  "result_type": "new_material",
  "experiment_hash": "abc123",
  "material_id": "mat_iron_alloy_a1b2c3",
  "material_hash": "hash123",
  "name": "Iron Alloy",
  "rarity": "uncommon",
  "state": "solid",
  "generation_status": "created",
  "visual_status": "pending",
  "fallback_icon": "materials/default/alloy.png",
  "visual_asset_key": null,
  "texture_asset_key": null,
  "thumbnail_asset_key": null,
  "message": "새로운 물질이 발견되었습니다. 아이콘과 텍스처는 생성 중입니다."
}
```

### 2. 이미지 생성 완료 후 visual-status 조회

```json
{
  "material_id": "mat_iron_alloy_a1b2c3",
  "visual_status": "visual_ready",
  "visual_asset_key": "materials/mat_iron_alloy_a1b2c3/icon.png",
  "texture_asset_key": "materials/mat_iron_alloy_a1b2c3/texture.png",
  "thumbnail_asset_key": "materials/mat_iron_alloy_a1b2c3/thumbnail.png"
}
```

### 3. 같은 실험을 다시 실행해 cached experiment가 반환되는 경우

```json
{
  "result_type": "cached_experiment",
  "experiment_hash": "abc123",
  "cached": true,
  "material_id": "mat_iron_alloy_a1b2c3",
  "material_hash": "hash123",
  "name": "Iron Alloy",
  "rarity": "uncommon",
  "state": "solid",
  "generation_status": "cached",
  "visual_status": "visual_ready",
  "fallback_icon": "materials/default/alloy.png",
  "visual_asset_key": "materials/mat_iron_alloy_a1b2c3/icon.png",
  "texture_asset_key": "materials/mat_iron_alloy_a1b2c3/texture.png",
  "thumbnail_asset_key": "materials/mat_iron_alloy_a1b2c3/thumbnail.png",
  "message": "이미 발견된 물질입니다."
}
```

---

## Task 1: 응답 스키마에 asset key 필드 추가

**Files:**
- Modify: `backend/src/agents/material_generation/schemas.py`
- Modify: `backend/tests/agents/material_generation/test_schemas_state.py`

- [ ] **Step 1: 실패하는 스키마 테스트 작성**

`backend/tests/agents/material_generation/test_schemas_state.py`에 아래 테스트를 추가한다.

```python
def test_response_visual_asset_key_fields_optional() -> None:
    """MaterialCreationResponse 생성 시 visual asset key 필드는 기본적으로 None이어야 합니다."""
    resp = MaterialCreationResponse(result_type="new_material", experiment_hash="h")

    assert resp.visual_asset_key is None
    assert resp.texture_asset_key is None
    assert resp.thumbnail_asset_key is None
```

- [ ] **Step 2: RED 확인**

Run:

```bash
cd backend && python -m pytest tests/agents/material_generation/test_schemas_state.py::test_response_visual_asset_key_fields_optional -v
```

Expected: 필드가 아직 없어서 `AttributeError`로 실패한다.

- [ ] **Step 3: 최소 구현 작성**

`backend/src/agents/material_generation/schemas.py`의 `MaterialCreationResponse`에서 `fallback_icon` 아래에 필드를 추가한다.

```python
    visual_status: str | None = None
    fallback_icon: str | None = None
    visual_asset_key: str | None = None
    texture_asset_key: str | None = None
    thumbnail_asset_key: str | None = None
    message: str | None = None
```

- [ ] **Step 4: GREEN 확인**

Run:

```bash
cd backend && python -m pytest tests/agents/material_generation/test_schemas_state.py -v
```

Expected: PASS.

- [ ] **Step 5: 커밋**

```bash
git add backend/src/agents/material_generation/schemas.py \
        backend/tests/agents/material_generation/test_schemas_state.py
git commit -m "feat: add material visual asset key response fields"
```

---

## Task 2: cached experiment 응답에 완료 asset key 포함

**Files:**
- Modify: `backend/src/agents/material_generation/nodes.py`
- Modify: `backend/tests/agents/material_generation/test_agent.py`

- [ ] **Step 1: 실패하는 cached response 테스트 작성**

`backend/tests/agents/material_generation/test_agent.py`에 아래 테스트를 추가한다.

```python
def test_agent_synthesize_cached_experiment_returns_ready_visual_asset_keys(
    db_session: Session,
) -> None:
    agent = MaterialCreationAgent()
    req = MaterialCreationRequest(
        machine_type="Synthesizer",
        inputs=[
            InputItemSchema(item_id="iron_ore", qty=1),
            InputItemSchema(item_id="iron_ingot", qty=1),
        ],
        player_id="player_visual_asset_key_test",
        generate_visual_asset=False,
    )

    with (
        patch("llm.adapter.GoogleGenAiLLMAdapter.invoke", return_value=None),
        patch("llm.adapter.OpenAILLMAdapter.invoke", return_value=None),
        patch("llm.adapter.LocalLLMAdapter.invoke", return_value=None),
    ):
        first = agent.synthesize(db_session, req)

    assert first.result_type == "new_material"
    assert first.material_id is not None

    stored = db_session.get(GeneratedMaterialModel, first.material_id)
    assert stored is not None
    stored.visual_status = "visual_ready"
    stored.visual_asset_key = f"materials/{first.material_id}/icon.png"
    stored.texture_asset_key = f"materials/{first.material_id}/texture.png"
    stored.thumbnail_asset_key = f"materials/{first.material_id}/thumbnail.png"
    db_session.commit()

    cached = agent.synthesize(db_session, req)

    assert cached.result_type == "cached_experiment"
    assert cached.material_id == first.material_id
    assert cached.visual_status == "visual_ready"
    assert cached.visual_asset_key == f"materials/{first.material_id}/icon.png"
    assert cached.texture_asset_key == f"materials/{first.material_id}/texture.png"
    assert cached.thumbnail_asset_key == (
        f"materials/{first.material_id}/thumbnail.png"
    )
```

- [ ] **Step 2: RED 확인**

Run:

```bash
cd backend && python -m pytest tests/agents/material_generation/test_agent.py::test_agent_synthesize_cached_experiment_returns_ready_visual_asset_keys -v
```

Expected: cached `MaterialCreationResponse`가 asset key 필드를 채우지 않아서 실패한다.

- [ ] **Step 3: `lookup_cache_node`에 asset key 연결**

`backend/src/agents/material_generation/nodes.py`의 `lookup_cache_node`에서 `mat_model`을 읽는 구간에 아래 변수를 추가한다.

```python
            visual_asset_key = mat_model.visual_asset_key if mat_model else None
            texture_asset_key = mat_model.texture_asset_key if mat_model else None
            thumbnail_asset_key = mat_model.thumbnail_asset_key if mat_model else None
```

cached response 생성 시 아래 필드를 전달한다.

```python
                visual_asset_key=visual_asset_key,
                texture_asset_key=texture_asset_key,
                thumbnail_asset_key=thumbnail_asset_key,
```

- [ ] **Step 4: 대상 테스트 GREEN 확인**

Run:

```bash
cd backend && python -m pytest tests/agents/material_generation/test_agent.py::test_agent_synthesize_cached_experiment_returns_ready_visual_asset_keys -v
```

Expected: PASS.

- [ ] **Step 5: 커밋**

```bash
git add backend/src/agents/material_generation/nodes.py \
        backend/tests/agents/material_generation/test_agent.py
git commit -m "feat: include visual asset keys in cached material responses"
```

---

## Task 3: visual-status asset key 계약 회귀 테스트

**Files:**
- Create: `backend/tests/agents/material_generation/test_visual_status_router.py`

- [ ] **Step 1: visual-status 응답 테스트 작성**

`backend/tests/agents/material_generation/test_visual_status_router.py`를 생성한다.

```python
"""material visual status API asset key 응답 통합 테스트입니다."""

from __future__ import annotations

from collections.abc import Iterator
from contextlib import contextmanager
from unittest.mock import patch

from fastapi.testclient import TestClient
from sqlalchemy.orm import Session

from app import create_app
from db.models import GeneratedMaterialModel


def test_visual_status_returns_asset_keys(db_session: Session) -> None:
    db_session.add(
        GeneratedMaterialModel(
            id="mat_ready_001",
            material_hash="hash_mat_ready_001",
            name="Ready Alloy",
            category="alloy",
            rarity="common",
            properties_json={},
            visual_status="visual_ready",
            visual_asset_key="materials/mat_ready_001/icon.png",
            texture_asset_key="materials/mat_ready_001/texture.png",
            thumbnail_asset_key="materials/mat_ready_001/thumbnail.png",
            fallback_icon="materials/default/alloy.png",
        )
    )
    db_session.commit()

    @contextmanager
    def get_test_db_session() -> Iterator[Session]:
        yield db_session

    with patch("agents.material_generation.router.get_db_session", get_test_db_session):
        with TestClient(create_app()) as client:
            response = client.get(
                "/api/v1/materials/mat_ready_001/visual-status"
            )

    assert response.status_code == 200
    assert response.json() == {
        "material_id": "mat_ready_001",
        "visual_status": "visual_ready",
        "visual_asset_key": "materials/mat_ready_001/icon.png",
        "texture_asset_key": "materials/mat_ready_001/texture.png",
        "thumbnail_asset_key": "materials/mat_ready_001/thumbnail.png",
    }
```

- [ ] **Step 2: 테스트 실행**

Run:

```bash
cd backend && python -m pytest tests/agents/material_generation/test_visual_status_router.py -v
```

Expected: 현재 구현이 이미 asset key를 반환하므로 PASS해야 한다. 실패하면 기존 endpoint 계약이 깨진 것이므로 실제 응답 필드를 확인해 테스트와 문서를 맞춘다.

- [ ] **Step 3: 커밋**

```bash
git add backend/tests/agents/material_generation/test_visual_status_router.py
git commit -m "test: lock material visual status asset key response"
```

---

## Task 4: 아키텍처 문서 업데이트

**Files:**
- Modify: `docs/03_architecture/material_generation_current_structure.md`

- [ ] **Step 1: 응답 테이블 업데이트**

`MaterialCreationResponse` 응답 payload 섹션에 아래 행을 추가한다.

```markdown
| `visual_asset_key` | `str \| null` | 아니오 | `visual_status == "visual_ready"`인 cached material의 아이콘 asset key |
| `texture_asset_key` | `str \| null` | 아니오 | `visual_status == "visual_ready"`인 cached material의 텍스처 asset key |
| `thumbnail_asset_key` | `str \| null` | 아니오 | `visual_status == "visual_ready"`인 cached material의 썸네일 asset key |
```

- [ ] **Step 2: 이미지 로딩 계약 설명 추가**

visual asset 섹션에 아래 문장을 추가한다.

```markdown
이미지는 HTTP URL 계약으로 반환하지 않는다. 백엔드는 `materials/{material_id}/icon.png`
형태의 asset key만 반환하며, 실제 로딩 경로 또는 런타임 asset 참조로 변환하는 책임은
클라이언트의 asset resolver에 있다.
```

- [ ] **Step 3: 문서 diff 확인**

Run:

```bash
git diff -- docs/03_architecture/material_generation_current_structure.md
```

Expected: asset key 응답 필드와 non-HTTP 이미지 로딩 계약 설명만 변경되어 있다.

- [ ] **Step 4: 커밋**

```bash
git add docs/03_architecture/material_generation_current_structure.md
git commit -m "docs: document material visual asset key response"
```

---

## Task 5: 최종 검증

**Files:**
- Verify: Task 1-4에서 변경한 모든 파일

- [ ] **Step 1: material_generation 집중 테스트 실행**

```bash
cd backend && python -m pytest tests/agents/material_generation/ -v --tb=short
```

Expected: PASS.

- [ ] **Step 2: visual 테스트 실행**

```bash
cd backend && python -m pytest tests/visual/ -v --tb=short
```

Expected: PASS.

- [ ] **Step 3: lint 실행**

```bash
cd backend && ruff check src tests
```

Expected: PASS.

- [ ] **Step 4: 최종 diff 검토**

```bash
git diff --stat HEAD
git diff HEAD -- backend/src/agents/material_generation \
                 backend/tests/agents/material_generation \
                 docs/03_architecture/material_generation_current_structure.md
```

Expected: 응답 스키마, cached response 연결, 테스트, 문서 변경으로 범위가 제한되어 있다.

---

## 자체 점검

- 요구사항 커버리지: HTTP URL을 쓰지 않고 asset key만 반환하는 계약으로 수정했다.
- 미작성 항목 점검: placeholder marker나 구체성 없는 "테스트 작성" 단계가 남아 있지 않다.
- 타입 일관성: schema, nodes, tests, docs에서 필드명을 `visual_asset_key`, `texture_asset_key`, `thumbnail_asset_key`로 통일했다.
- 범위 점검: 이미지 생성 방식, 저장 방식, DB 스키마, 최초 pending 응답 생명주기는 변경하지 않는다.
