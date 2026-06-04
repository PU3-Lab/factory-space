# Quest RAG Reference Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `data/game/recipes.csv` 같은 게임 데이터 문서를 근거로 검색한 뒤, 그 근거를 사용해 퀘스트를 작성하는 퀘스트 생성 보조 흐름을 만든다.

**Architecture:** 1차 구현은 벡터 DB 없이 CSV를 구조화해 읽고, 키워드/ID 기반 검색 결과를 퀘스트 생성 서비스에 주입한다. 검색 결과는 `sources`와 `metadata.retrieval`에 남겨 나중에 실제 RAG, 임베딩, pgvector로 교체했는지 검증할 수 있게 한다.

**Tech Stack:** Python, Pydantic, pytest, existing backend agent pipeline, CSV files under `data/game`.

---

## 전제와 범위

- 현재 퀘스트 생성기는 `backend/src/agents/quest_generator/service.py`의 서버 내부 예시 퀘스트 풀에서 5개를 뽑는다.
- 현재 CSV 기반 수동 Q&A는 `backend/src/agents/operator_guide/csv_repository.py`가 `data/game/*.csv`를 직접 읽는다.
- 이번 계획은 퀘스트 생성기가 게임 데이터 근거를 참고하도록 만드는 계획이다. 첫 단계에서는 외부 벡터 DB, 임베딩 API, PostgreSQL, pgvector를 추가하지 않는다.
- RAG라는 이름을 쓰되, 첫 구현의 성공 기준은 "검색된 근거를 퀘스트 생성에 사용하고 응답에 근거를 남긴다"이다.
- LLM 생성은 후속 확장이다. 현재는 결정적 템플릿 생성으로 테스트 가능한 기반을 먼저 만든다.

## 접근 방식 선택

추천 접근: CSV 구조화 검색 기반의 경량 RAG

- 장점: 현재 데이터와 테스트 구조에 바로 맞고, 네트워크/API 키 없이 검증 가능하다.
- 단점: 의미 기반 검색은 약하다.
- 채택 이유: 퀘스트 생성의 첫 목표는 데이터 무결성과 재현 가능한 결과이므로, 임베딩보다 CSV 근거 연결이 먼저다.

대안 1: pgvector/임베딩 RAG를 즉시 도입

- 장점: 자연어 검색 확장성이 좋다.
- 단점: DB 마이그레이션, 임베딩 모델, 색인 갱신, CI 환경 구성이 커진다.
- 이번 범위에서는 제외한다.

대안 2: LLM 프롬프트에 CSV 전문을 직접 주입

- 장점: 구현이 빠르다.
- 단점: 토큰 낭비가 크고 근거 추적 및 테스트가 어렵다.
- 이번 범위에서는 제외한다.

## 파일 구조

- Create: `backend/src/agents/quest_generator/knowledge.py`
  - 퀘스트 생성에 필요한 CSV 행을 `QuestKnowledgeEntry`로 정규화한다.
  - recipe, resource, equipment, issue 근거를 하나의 검색 결과 타입으로 묶는다.
- Create: `backend/src/agents/quest_generator/retriever.py`
  - `CsvManualQARepository`를 사용해 퀘스트 후보 근거를 조회한다.
  - 1차 구현은 생산 레시피 전체를 후보로 만들고, 출력 자원/장비/병목 정보를 함께 반환한다.
- Modify: `backend/src/agents/quest_generator/schemas.py`
  - 퀘스트 응답에 선택적으로 `sources`와 `metadata`를 추가한다.
  - 기존 클라이언트 계약인 `payload.quests`는 유지한다.
- Modify: `backend/src/agents/quest_generator/service.py`
  - 고정 예시 풀 대신 검색된 레시피 근거에서 생산 퀘스트를 만든다.
  - 검색 실패 시 기존 예시 풀 fallback을 유지한다.
- Modify: `backend/src/agents/quest_generator/production_quest.py`
  - fallback 응답에도 retrieval metadata가 포함되도록 `QuestAgentService` 결과를 그대로 반환한다.
- Test: `backend/tests/test_quest_agent_retriever.py`
  - CSV 근거 검색 단위 테스트.
