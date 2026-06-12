"""Integration tests for MaterialCreationAgent orchestration."""

from __future__ import annotations

from sqlalchemy.orm import Session

from agents.material_generation.agent import MaterialCreationAgent
from agents.material_generation.schemas import (
    InputItemSchema,
    MaterialCreationRequest,
)


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
    from unittest.mock import patch

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


def test_agent_synthesize_experiment_cached_reuse(db_session: Session) -> None:
    from unittest.mock import patch

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
    from unittest.mock import patch

    from agents.material_generation.schemas import MaterialProposal

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
