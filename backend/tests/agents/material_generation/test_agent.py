"""MaterialCreationAgent 오케스트레이션에 대한 통합 테스트입니다."""

from __future__ import annotations

from collections.abc import Iterator
from contextlib import contextmanager
from unittest.mock import patch

from fastapi.testclient import TestClient
from sqlalchemy import func, select
from sqlalchemy.orm import Session, sessionmaker

from agents.material_generation.agent import MaterialCreationAgent
from agents.material_generation.events import MaterialEventPublisher
from agents.material_generation.schemas import (
    InputItemSchema,
    MaterialCreationRequest,
    MaterialProposal,
)
from app import create_app
from db.models import GeneratedMaterialModel


def test_agent_synthesize_existing_recipe(db_session: Session) -> None:
    agent = MaterialCreationAgent()
    req = MaterialCreationRequest(
        machine_type="Smelter",
        inputs=[InputItemSchema(item_id="iron_ore", qty=2)],
        player_id="player_001",
    )

    res = agent.synthesize(db_session, req)
    assert res.result_type == "existing_recipe"
    assert res.recipe_name == "Smelt_Iron"
    assert res.outputs is not None
    assert len(res.outputs) == 1
    assert res.outputs[0].item_id == "iron_ingot"
    assert res.outputs[0].qty == 1


def test_agent_synthesize_failed_by_policy(db_session: Session) -> None:
    agent = MaterialCreationAgent()
    req = MaterialCreationRequest(
        machine_type="Smelter",
        inputs=[
            InputItemSchema(item_id="iron_ore", qty=1),
            InputItemSchema(item_id="iron_ingot", qty=1),
        ],
        player_id="player_001",
    )

    res = agent.synthesize(db_session, req)
    assert res.result_type == "failed_result"
    assert "Machine policies prohibit" in (res.failure_reason or "")


def test_agent_synthesize_new_material_fallback_path(db_session: Session) -> None:
    agent = MaterialCreationAgent()
    req = MaterialCreationRequest(
        machine_type="Synthesizer",
        inputs=[
            InputItemSchema(item_id="iron_ore", qty=1),
            InputItemSchema(item_id="iron_ingot", qty=1),
        ],
        player_id="player_001",
    )

    with (
        patch("llm.adapter.GoogleGenAiLLMAdapter.invoke", return_value=None),
        patch("llm.adapter.OpenAILLMAdapter.invoke", return_value=None),
        patch("llm.adapter.LocalLLMAdapter.invoke", return_value=None),
    ):
        res = agent.synthesize(db_session, req)
        assert res.result_type == "new_material"
        assert res.material_id is not None
        assert res.name  # 비어있지 않음
        assert res.rarity in {"common", "uncommon", "rare", "epic"}
        assert res.state in {"solid", "liquid", "gas", "plasma"}
        assert res.visual_status == "visual_ready"
        assert res.visual_asset_key == f"materials/{res.material_id}/icon.png"
        assert res.texture_asset_key == f"materials/{res.material_id}/texture.png"
        assert res.thumbnail_asset_key == f"materials/{res.material_id}/thumbnail.png"

        # 저장된 물질의 결정론 값 검증 (iron_ore+iron_ingot @ Synthesizer)
        stored = db_session.execute(
            select(GeneratedMaterialModel).where(
                GeneratedMaterialModel.id == res.material_id
            )
        ).scalar_one()
        assert stored.state == "solid"
        assert stored.rarity == "uncommon"


