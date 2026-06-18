from __future__ import annotations

import re

from agents.quest_generator.context_builder import QuestContextBuilder
from agents.quest_generator.rule_generator import QuestRuleGenerator


def test_generate_drafts_success() -> None:
    # 1. Setup payload with a known issue (Level 2)
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
    # 수량 스케일링: Level 2 -> 10 * 2 = 20
    assert "공장 지원을 위해 철괴 20개를 확보하세요." in draft.description
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

    # Verify rewards: 100 * Level 2 = 200
    assert len(draft.rewards) == 1
    reward = draft.rewards[0]
    assert reward.type == "currency"
    assert reward.target_id == "gold"
    assert reward.amount == 200


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
    assert "공장 지원을 위해 구리괴 20개를 확보하세요." in drafts2[0].description

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


def test_generate_drafts_level_scaling_and_caps() -> None:
    """공장 레벨에 따른 수량 스케일링, 보상 스케일링, 그리고 producible 여부에 따른 상한(Caps)을 검증합니다."""
    # 1. producible=True (생산 가능) 일 때: HARD_CAP(1000) 테스트
    # Level 150 -> 10 * 150 = 1500, but capped at 1000
    payload_high = {
        "factory_id": "factory_001",
        "factory_level": 150,
        "current_main_quest": {
            "quest_id": "main_001",
            "title": "철괴 확보",
            "objectives": [
                {
                    "main_objective_id": "mobj_001",
                    "objective_type": "collect_item",
                    "item_id": "resource_iron_ingot",
                    "required": 2000,
                    "current": 10,
                }
            ],
        },
        "inventory": {"resource_iron_ingot": 10},
        "unlocked_recipes": ["recipe_smelt_iron"],
    }
    context_high = QuestContextBuilder.build_context(payload_high)
    drafts_high = QuestRuleGenerator.generate_drafts(context_high, set())

    assert len(drafts_high) == 1
    assert drafts_high[0].objectives[0].target_amount == 1000  # Capped at HARD_CAP
    assert drafts_high[0].rewards[0].amount == 15000  # 100 * 150 = 15000

    # 2. producible=False (생산 불가) 일 때: NONPRODUCIBLE_CAP(10) 테스트
    # Level 5 -> 10 * 5 = 50, but capped at 10 because recipe not unlocked
    payload_unproducible = {
        "factory_id": "factory_001",
        "factory_level": 5,
        "current_main_quest": {
            "quest_id": "main_001",
            "title": "철괴 확보",
            "objectives": [
                {
                    "main_objective_id": "mobj_001",
                    "objective_type": "collect_item",
                    "item_id": "resource_iron_ingot",
                    "required": 100,
                    "current": 10,
                }
            ],
        },
        "inventory": {"resource_iron_ingot": 10},
        "unlocked_recipes": [],  # 레시피 해금하지 않음 -> producible=False
    }
    context_unproducible = QuestContextBuilder.build_context(payload_unproducible)
    drafts_unproducible = QuestRuleGenerator.generate_drafts(
        context_unproducible, set()
    )

    assert len(drafts_unproducible) == 1
    assert (
        drafts_unproducible[0].objectives[0].target_amount == 10
    )  # Capped at NONPRODUCIBLE_CAP
    assert drafts_unproducible[0].rewards[0].amount == 500  # 100 * 5 = 500


def test_generate_drafts_level_gating() -> None:
    """아이템 요구 레벨에 의한 레벨 게이트(Gating) 기능을 검증합니다. (아연은 최소 2레벨 필요)"""
    # 아연광석(zinc_ore)과 철괴(iron_ingot)를 메인 목표로 가지는 메인 퀘스트 생성
    payload = {
        "factory_id": "factory_001",
        "current_main_quest": {
            "quest_id": "main_001",
            "title": "자원 확보",
            "objectives": [
                {
                    "main_objective_id": "mobj_zinc",
                    "objective_type": "collect_item",
                    "item_id": "zinc_ore",  # 요구레벨 2
                    "required": 20,
                    "current": 5,
                },
                {
                    "main_objective_id": "mobj_iron",
                    "objective_type": "collect_item",
                    "item_id": "iron_ingot",  # 요구레벨 1
                    "required": 20,
                    "current": 5,
                },
            ],
        },
        "inventory": {
            "zinc_ore": 5,
            "iron_ingot": 5,
        },
        "unlocked_recipes": [],
    }

    # 1. factory_level = 1 일 때 -> 아연(요구레벨 2)은 게이트에 의해 스킵되어 퀘스트 미생성. 철괴(요구레벨 1)만 생성됨.
    payload["factory_level"] = 1
    context_lvl1 = QuestContextBuilder.build_context(payload)

    drafts_lvl1 = QuestRuleGenerator.generate_drafts(context_lvl1, set())
    assert len(drafts_lvl1) == 1
    assert drafts_lvl1[0].objectives[0].target_id == "resource_iron_ingot"

    # 2. factory_level = 2 일 때 -> 아연(요구레벨 2) 조건 충족하여 두 가지 모두 정상 생성
    payload["factory_level"] = 2
    context_lvl2 = QuestContextBuilder.build_context(payload)

    drafts_lvl2 = QuestRuleGenerator.generate_drafts(context_lvl2, set())
    assert len(drafts_lvl2) == 2
    assert drafts_lvl2[0].objectives[0].target_id == "resource_zinc_ore"
    assert drafts_lvl2[1].objectives[0].target_id == "resource_iron_ingot"
