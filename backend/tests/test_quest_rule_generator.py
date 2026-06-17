from __future__ import annotations

import re

from agents.quest_generator.context_builder import QuestContextBuilder
from agents.quest_generator.rule_generator import QuestRuleGenerator


def test_generate_drafts_success() -> None:
    # 1. Setup payload with a known issue
    payload = {
        "factory_id": "factory_001",
        "factory_level": 2,
        "current_main_quest": {
            "quest_id": "main_commtower",
            "title": "통신탑 건설",
            "objectives": [
                {
                    "main_objective_id": "need_iron_ingot",
                    "objective_type": "collect_item",
                    "item_id": "iron_ingot",
                    "required": 20,
                    "current": 8,
                }
            ],
        },
        "inventory": {
            "iron_ingot": 8,
        },
        "unlocked_recipes": [
            "smelt_iron",
        ],
    }
    context = QuestContextBuilder.build_context(payload)
    active_target_ids = set()

    # 2. Execute
    drafts = QuestRuleGenerator.generate_drafts(context, active_target_ids)

    # 3. Assertions
    assert len(drafts) == 1
    draft = drafts[0]
    assert draft.title == "철괴 확보 지원"
    assert (
        "메인 퀘스트 진행을 위해 철괴 12개가 더 필요합니다. 총 20개를 모으세요."
        in draft.description
    )
    assert draft.quest_type == "support"
    assert draft.support_type == "collect_item"
    assert len(draft.objectives) == 1

    obj = draft.objectives[0]
    # Verify objective_id format: obj_{uuid}
    assert re.match(r"^obj_[a-f0-9\-]{36}$", obj.id) is not None
    assert obj.target_id == "resource_iron_ingot"
    assert obj.target_amount == 20
    assert obj.current_amount == 0
    assert obj.status == "in_progress"

    # Verify rewards
    assert len(draft.rewards) == 1
    reward = draft.rewards[0]
    assert reward.type == "currency"
    assert reward.target_id == "gold"
    assert reward.amount == 100


def test_generate_drafts_skip_active() -> None:
    payload = {
        "factory_id": "factory_001",
        "factory_level": 2,
        "current_main_quest": {
            "quest_id": "main_commtower",
            "title": "통신탑 건설",
            "objectives": [
                {
                    "main_objective_id": "need_iron_ingot",
                    "objective_type": "collect_item",
                    "item_id": "iron_ingot",
                    "required": 20,
                    "current": 8,
                }
            ],
        },
        "inventory": {
            "iron_ingot": 8,
        },
        "unlocked_recipes": [
            "smelt_iron",
        ],
    }
    context = QuestContextBuilder.build_context(payload)
    # Iron ingot is already active
    active_target_ids = {"resource_iron_ingot"}

    drafts = QuestRuleGenerator.generate_drafts(context, active_target_ids)
    assert len(drafts) == 0


def test_generate_drafts_multiple_issues_priority() -> None:
    payload = {
        "factory_id": "factory_001",
        "factory_level": 2,
        "current_main_quest": {
            "quest_id": "main_commtower",
            "title": "통신탑 건설",
            "objectives": [
                {
                    "main_objective_id": "need_iron_ingot",
                    "objective_type": "collect_item",
                    "item_id": "iron_ingot",
                    "required": 20,
                    "current": 8,
                },
                {
                    "main_objective_id": "need_copper_ingot",
                    "objective_type": "collect_item",
                    "item_id": "copper_ingot",
                    "required": 15,
                    "current": 5,
                },
            ],
        },
        "inventory": {
            "iron_ingot": 8,
            "copper_ingot": 5,
        },
        "unlocked_recipes": [
            "smelt_iron",
            "smelt_copper",
        ],
    }
    context = QuestContextBuilder.build_context(payload)

    # 1. If none are active, both candidates are generated in priority order
    drafts1 = QuestRuleGenerator.generate_drafts(context, set())
    assert len(drafts1) == 2
    assert drafts1[0].objectives[0].target_id == "resource_iron_ingot"
    assert drafts1[1].objectives[0].target_id == "resource_copper_ingot"

    # 2. If iron is active, only copper is generated
    drafts2 = QuestRuleGenerator.generate_drafts(context, {"resource_iron_ingot"})
    assert len(drafts2) == 1
    assert drafts2[0].objectives[0].target_id == "resource_copper_ingot"
    assert "구리괴" in drafts2[0].title
    assert "구리괴 10개가 더 필요합니다. 총 15개를 모으세요." in drafts2[0].description

    # 3. If both are active, return empty list
    drafts3 = QuestRuleGenerator.generate_drafts(
        context, {"resource_iron_ingot", "resource_copper_ingot"}
    )
    assert len(drafts3) == 0


def test_generate_drafts_no_issues() -> None:
    # 1. No main quest
    payload_no_quest = {
        "factory_id": "factory_001",
        "factory_level": 2,
        "inventory": {},
    }
    context_no_quest = QuestContextBuilder.build_context(payload_no_quest)
    assert len(QuestRuleGenerator.generate_drafts(context_no_quest, set())) == 0

    # 2. Objectives fully met
    payload_met = {
        "factory_id": "factory_001",
        "factory_level": 2,
        "current_main_quest": {
            "quest_id": "main_commtower",
            "title": "통신탑 건설",
            "objectives": [
                {
                    "main_objective_id": "need_iron_ingot",
                    "objective_type": "collect_item",
                    "item_id": "iron_ingot",
                    "required": 20,
                    "current": 20,
                }
            ],
        },
        "inventory": {
            "iron_ingot": 20,
        },
    }
    context_met = QuestContextBuilder.build_context(payload_met)
    assert len(QuestRuleGenerator.generate_drafts(context_met, set())) == 0


def test_generate_drafts_objective_mismatch_recovery() -> None:
    """기획 불일치로 첫 번째 known_issue의 메인 목표를 찾을 수 없는 경우, 다음 유효한 부족 자원으로 정상 동작하는지 검증합니다."""
    payload = {
        "factory_id": "factory_001",
        "factory_level": 2,
        "current_main_quest": {
            "quest_id": "main_commtower",
            "title": "통신탑 건설",
            "objectives": [
                {
                    "main_objective_id": "need_copper_ingot",
                    "objective_type": "collect_item",
                    "item_id": "copper_ingot",
                    "required": 15,
                    "current": 5,
                }
            ],
        },
        "inventory": {
            "iron_ingot": 8,
            "copper_ingot": 5,
        },
        "unlocked_recipes": [
            "smelt_iron",
            "smelt_copper",
        ],
    }
    context = QuestContextBuilder.build_context(payload)

    # Manually inject a mismatched known issue as the first item
    from agents.quest_generator.models import KnownIssue

    mismatched_issue = KnownIssue(
        item_id="resource_iron_ingot",
        shortage_amount=12,
        main_objective_id="invalid_objective_id",  # Mismatched ID
        producible=True,
    )
    context.known_issues.insert(0, mismatched_issue)

    # Execute
    drafts = QuestRuleGenerator.generate_drafts(context, set())

    # Assertions: The mismatched iron_ingot should be skipped, and copper_ingot should be successfully generated.
    assert len(drafts) == 1
    assert drafts[0].objectives[0].target_id == "resource_copper_ingot"
    assert "구리괴" in drafts[0].title
