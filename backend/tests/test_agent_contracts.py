from __future__ import annotations

from agents.manual_qa.agent import ManualQaAgent
from agents.orchestrator import OrchestratorAgent
from agents.quest_generator.agent import QuestGeneratorAgent
from agents.router import create_default_agent_router


def test_default_agent_router_contains_leaf_agents() -> None:
    router = create_default_agent_router()

    assert router.list_agent_ids() == [
        "manual_qa.machine_help",
        "manual_qa.recipe_explainer",
        "manual_qa.troubleshooter",
        "new_material_generator",
        "process_optimizer",
        "quest_generator.economy_quest",
        "quest_generator.exploration_quest",
        "quest_generator.production_quest",
        "quest_generator.tutorial_quest",
    ]


def test_orchestrator_parses_only_allowed_model_selection() -> None:
    orchestrator = OrchestratorAgent()

    assert (
        orchestrator.parse_agent_selection(
            '{"agent":"quest_generator","reason":"needs a quest"}'
        )
        == "quest_generator"
    )
    assert orchestrator.parse_agent_selection('{"agent":"unknown"}') is None


def test_sub_orchestrators_parse_only_allowed_model_selection() -> None:
    manual = ManualQaAgent()
    quest = QuestGeneratorAgent()

    assert (
        manual.parse_sub_agent_selection(
            '{"sub_agent":"manual_qa.machine_help","reason":"machine question"}'
        )
        == "manual_qa.machine_help"
    )
    assert (
        quest.parse_sub_agent_selection(
            '{"sub_agent":"quest_generator.production_quest","reason":"production"}'
        )
        == "quest_generator.production_quest"
    )
    assert manual.parse_sub_agent_selection('{"sub_agent":"manual_qa.unknown"}') is None
    assert quest.parse_sub_agent_selection('{"sub_agent":"quest_generator.unknown"}') is None