def test_material_creation_api_persists_material_without_duplicates(
    db_session: Session,
) -> None:
    session_factory = sessionmaker(
        bind=db_session.get_bind(),
        expire_on_commit=False,
        class_=Session,
    )

    @contextmanager
    def get_test_db_session() -> Iterator[Session]:
        session = session_factory()
        try:
            yield session
            session.commit()
        except Exception:
            session.rollback()
            raise
        finally:
            session.close()

    request_payload = {
        "machine_type": "Synthesizer",
        "inputs": [
            {"item_id": "iron_ore", "qty": 1},
            {"item_id": "iron_ingot", "qty": 1},
        ],
        "player_id": "player_persistence_test",
        "generate_visual_asset": False,
    }

    with (
        patch(
            "agents.material_generation.router.get_db_session",
            get_test_db_session,
        ),
        patch("llm.adapter.GoogleGenAiLLMAdapter.invoke", return_value=None),
        patch("llm.adapter.OpenAILLMAdapter.invoke", return_value=None),
        patch("llm.adapter.LocalLLMAdapter.invoke", return_value=None),
    ):
        with TestClient(create_app()) as client:
            first_result = client.post(
                "/api/v1/experiments/material-creation",
                json=request_payload,
            )
            repeated_result = client.post(
                "/api/v1/experiments/material-creation",
                json=request_payload,
            )

        assert first_result.status_code == 201
        assert repeated_result.status_code == 201
        first_response = first_result.json()
        repeated_response = repeated_result.json()

        with session_factory() as read_session:
            persisted_material = read_session.get(
                GeneratedMaterialModel,
                first_response["material_id"],
            )

            assert persisted_material is not None
            assert persisted_material.material_hash == first_response["material_hash"]
            assert persisted_material.name == first_response["name"]
            assert persisted_material.category == "alloy"
            assert persisted_material.properties_json
            assert (
                persisted_material.source_experiment_hash
                == (first_response["experiment_hash"])
            )
            assert persisted_material.visual_status == "skipped"
            material_count = read_session.scalar(
                select(func.count())
                .select_from(GeneratedMaterialModel)
                .where(
                    GeneratedMaterialModel.material_hash
                    == first_response["material_hash"]
                )
            )

            assert repeated_response["result_type"] == "cached_experiment"
            assert repeated_response["material_id"] == first_response["material_id"]
            assert material_count == 1


def test_agent_synthesize_experiment_cached_reuse(db_session: Session) -> None:
    agent = MaterialCreationAgent()
    req = MaterialCreationRequest(
        machine_type="Synthesizer",
        inputs=[
            InputItemSchema(item_id="iron_ore", qty=1),
            InputItemSchema(item_id="iron_ingot", qty=1),
        ],
        player_id="player_001",
    )

    with (
        patch("llm.adapter.GoogleGenAiLLMAdapter.invoke", return_value=None),
        patch("llm.adapter.OpenAILLMAdapter.invoke", return_value=None),
        patch("llm.adapter.LocalLLMAdapter.invoke", return_value=None),
    ):
        # 첫 번째 시도 (새로운 재료 생성)
        res1 = agent.synthesize(db_session, req)
        assert res1.result_type == "new_material"

        # 두 번째 시도 (캐시된 실험을 재사용해야 함)
        res2 = agent.synthesize(db_session, req)
        assert res2.result_type == "cached_experiment"
        assert res2.cached is True
        assert res2.material_id == res1.material_id
        assert res2.name == res1.name
        assert res2.rarity == res1.rarity
        assert res2.visual_status == res1.visual_status
        assert res2.fallback_icon == res1.fallback_icon


def test_agent_synthesize_retry_loop_failure(db_session: Session) -> None:
    agent = MaterialCreationAgent()
    req = MaterialCreationRequest(
        machine_type="Synthesizer",
        inputs=[
            InputItemSchema(item_id="iron_ore", qty=1),
            InputItemSchema(item_id="iron_ingot", qty=1),
        ],
        player_id="player_retry_test",
    )

    # 유효하지 않거나 실패한 제안을 반환하도록 generate_proposal 모킹(Mocking)
    failed_proposal = MaterialProposal(
        proposal_type="failed",
        confidence=0.0,
        reason="Mocked failure proposal",
        result=None,
    )

    with patch(
        "agents.material_generation.proposal_generator.MaterialProposalGenerator.generate_proposal",
        return_value=failed_proposal,
    ) as mock_gen:
        res = agent.synthesize(db_session, req)

        # 최종적으로 failed_result를 반환하는지 확인
        assert res.result_type == "failed_result"
        assert res.failure_reason == "Synthesis rejected by LLM analysis."

        # LLM 제안 생성기가 정확히 3번 조회되었는지 확인 (재시도 루프 때문)
        assert mock_gen.call_count == 3


