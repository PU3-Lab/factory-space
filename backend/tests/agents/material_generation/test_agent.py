"""Integration tests for MaterialCreationAgent orchestration."""

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
        assert "Fallback" in (res.name or "")
        assert res.visual_status == "pending"


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
        # First attempt (creates new material)
        res1 = agent.synthesize(db_session, req)
        assert res1.result_type == "new_material"

        # Second attempt (should reuse cached experiment)
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

    # Mock generate_proposal to return invalid/failed proposal
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

        # Verify that it finally returns failed_result
        assert res.result_type == "failed_result"
        assert res.failure_reason == "Synthesis rejected by LLM analysis."

        # Verify that the LLM proposal generator was queried exactly 3 times (due to retry loop)
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
        assert "Fallback" in (res.name or "")
        assert res.visual_status == "skipped"
        assert res.message == "새로운 물질이 발견되었습니다."


def test_agent_synthesize_new_material_visual_asset_true_background_processing(
    db_session: Session,
) -> None:
    """Test that setting generate_visual_asset=True runs the background task and updates the DB."""
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
        assert res.visual_status == "pending"

        # Commit session to trigger "after_commit" event listener
        db_session.commit()

        # Wait for all background executor jobs to complete
        MaterialEventPublisher.wait_for_jobs()

        # Query the updated material from DB
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
    """Test that permanently shutting down the executor rejects new jobs and doesn't recreate it."""
    # Ensure executor is in a clean reset state before test
    MaterialEventPublisher.reset_executor(wait=True)

    # Trigger a normal event publication
    MaterialEventPublisher.publish_material_created(
        material_id="mat_test_001",
        visual_prompt="A shiny metallic plate",
        category="plates",
    )

    # Perform permanent shutdown
    MaterialEventPublisher.shutdown_executor(wait=True)

    # Attempt to publish another event after shutdown
    # This should be safely ignored (logged) without raising errors
    MaterialEventPublisher.publish_material_created(
        material_id="mat_test_002",
        visual_prompt="Another metallic plate",
        category="plates",
    )

    # Finally, reset back the executor to not affect other tests
    MaterialEventPublisher.reset_executor(wait=True)


def test_consecutive_app_lifespans_processing_events(db_session: Session) -> None:
    """Test that two consecutive app lifespans can both process visual events correctly."""
    # App 1 execution
    app1 = create_app()
    with TestClient(app1):
        MaterialEventPublisher.publish_material_created(
            material_id="mat_lifespan_test_1",
            visual_prompt="Shiny metal",
            category="plates",
        )
    # Exiting the client block shuts down the executor permanently

    # App 2 execution
    app2 = create_app()
    with TestClient(app2):
        # A new material model in DB
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

        # Re-triggering a new event in the second lifespan
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
