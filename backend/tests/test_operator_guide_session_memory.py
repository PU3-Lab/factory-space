"""Tests for operator_guide session memory."""

from __future__ import annotations

from agents.pipeline import AgentPipeline
from tests.harness import StubLLM, assert_agent_response


def test_pipeline_adds_recent_operator_guide_turns_to_followup_prompt() -> None:
    llm = StubLLM(
        [
            (
                '{"final_answer":"A crusher breaks resources down.",'
                '"actions":[],"question":"What is a crusher?",'
                '"topic":"machine"}'
            ),
            (
                '{"final_answer":"For iron ingots, check the smelter flow first.",'
                '"actions":[],"question":"Does it help with iron ingots?",'
                '"topic":"recipe"}'
            ),
        ]
    )
    pipeline = AgentPipeline(llm=llm)

    first_response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "memory-request-1",
            "session_id": "memory-session-1",
            "agent": "operator_guide",
            "payload": {
                "question": "What is a crusher?",
                "sub_agent": "operator_guide.machine_help",
            },
        }
    )
    second_response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "memory-request-2",
            "session_id": "memory-session-1",
            "agent": "operator_guide",
            "payload": {
                "question": "Does it help with iron ingots?",
                "sub_agent": "operator_guide.recipe_explainer",
            },
        }
    )

    assert_agent_response(
        first_response,
        agent="operator_guide",
        sub_agent="operator_guide.machine_help",
    )
    assert_agent_response(
        second_response,
        agent="operator_guide",
        sub_agent="operator_guide.recipe_explainer",
    )

    followup_user_prompt = llm.prompt_messages[-1][1]["content"]

    assert "[RECENT_CONVERSATION_CONTEXT]" in followup_user_prompt
    assert "What is a crusher?" in followup_user_prompt
    assert "A crusher breaks resources down." in followup_user_prompt
    memory_metadata = second_response["payload"]["metadata"]["memory"]
    assert memory_metadata["used"] is True
    assert memory_metadata["turn_count"] == 1
    assert memory_metadata["max_turns"] == 4
    assert "confirmed_facts" in memory_metadata
    assert "summary_version" in memory_metadata
