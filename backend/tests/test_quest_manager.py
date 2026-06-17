from __future__ import annotations

import unittest.mock
from collections.abc import Generator

import pytest
from sqlalchemy import create_engine
from sqlalchemy.orm import Session, sessionmaker

from agents.quest_generator.manager import QuestManager
from agents.quest_generator.models import QuestObjective, QuestReward, SupportQuestDraft
from agents.quest_generator.repository import QuestRepository
from agents.quest_generator.reward_resolver import QuestRewardResolver
from db.models import Base


@pytest.fixture
def db_session() -> Generator[Session, None, None]:
    # Setup in-memory SQLite database
    engine = create_engine("sqlite:///:memory:")
    Base.metadata.create_all(bind=engine)
    session_local = sessionmaker(bind=engine)
    session = session_local()
    try:
        yield session
    finally:
        session.close()


def test_reward_resolver_filter_inputs() -> None:
    # A needs B, B needs C.
    # If target is A, excluded should be A, B, C.
    mock_recipe_map = {
        "recipe_b_to_a": ("resource_a", ["resource_b"]),
        "recipe_c_to_b": ("resource_b", ["resource_c"]),
    }

    rewards = [
        QuestReward(type="currency", target_id="gold", amount=100),
        QuestReward(type="currency", target_id="gem", amount=5),
        # Excluded item rewards
        QuestReward(
            type="currency", target_id="resource_a", amount=1
        ),  # same as target
        QuestReward(type="currency", target_id="resource_b", amount=2),  # input 1
        QuestReward(type="currency", target_id="resource_c", amount=3),  # input 2
        # Allowed item rewards
        QuestReward(type="currency", target_id="resource_d", amount=5),  # unrelated
    ]

    # Patch type to cheat pydantic Literal restriction (Literal["currency"] in MVP models.py)
    # Actually, models.py defines: type: Literal["currency"] = Field(default="currency")
    # For testing item type exclusion logic, we temporarily change reward target_id.
    # Note: MVP only supports Literal["currency"], but our resolver logic should filter by target_id of items.
    # Since MVP models.py does not support Literal["item"] yet, we test by target_id filtering on our resolver.
    # Resolver filters out if reward.target_id is in excluded list (irrespective of type, or if reward type is currency but target is item id).
    # This keeps it safe.

    with unittest.mock.patch(
        "agents.quest_generator.game_data.get_recipe_map", return_value=mock_recipe_map
    ):
        resolved = QuestRewardResolver.resolve_rewards(rewards, "resource_a")

    # resource_a, resource_b, resource_c must be excluded.
    # gold, gem, and resource_d should remain.
    remaining_ids = [r.target_id for r in resolved]
    assert "gold" in remaining_ids
    assert "gem" in remaining_ids
    assert "resource_d" in remaining_ids
    assert "resource_a" not in remaining_ids
    assert "resource_b" not in remaining_ids
    assert "resource_c" not in remaining_ids


def test_create_quest_from_draft_success(db_session: Session) -> None:
    # 1. Setup draft
    draft = SupportQuestDraft(
        title="철괴 확보 지원",
        description="철괴를 확보하세요.",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_test_001",
                type="collect_item",
                target_id="resource_iron_ingot",
                target_amount=10,
                current_amount=0,
                status="in_progress",
            )
        ],
        rewards=[QuestReward(type="currency", target_id="gold", amount=100)],
    )

    # 2. Execute creation
    instance = QuestManager.create_quest_from_draft(
        session=db_session,
        factory_id="factory_001",
        draft=draft,
        related_main_quest_id="main_001",
    )

    # 3. Assertions
    assert instance.id.startswith("qinst_")
    assert instance.factory_id == "factory_001"
    assert instance.status == "in_progress"
    assert len(instance.objectives) == 1
    assert instance.objectives[0].id == "obj_test_001"
    assert instance.rewards[0].amount == 100

    # 4. Verify DB persistence via Repository
    db_inst = QuestRepository.get_instance_by_id(db_session, instance.id)
    assert db_inst is not None
    assert db_inst.title == "철괴 확보 지원"
    assert db_inst.related_main_quest_id == "main_001"

    db_progs = QuestRepository.get_progress_by_instance(db_session, instance.id)
    assert len(db_progs) == 1
    assert db_progs[0].objective_id == "obj_test_001"
    assert db_progs[0].target_amount == 10
    assert db_progs[0].current_amount == 0