- Test: `backend/tests/test_quest_agent_service.py`
  - 퀘스트가 CSV 레시피 근거를 사용하고 `metadata.retrieval`을 남기는지 검증.
- Test: `backend/tests/test_pipeline_edges.py`
  - 기존 `quest_generator.production_quest` 경로가 새 payload shape에서도 유지되는지 검증.

## 완료 체크리스트

- [ ] CSV 기반 퀘스트 검색 전용 모듈이 추가되었다.
- [ ] `recipes.csv`의 `recipe_id`, `output_resource`, `required_equipment`, `production_steps`, `common_bottlenecks`가 퀘스트 생성 근거로 사용된다.
- [ ] `resources.csv`의 자원 이름이 퀘스트 제목/설명에 사용된다.
- [ ] `equipment.csv`의 장비 이름이 퀘스트 설명에 사용된다.
- [ ] `troubleshooting_rules.csv`의 병목 정보가 퀘스트 설명이나 metadata에 연결된다.
- [ ] 응답의 기존 필드 `payload.quests`가 유지된다.
- [ ] 응답에 `metadata.retrieval.sources` 또는 동등한 근거 추적 필드가 포함된다.
- [ ] 검색 실패 시 기존 예시 퀘스트 fallback이 동작한다.
- [ ] 새 단위 테스트가 실패 상태에서 시작한 뒤 구현 후 통과했다.
- [ ] 전체 backend pytest가 통과했다.
- [ ] smoke script 또는 WebSocket 경로에서 quest 응답이 5개 반환되는지 확인했다.
- [ ] 문서 마지막의 자체 점검 표가 실제 구현 결과로 갱신되었다.

## Task 1: 퀘스트 지식 엔트리 타입 추가

**Files:**
- Create: `backend/src/agents/quest_generator/knowledge.py`
- Test: `backend/tests/test_quest_agent_retriever.py`

- [ ] **Step 1: 실패하는 테스트 작성**

```python
from __future__ import annotations

from agents.quest_generator.knowledge import QuestKnowledgeEntry, QuestKnowledgeSource


def test_quest_knowledge_entry_serializes_source_ids() -> None:
    entry = QuestKnowledgeEntry(
        recipe_id="recipe_gear",
        title="기어 제작 공정",
        target_item_id="resource_gear",
        target_item_name="기어",
        required_equipment_id="equipment_assembler",
        required_equipment_name="조립기",
        production_steps="철괴로 기어 제작",
        bottleneck_issue_ids=["issue_production_bottleneck"],
        sources=[
            QuestKnowledgeSource(
                source_id="recipe_gear",
                source_type="recipe",
                title="기어 제작 공정",
            )
        ],
    )

    assert entry.source_ids == ["recipe_gear"]
```

- [ ] **Step 2: 실패 확인**

Run:

```bash
cd backend
python -m pytest tests/test_quest_agent_retriever.py::test_quest_knowledge_entry_serializes_source_ids -v
```

Expected: `ModuleNotFoundError` 또는 `ImportError`로 실패한다.

- [ ] **Step 3: 최소 구현 작성**

`backend/src/agents/quest_generator/knowledge.py`:

```python
"""Knowledge records used by quest generation."""

from __future__ import annotations

from pydantic import BaseModel, ConfigDict, computed_field


class QuestKnowledgeSource(BaseModel):
    """One source row used to justify a generated quest."""

    model_config = ConfigDict(extra="forbid")

    source_id: str
    source_type: str
    title: str


class QuestKnowledgeEntry(BaseModel):
    """Normalized game-data fact used to build one production quest."""

    model_config = ConfigDict(extra="forbid")

    recipe_id: str
    title: str
    target_item_id: str
    target_item_name: str
    required_equipment_id: str
    required_equipment_name: str
    production_steps: str
    bottleneck_issue_ids: list[str]
    sources: list[QuestKnowledgeSource]

    @computed_field
    @property
    def source_ids(self) -> list[str]:
        return [source.source_id for source in self.sources]
```

- [ ] **Step 4: 통과 확인**

Run:

```bash
cd backend
python -m pytest tests/test_quest_agent_retriever.py::test_quest_knowledge_entry_serializes_source_ids -v
```

Expected: `1 passed`.

## Task 2: CSV 기반 퀘스트 검색기 추가