def test_agent_synthesize_new_material_visual_asset_false(db_session: Session) -> None:
    agent = MaterialCreationAgent()
    req = MaterialCreationRequest(
        machine_type="Synthesizer",
        inputs=[
            InputItemSchema(item_id="iron_ore", qty=1),
            InputItemSchema(item_id="iron_ingot", qty=1),
        ],
        player_id="player_001",
        generate_visual_asset=False,
    )

    with (
        patch("llm.adapter.GoogleGenAiLLMAdapter.invoke", return_value=None),
        patch("llm.adapter.OpenAILLMAdapter.invoke", return_value=None),
        patch("llm.adapter.LocalLLMAdapter.invoke", return_value=None),
    ):
        res = agent.synthesize(db_session, req)
        assert res.result_type == "new_material"
        assert res.material_id is not None
        assert res.name
        assert res.visual_status == "skipped"
        assert res.message == "새로운 물질이 발견되었습니다."


def test_agent_synthesize_new_material_visual_asset_true_returns_ready_assets(
    db_session: Session,
) -> None:
    """generate_visual_asset=True이면 이미지 생성 완료 후 에셋 키가 포함된 응답을 반환합니다."""
    agent = MaterialCreationAgent()
    req = MaterialCreationRequest(
        machine_type="Synthesizer",
        inputs=[
            InputItemSchema(item_id="iron_ore", qty=1),
            InputItemSchema(item_id="iron_ingot", qty=1),
        ],
        player_id="player_001",
        generate_visual_asset=True,
    )

    with (
        patch("llm.adapter.GoogleGenAiLLMAdapter.invoke", return_value=None),
        patch("llm.adapter.OpenAILLMAdapter.invoke", return_value=None),
        patch("llm.adapter.LocalLLMAdapter.invoke", return_value=None),
    ):
        res = agent.synthesize(db_session, req)
        assert res.result_type == "new_material"
        assert res.material_id is not None
        assert res.visual_status == "visual_ready"
        assert res.visual_asset_key == f"materials/{res.material_id}/icon.png"
        assert res.texture_asset_key == f"materials/{res.material_id}/texture.png"
        assert res.thumbnail_asset_key == f"materials/{res.material_id}/thumbnail.png"

        # DB에서 업데이트된 재료를 쿼리
        db_session.expire_all()
        updated_material = db_session.execute(
            select(GeneratedMaterialModel).where(
                GeneratedMaterialModel.id == res.material_id
            )
        ).scalar_one_or_none()

        assert updated_material is not None
        assert updated_material.visual_status == "visual_ready"
        assert (
            updated_material.visual_asset_key == f"materials/{res.material_id}/icon.png"
        )


def test_visual_executor_permanent_shutdown_and_block(db_session: Session) -> None:
    """실행자를 영구적으로 종료하면 새 작업이 거부되고 재생성되지 않는지 테스트합니다."""
    # 테스트 전에 실행자가 깨끗하게 리셋된 상태인지 확인
    MaterialEventPublisher.reset_executor(wait=True)

    # 정상적인 이벤트 게시 트리거
    MaterialEventPublisher.publish_material_created(
        material_id="mat_test_001",
        visual_prompt="A shiny metallic plate",
        category="plates",
    )

    # 영구 종료 수행
    MaterialEventPublisher.shutdown_executor(wait=True)

    # 종료 후 다른 이벤트를 게시하도록 시도
    # 이는 에러를 발생시키지 않고 안전하게 무시(로그 기록)되어야 함
    MaterialEventPublisher.publish_material_created(
        material_id="mat_test_002",
        visual_prompt="Another metallic plate",
        category="plates",
    )

    # 마지막으로 다른 테스트에 영향을 주지 않도록 실행자를 다시 리셋
    MaterialEventPublisher.reset_executor(wait=True)