def test_complete_and_claim_quest_transitions(db_session: Session) -> None:
    draft = SupportQuestDraft(
        title="구리괴 확보 지원",
        description="구리괴를 확보하세요.",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_test_002",
                type="collect_item",
                target_id="resource_copper_ingot",
                target_amount=10,
                current_amount=0,
                status="in_progress",
            )
        ],
        rewards=[QuestReward(type="currency", target_id="gold", amount=100)],
    )

    # Create
    instance = QuestManager.create_quest_from_draft(
        session=db_session,
        factory_id="factory_001",
        draft=draft,
    )

    # 1. Complete Quest
    completed_instance = QuestManager.complete_quest(db_session, instance.id)
    assert completed_instance is not None
    assert completed_instance.status == "completed"
    assert completed_instance.completed_at is not None

    # Check database
    db_inst = QuestRepository.get_instance_by_id(db_session, instance.id)
    assert db_inst.status == "completed"
    assert db_inst.completed_at is not None

    # 2. Claim Reward
    claimed_instance = QuestManager.claim_reward(db_session, instance.id)
    assert claimed_instance is not None
    assert claimed_instance.status == "reward_claimed"

    # Check database
    db_inst = QuestRepository.get_instance_by_id(db_session, instance.id)
    assert db_inst.status == "reward_claimed"


def test_get_active_instances(db_session: Session) -> None:
    draft1 = SupportQuestDraft(
        title="퀘스트1",
        description="설명1",
        objectives=[
            QuestObjective(
                id="obj_1",
                target_id="resource_wood",
                target_amount=5,
            )
        ],
        rewards=[],
    )
    draft2 = SupportQuestDraft(
        title="퀘스트2",
        description="설명2",
        objectives=[
            QuestObjective(
                id="obj_2",
                target_id="resource_stone",
                target_amount=5,
            )
        ],
        rewards=[],
    )

    # Create two quests for factory_001
    inst1 = QuestManager.create_quest_from_draft(db_session, "factory_001", draft1)
    inst2 = QuestManager.create_quest_from_draft(db_session, "factory_001", draft2)

    # Verify active instances
    active_instances = QuestRepository.get_active_instances(db_session, "factory_001")
    assert len(active_instances) == 2
    active_ids = [inst.id for inst in active_instances]
    assert inst1.id in active_ids
    assert inst2.id in active_ids

    # Complete one quest
    QuestManager.complete_quest(db_session, inst1.id)

    # Only one active quest should remain
    active_instances = QuestRepository.get_active_instances(db_session, "factory_001")
    assert len(active_instances) == 1
    assert active_instances[0].id == inst2.id


def test_invalid_state_transitions(db_session: Session) -> None:
    draft = SupportQuestDraft(
        title="구리괴 확보 지원",
        description="구리괴를 확보하세요.",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_test_003",
                type="collect_item",
                target_id="resource_copper_ingot",
                target_amount=10,
                current_amount=0,
                status="in_progress",
            )
        ],
        rewards=[QuestReward(type="currency", target_id="gold", amount=100)],
    )

    # Create
    instance = QuestManager.create_quest_from_draft(db_session, "factory_001", draft)
    assert instance.status == "in_progress"

    # 1. in_progress -> reward_claimed 시도 (차단되어야 함, None 반환)
    claimed = QuestManager.claim_reward(db_session, instance.id)
    assert claimed is None

    # DB 확인
    db_inst = QuestRepository.get_instance_by_id(db_session, instance.id)
    assert db_inst.status == "in_progress"

    # 2. 퀘스트 완료 처리
    completed_instance = QuestManager.complete_quest(db_session, instance.id)
    assert completed_instance is not None
    assert completed_instance.status == "completed"
    completed_time_1 = completed_instance.completed_at

    # 3. 이미 완료된 퀘스트에 대해 중복 완료 시도 (멱등성: status는 completed 유지, completed_at 유지)
    completed_instance_2 = QuestManager.complete_quest(db_session, instance.id)
    assert completed_instance_2 is not None
    assert completed_instance_2.status == "completed"
    assert completed_instance_2.completed_at == completed_time_1

    # 4. 보상 수령 완료
    claimed_instance = QuestManager.claim_reward(db_session, instance.id)
    assert claimed_instance is not None
    assert claimed_instance.status == "reward_claimed"

    # 5. 이미 보상 수령 완료된 퀘스트 완료 처리 시도 (완료 시각은 변하지 않아야 함, status도 reward_claimed 유지)
    completed_instance_3 = QuestManager.complete_quest(db_session, instance.id)
    assert completed_instance_3 is not None
    assert completed_instance_3.status == "reward_claimed"
    db_inst_after = QuestRepository.get_instance_by_id(db_session, instance.id)
    assert db_inst_after.completed_at == completed_time_1

    # 6. 존재하지 않는 ID로 전이 시도 (None 반환)
    assert QuestManager.complete_quest(db_session, "invalid_id") is None
    assert QuestManager.claim_reward(db_session, "invalid_id") is None


def test_reward_resolver_empty_target() -> None:
    # target_item_id가 빈 문자열일 때의 조기 반환(방어 코드) 검증
    rewards = [QuestReward(type="currency", target_id="gold", amount=100)]
    resolved = QuestRewardResolver.resolve_rewards(rewards, "")
    assert resolved == rewards
