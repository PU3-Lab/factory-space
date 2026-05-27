from __future__ import annotations

import pytest

from factory_space.messages.protocol import ErrorMessage, MessageEnvelope
from tests.harness import (
    AgentScenario,
    assert_agent_response_contract,
    assert_error_contract,
    run_agent_scenario,
    run_ping_scenario,
)


@pytest.mark.parametrize(
    "scenario",
    [
        AgentScenario(
            name="factory optimization smoke",
            agent="factory_optimization",
            payload={
                "question": "Where is the bottleneck?",
                "factory_state": {
                    "machines": [
                        {
                            "id": "packaging_01",
                            "input_rate": 100,
                            "output_rate": 62,
                            "status": "running",
                        }
                    ]
                },
            },
        ),
        AgentScenario(
            name="material generation smoke",
            agent="material_generation",
            payload={
                "goal": "light and heat resistant material",
                "constraints": {
                    "max_weight": "low",
                    "heat_resistance": "high",
                    "cost": "medium",
                },
            },
        ),
        AgentScenario(
            name="qa chatbot smoke",
            agent="qa_chatbot",
            payload={
                "question": "How do I inspect this machine?",
                "context": {"selected_object_id": "machine_01"},
            },
        ),
        AgentScenario(
            name="quest smoke",
            agent="quest",
            payload={
                "event": "player_interacted",
                "object_id": "control_panel_01",
                "quest_id": "quest-001",
            },
        ),
    ],
    ids=lambda scenario: scenario.name,
)
def test_agent_smoke_scenarios_return_contract_response(
    scenario: AgentScenario,
) -> None:
    response = run_agent_scenario(scenario)

    assert isinstance(response, MessageEnvelope)
    assert_agent_response_contract(response, expected_agent=scenario.agent)


def test_ping_smoke_scenario_returns_pong() -> None:
    response = run_ping_scenario(payload={"timestamp": "now"})

    assert isinstance(response, MessageEnvelope)
    assert response.type == "pong"
    assert response.payload == {"timestamp": "now"}


def test_unknown_agent_scenario_returns_error_contract() -> None:
    response = run_agent_scenario(
        AgentScenario(
            name="unknown agent",
            agent="missing",
            payload={},
        )
    )

    assert isinstance(response, ErrorMessage)
    assert_error_contract(response, expected_code="UNKNOWN_AGENT")