def test_consecutive_app_lifespans_processing_events(db_session: Session) -> None:
    """두 개의 연속적인 앱 라이프사이클에서 시각적 이벤트를 모두 올바르게 처리할 수 있는지 테스트합니다."""
    # 앱 1 실행
    app1 = create_app()
    with TestClient(app1):
        MaterialEventPublisher.publish_material_created(
            material_id="mat_lifespan_test_1",
            visual_prompt="Shiny metal",
            category="plates",
        )
    # client 블록을 종료하면 실행자가 영구적으로 종료됩니다.

    # 앱 2 실행
    app2 = create_app()
    with TestClient(app2):
        # DB에 새로운 재료 모델 추가
        new_mat = GeneratedMaterialModel(
            id="mat_lifespan_test_2",
            material_hash="hash_test_lifecycle",
            name="Lifecycle Test Material",
            category="plates",
            rarity="common",
            properties_json={},
            visual_status="pending",
            visual_prompt="Shiny plate",
            fallback_icon="materials/default/plates.png",
        )
        db_session.add(new_mat)
        db_session.commit()

        # 두 번째 라이프사이클에서 새 이벤트 재트리거
        MaterialEventPublisher.publish_material_created(
            material_id="mat_lifespan_test_2",
            visual_prompt="Shiny plate",
            category="plates",
        )

        MaterialEventPublisher.wait_for_jobs()

        db_session.expire_all()
        updated = db_session.execute(
            select(GeneratedMaterialModel).where(
                GeneratedMaterialModel.id == "mat_lifespan_test_2"
            )
        ).scalar_one_or_none()

        assert updated is not None
        assert updated.visual_status == "visual_ready"


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


def test_agent_synthesize_existing_material_returns_asset_keys_when_experiment_cache_miss(
    db_session: Session,
) -> None:
    """실험 캐시(experiments 테이블)에는 정보가 없지만, 기존 생성된 물질(materials 테이블)이 존재하는 경우,
    deduplicate_material_node에서 기존 물질 정보와 함께 이미 완료된 비주얼 에셋 키를 반환하는지 테스트합니다."""
    # 1. 기존 물질 정보 직접 삽입 (visual_ready 상태 및 에셋 키 설정)
    material_id = "mat_existing_test_001"
    mat_hash = "hash_existing_test_001"
    db_session.add(
        GeneratedMaterialModel(
            id=material_id,
            material_hash=mat_hash,
            name="Existing Test Material",
            category="alloy",
            rarity="rare",
            properties_json={},
            visual_status="visual_ready",
            visual_asset_key=f"materials/{material_id}/icon.png",
            texture_asset_key=f"materials/{material_id}/texture.png",
            thumbnail_asset_key=f"materials/{material_id}/thumbnail.png",
            fallback_icon="materials/default/alloy.png",
        )
    )
    db_session.commit()

    # 2. Mocking generate_material_hash to return the exact hash we registered
    agent = MaterialCreationAgent()
    req = MaterialCreationRequest(
        machine_type="Synthesizer",
        inputs=[
            InputItemSchema(item_id="iron_ore", qty=1),
            InputItemSchema(item_id="iron_ingot", qty=1),
        ],
        player_id="player_existing_material_test",
        generate_visual_asset=False,
    )

    with (
        patch("llm.adapter.GoogleGenAiLLMAdapter.invoke", return_value=None),
        patch("llm.adapter.OpenAILLMAdapter.invoke", return_value=None),
        patch("llm.adapter.LocalLLMAdapter.invoke", return_value=None),
        patch(
            "agents.material_generation.nodes.generate_material_hash",
            return_value=mat_hash,
        ),
    ):
        # 3. synthesize 실행 - 이 실험의 해시는 DB에 없으므로 캐시 미스 발생.
        # 그러나 generate_material_hash 모킹으로 deduplicate_material_node에서 existing_mat를 만나게 됨.
        res = agent.synthesize(db_session, req)

    # 4. 검증: result_type은 new_material이고(중복 물질 발견 시점),
    # generation_status는 cached이며, asset key들이 정확히 반환되는지 확인.
    assert res.result_type == "new_material"
    assert res.material_id == material_id
    assert res.generation_status == "cached"
    assert res.visual_status == "visual_ready"
    assert res.visual_asset_key == f"materials/{material_id}/icon.png"
    assert res.texture_asset_key == f"materials/{material_id}/texture.png"
    assert res.thumbnail_asset_key == f"materials/{material_id}/thumbnail.png"