**Files:**
- Create: `backend/src/agents/quest_generator/retriever.py`
- Modify: `backend/tests/test_quest_agent_retriever.py`

- [ ] **Step 1: 실패하는 검색 테스트 작성**

`backend/tests/test_quest_agent_retriever.py`에 추가:

```python
from agents.quest_generator.retriever import CsvQuestKnowledgeRetriever


def test_retriever_returns_recipe_backed_quest_knowledge() -> None:
    retriever = CsvQuestKnowledgeRetriever()

    entries = retriever.production_entries()

    gear_entry = next(entry for entry in entries if entry.recipe_id == "recipe_gear")
    assert gear_entry.target_item_id == "resource_gear"
    assert gear_entry.target_item_name == "기어"
    assert gear_entry.required_equipment_name == "조립기"
    assert "recipe_gear" in gear_entry.source_ids
    assert "resource_gear" in gear_entry.source_ids
    assert "equipment_assembler" in gear_entry.source_ids
```

- [ ] **Step 2: 실패 확인**

Run:

```bash
cd backend
python -m pytest tests/test_quest_agent_retriever.py -v
```

Expected: `CsvQuestKnowledgeRetriever` import 실패 또는 구현 부재로 실패한다.

- [ ] **Step 3: 최소 구현 작성**

`backend/src/agents/quest_generator/retriever.py`:

```python
"""CSV-backed quest knowledge retrieval."""

from __future__ import annotations

from agents.operator_guide.csv_repository import CsvManualQARepository
from agents.quest_generator.knowledge import QuestKnowledgeEntry, QuestKnowledgeSource


class CsvQuestKnowledgeRetriever:
    """Build production quest knowledge from existing game CSV files."""

    def __init__(self, repository: CsvManualQARepository | None = None) -> None:
        self._repository = repository or CsvManualQARepository()

    def production_entries(self) -> list[QuestKnowledgeEntry]:
        entries: list[QuestKnowledgeEntry] = []
        for recipe_id in [
            "recipe_iron_ingot",
            "recipe_copper_ingot",
            "recipe_gear",
            "recipe_wire",
            "recipe_basic_motor",
        ]:
            recipe = self._repository.get_recipe(recipe_id)
            if recipe is None:
                continue

            resource = self._repository.get_resource(recipe.output_resource)
            equipment = self._repository.get_equipment(recipe.required_equipment)
            if resource is None or equipment is None:
                continue

            sources = [
                QuestKnowledgeSource(
                    source_id=recipe.recipe_id,
                    source_type="recipe",
                    title=recipe.name,
                ),
                QuestKnowledgeSource(
                    source_id=resource.resource_id,
                    source_type="resource",
                    title=resource.name,
                ),
                QuestKnowledgeSource(
                    source_id=equipment.equipment_id,
                    source_type="equipment",
                    title=equipment.name,
                ),
            ]

            entries.append(
                QuestKnowledgeEntry(
                    recipe_id=recipe.recipe_id,
                    title=recipe.name,
                    target_item_id=resource.resource_id,
                    target_item_name=resource.name,
                    required_equipment_id=equipment.equipment_id,
                    required_equipment_name=equipment.name,
                    production_steps=recipe.production_steps,
                    bottleneck_issue_ids=recipe.common_bottlenecks,
                    sources=sources,
                )
            )
        return entries
```

- [ ] **Step 4: 통과 확인**

Run:

```bash
cd backend
python -m pytest tests/test_quest_agent_retriever.py -v
```

Expected: `2 passed`.

## Task 3: 퀘스트 응답에 근거 metadata 추가

**Files:**
- Modify: `backend/src/agents/quest_generator/schemas.py`
- Modify: `backend/tests/test_quest_agent_service.py`

- [ ] **Step 1: 실패하는 schema 테스트 작성**

`backend/tests/test_quest_agent_service.py`에 추가:

```python
def test_quest_response_allows_retrieval_metadata() -> None:
    response = QuestResponse.model_validate(
        {
            "quests": [
                {
                    "id": 1,
                    "type": "production",
                    "title": "기어 4개 제작",
                    "description": "조립기에서 기어를 제작하세요.",
                    "objectives": [
                        {"target_item_id": "resource_gear", "quantity": 4}
                    ],
                }
            ],
            "metadata": {
                "retrieval": {
                    "mode": "csv_structured",
                    "sources": [
                        {
                            "source_id": "recipe_gear",
                            "source_type": "recipe",
                            "title": "기어 제작 공정",
                        }
                    ],
                }
            },
        }
    )

    assert response.metadata["retrieval"]["mode"] == "csv_structured"
```

- [ ] **Step 2: 실패 확인**

Run:

```bash
cd backend
python -m pytest tests/test_quest_agent_service.py::test_quest_response_allows_retrieval_metadata -v
```

Expected: `metadata` 필드 미정의 또는 extra field 처리 문제로 실패한다.

- [ ] **Step 3: schema 최소 수정**

`backend/src/agents/quest_generator/schemas.py`의 `QuestResponse`를 다음 형태로 확장한다.

```python
class QuestResponse(BaseModel):
    """Top-level quest response payload."""

    quests: list[Quest] = Field(min_length=1)
    metadata: dict[str, object] = Field(default_factory=dict)
```

- [ ] **Step 4: 통과 확인**

Run:

```bash
cd backend
python -m pytest tests/test_quest_agent_service.py::test_quest_response_allows_retrieval_metadata -v
```

Expected: `1 passed`.

## Task 4: 검색 근거 기반 퀘스트 생성으로 서비스 확장

**Files:**
- Modify: `backend/src/agents/quest_generator/service.py`
- Modify: `backend/tests/test_quest_agent_service.py`

- [ ] **Step 1: 실패하는 서비스 테스트 작성**

`backend/tests/test_quest_agent_service.py`에 추가:

```python
def test_service_generates_quests_from_csv_knowledge() -> None:
    service = QuestAgentService(rng=random.Random(0))

    result = service.generate_quest_json()

    QuestResponse.model_validate(result)
    assert len(result["quests"]) == 5
    assert result["metadata"]["retrieval"]["mode"] == "csv_structured"
    assert result["metadata"]["retrieval"]["source_count"] >= 5
    assert any(
        objective["target_item_id"] == "resource_gear"
        for quest in result["quests"]
        for objective in quest["objectives"]
    )
    assert any("조립기" in quest["description"] for quest in result["quests"])
```

- [ ] **Step 2: 실패 확인**

Run:

```bash
cd backend
python -m pytest tests/test_quest_agent_service.py::test_service_generates_quests_from_csv_knowledge -v
```

Expected: 기존 예시 풀에 `metadata.retrieval`이 없어서 실패한다.

- [ ] **Step 3: 서비스 구현 수정**

`backend/src/agents/quest_generator/service.py` 변경 방향:

```python
from agents.quest_generator.retriever import CsvQuestKnowledgeRetriever


class QuestAgentService:
    """Generate validated quests from retrieved game knowledge."""

    def __init__(
        self,
        rng: random.Random | None = None,
        retriever: CsvQuestKnowledgeRetriever | None = None,
    ) -> None:
        self._rng = rng or random.SystemRandom()
        self._retriever = retriever or CsvQuestKnowledgeRetriever()

    def generate_quest_json(self, count: int = 5) -> dict[str, Any]:
        """Return JSON-serializable quests after Pydantic validation."""

        entries = self._retriever.production_entries()
        if len(entries) < count:
            return self._generate_fallback_quest_json(count)

        selected_entries = self._rng.sample(entries, k=count)
        quests = [
            Quest.model_validate(
                {
                    "id": index,
                    "type": "production",
                    "title": f"{entry.target_item_name} 생산",
                    "description": (
                        f"{entry.required_equipment_name}를 사용해 "
                        f"{entry.target_item_name}을 생산하세요. "
                        f"공정 순서: {entry.production_steps}"
                    ),
                    "objectives": [
                        {
                            "target_item_id": entry.target_item_id,
                            "quantity": 4,
                        }
                    ],
                }
            )
            for index, entry in enumerate(selected_entries, start=1)
        ]
        sources = [
            source.model_dump()
            for entry in selected_entries
            for source in entry.sources
        ]
        return QuestResponse(
            quests=quests,
            metadata={
                "retrieval": {
                    "mode": "csv_structured",
                    "source_count": len(sources),
                    "sources": sources,
                }
            },
        ).model_dump(mode="json")
```

