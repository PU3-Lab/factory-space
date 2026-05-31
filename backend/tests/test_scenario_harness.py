from __future__ import annotations

import pytest

from agents.pipeline import AgentPipeline
from tests.harness import StubLLM, top_agent_decision


@pytest.mark.parametrize(
    ("agent", "sub_agent", "expected_type"),
    [
        ("manual_qa", "manual_qa.machine_help", "machine"),
        ("manual_qa", "manual_qa.recipe_explainer", "recipe"),
        ("manual_qa", "manual_qa.troubleshooter", "troubleshooting"),
        ("quest_generator", "quest_generator.tutorial_quest", "tutorial"),
        ("quest_generator", "quest_generator.production_quest", "production"),
        ("quest_generator", "quest_generator.exploration_quest", "exploration"),
        ("quest_generator", "quest_generator.economy_quest", "economy"),
    ],
)
def test_explicit_sub_agent_scenarios(
    agent: str,
    sub_agent: str,
    expected_type: str,
) -> None:
    pipeline = AgentPipeline(llm=StubLLM([top_agent_decision(agent), None]))

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": f"request-{expected_type}",
            "agent": agent,
            "payload": {
                "sub_agent": sub_agent,
                "question": "What should I do?",
            },
        }
    )

    assert response["type"] == "agent.response"
    assert response["agent"] == agent
    assert response["payload"]["metadata"]["selectedLeafAgent"] == sub_agent
    assert "selectedSubAgent" not in response["payload"]["metadata"]