기존 예시 풀 fallback은 `_generate_fallback_quest_json()` private 메서드로 이동한다. fallback 응답에도 다음 metadata를 포함한다.

```python
"metadata": {
    "retrieval": {
        "mode": "fallback_examples",
        "source_count": 0,
        "sources": [],
    }
}
```

- [ ] **Step 4: 서비스 테스트 통과 확인**

Run:

```bash
cd backend
python -m pytest tests/test_quest_agent_service.py -v
```

Expected: 기존 테스트와 새 테스트가 모두 통과한다.

## Task 5: 검색 실패 fallback 테스트 추가

**Files:**
- Modify: `backend/tests/test_quest_agent_service.py`

- [ ] **Step 1: 실패하는 fallback 테스트 작성**

```python
class EmptyQuestKnowledgeRetriever:
    def production_entries(self) -> list[object]:
        return []


def test_service_falls_back_when_retrieval_has_too_few_entries() -> None:
    service = QuestAgentService(
        rng=random.Random(0),
        retriever=EmptyQuestKnowledgeRetriever(),
    )

    result = service.generate_quest_json()

    QuestResponse.model_validate(result)
    assert len(result["quests"]) == 5
    assert result["metadata"]["retrieval"]["mode"] == "fallback_examples"
```

- [ ] **Step 2: 실패 확인**

Run:

```bash
cd backend
python -m pytest tests/test_quest_agent_service.py::test_service_falls_back_when_retrieval_has_too_few_entries -v
```

Expected: retriever 주입 또는 fallback metadata가 없어서 실패한다.

- [ ] **Step 3: Task 4의 retriever 주입과 fallback 구현을 완료한다**

Task 4에서 작성한 `QuestAgentService.__init__()`와 `_generate_fallback_quest_json()`을 사용한다.

- [ ] **Step 4: 통과 확인**

Run:

```bash
cd backend
python -m pytest tests/test_quest_agent_service.py::test_service_falls_back_when_retrieval_has_too_few_entries -v
```

Expected: `1 passed`.

## Task 6: 파이프라인 계약 유지 검증

**Files:**
- Modify: `backend/tests/test_pipeline_edges.py`

- [ ] **Step 1: 기존 quest pipeline 테스트에 metadata 검증 추가**

기존 `test_pipeline_routes_production_quest_from_llm_leaf_decision`의 마지막에 추가한다.

```python
    assert response["payload"]["metadata"]["retrieval"]["mode"] in {
        "csv_structured",
        "fallback_examples",
    }
```

- [ ] **Step 2: 테스트 실행**

Run:

```bash
cd backend
python -m pytest tests/test_pipeline_edges.py::test_pipeline_routes_production_quest_from_llm_leaf_decision -v
```

Expected: `1 passed`.

- [ ] **Step 3: 빈 payload quest 요청 경로도 확인**

Run:

```bash
cd backend
python -m pytest tests/test_pipeline_edges.py::test_pipeline_routes_empty_quest_request_without_llm -v
```

Expected: `1 passed`. 실패하면 해당 테스트 이름을 `rg -n "empty_quest|without_llm" tests`로 찾아 실제 이름에 맞춰 실행한다.

## Task 7: smoke 검증

**Files:**
- Modify only if needed: `backend/scripts/smoke_agent_pipeline.py`
- Modify only if needed: `backend/tests/test_smoke_agent_pipeline_script.py`

- [ ] **Step 1: smoke script의 quest count 검증 유지 확인**

Run:

```bash
cd backend
python -m pytest tests/test_smoke_agent_pipeline_script.py -v
```

Expected: 전체 통과.

- [ ] **Step 2: 로컬 서버 smoke 실행**

서버를 별도 터미널에서 실행한다.

```bash
cd backend
python scripts/run_server.py --host 127.0.0.1 --port 8012
```

다른 터미널에서 실행한다.

```bash
cd backend
python scripts/smoke_agent_pipeline.py none --base-url http://127.0.0.1:8012
```

Expected:

```text
PASS none/health
PASS none/invalid_json
PASS none/invalid_envelope
PASS none/routing_unavailable
```

quest case가 별도 PASS 라인을 출력하는 현재 스크립트라면 quest 응답도 PASS로 확인한다. 출력 형식이 다르면 `expected_quest_count=5` 검증이 실패하지 않는 것을 기준으로 삼는다.

## Task 8: RAG 확장 기준 문서화

**Files:**
- Create: `backend/docs/plans/quest_rag_runtime_notes.md`

- [ ] **Step 1: 런타임 메모 문서 작성**

```markdown
# Quest RAG Runtime Notes

## 현재 모드

- mode: `csv_structured`
- 저장소: `data/game/*.csv`
- 검색 방식: CSV 행을 구조화한 뒤 recipe 중심으로 선택
- 외부 의존성: 없음

## 실제 RAG로 확장하는 기준

- 자연어 조건으로 레시피, 자원, 장비, 병목을 함께 검색해야 한다.
- CSV 행 수가 늘어나 키워드/ID 기반 선택이 불충분하다.
- 퀘스트 생성 요청에 플레이어 상태, 목표, 진행 단계가 들어와 의미 기반 랭킹이 필요하다.

## pgvector 전환 시 유지할 계약

- `payload.quests` 배열은 유지한다.
- `metadata.retrieval.mode`만 `pgvector` 또는 `embedding_vector`로 바꾼다.
- `metadata.retrieval.sources`에는 source id, source type, title을 계속 남긴다.
```

- [ ] **Step 2: 문서 변경 확인**

Run:

```bash
git diff -- backend/docs/plans/quest_rag_runtime_notes.md
```

Expected: 위 문서 내용이 추가되어 있다.

## 최종 검증

- [ ] `cd backend && python -m pytest tests/test_quest_agent_retriever.py -v`
- [ ] `cd backend && python -m pytest tests/test_quest_agent_service.py -v`
- [ ] `cd backend && python -m pytest tests/test_pipeline_edges.py::test_pipeline_routes_production_quest_from_llm_leaf_decision -v`
- [ ] `cd backend && python -m pytest tests/test_smoke_agent_pipeline_script.py -v`
- [ ] `cd backend && python -m pytest tests -v`
- [ ] `cd backend && python -m ruff check src tests scripts`
- [ ] `cd backend && python scripts/smoke_agent_pipeline.py none --base-url http://127.0.0.1:8012`

## 구현 후 자체 점검표

| 점검 항목 | 완료 여부 | 근거 |
| --- | --- | --- |
| `recipes.csv` 기반 퀘스트가 생성된다 | [ ] | `test_service_generates_quests_from_csv_knowledge` |
| 응답에 검색 근거 metadata가 포함된다 | [ ] | `metadata.retrieval.mode == "csv_structured"` |
| 기존 `payload.quests` 계약이 깨지지 않았다 | [ ] | pipeline edge test |
| 검색 실패 fallback이 동작한다 | [ ] | `test_service_falls_back_when_retrieval_has_too_few_entries` |
| WebSocket/smoke 경로에서 5개 quest를 반환한다 | [ ] | smoke script 결과 |
| 외부 DB 없이 CI에서 테스트 가능하다 | [ ] | 전체 pytest 결과 |

## Spec Coverage

- CSV 문서 참고: Task 2, Task 4에서 `recipes.csv`, `resources.csv`, `equipment.csv`를 검색 근거로 사용한다.
- RAG 유사 검색 흐름: Task 1, Task 2에서 normalized knowledge entry와 retriever를 만든다.
- 퀘스트 작성 참고: Task 4에서 검색 결과를 퀘스트 제목, 설명, objective 생성에 사용한다.
- 나중에 계획대로 했는지 확인: 완료 체크리스트, 최종 검증, 구현 후 자체 점검표를 포함했다.
- 단순함 우선: 첫 구현은 CSV 구조화 검색이며, pgvector/임베딩은 Task 8의 전환 기준으로만 남겼다.

## Placeholder Scan

- 미정 상태를 뜻하는 자리표시자 표현은 남기지 않았다.
- 모든 생성/수정 대상 파일은 구체 경로로 적었다.
- 각 테스트 단계에는 실행 명령과 기대 결과를 적었다.
