from __future__ import annotations

from collections.abc import Generator

import pytest
from sqlalchemy import create_engine
from sqlalchemy.orm import Session, sessionmaker

from agents.quest_generator.manager import QuestManager
from agents.quest_generator.models import QuestObjective, QuestReward, SupportQuestDraft
from agents.quest_generator.repository import QuestRepository
from agents.quest_generator.tracker import QuestProgressTracker
from db.models import Base


@pytest.fixture
def db_session() -> Generator[Session, None, None]:
    engine = create_engine("sqlite:///:memory:")
    Base.metadata.create_all(bind=engine)
    session_local = sessionmaker(bind=engine)
    session = session_local()
    try:
        yield session
    finally:
        session.close()


def test_track_progress_success(db_session: Session) -> None:
    # 1. Setup draft and create quest
    draft = SupportQuestDraft(
        title="구리괴 확보 지원",
        description="구리괴를 확보하세요.",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_copper_001",
                type="collect_item",
                target_id="resource_copper_ingot",
                target_amount=10,
                current_amount=0,
                status="in_progress",
            )
        ],
        rewards=[QuestReward(type="currency", target_id="gold", amount=100)],
    )
    instance = QuestManager.create_quest_from_draft(db_session, "factory_001", draft)
    assert instance.status == "in_progress"

    # 2. Receive item_collected event (partial amount)
    QuestProgressTracker.track_item_collected(
        session=db_session,
        event_id="evt_001",
        factory_id="factory_001",
        item_id="copper_ingot",
        current_total=5,
    )

    # Verify progress snapshot update
    db_progs = QuestRepository.get_progress_by_instance(db_session, instance.id)
    assert len(db_progs) == 1
    assert db_progs[0].current_amount == 5
    assert db_progs[0].last_event_id == "evt_001"
    assert db_progs[0].status == "in_progress"

    # Verify instance is still in_progress
    db_inst = QuestRepository.get_instance_by_id(db_session, instance.id)
    assert db_inst.status == "in_progress"
    assert db_inst.completed_at is None

    # 3. Receive item_collected event (target amount met)
    QuestProgressTracker.track_item_collected(
        session=db_session,
        event_id="evt_002",
        factory_id="factory_001",
        item_id="copper_ingot",
        current_total=12,  # Over the target of 10
    )

    # Verify progress state
    db_progs = QuestRepository.get_progress_by_instance(db_session, instance.id)
    assert db_progs[0].current_amount == 12
    assert db_progs[0].last_event_id == "evt_002"
    assert db_progs[0].status == "completed"

    # Verify instance state transitions to completed
    db_inst = QuestRepository.get_instance_by_id(db_session, instance.id)
    assert db_inst.status == "completed"
    assert db_inst.completed_at is not None


def test_track_progress_idempotent(db_session: Session) -> None:
    draft = SupportQuestDraft(
        title="철괴 확보 지원",
        description="철괴를 확보하세요.",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_iron_001",
                type="collect_item",
                target_id="resource_iron_ingot",
                target_amount=10,
                current_amount=0,
                status="in_progress",
            )
        ],
        rewards=[QuestReward(type="currency", target_id="gold", amount=100)],
    )
    instance = QuestManager.create_quest_from_draft(db_session, "factory_001", draft)

    # 1. Process event first time (completing the quest)
    QuestProgressTracker.track_item_collected(
        session=db_session,
        event_id="evt_dup_100",
        factory_id="factory_001",
        item_id="iron_ingot",
        current_total=10,
    )
    db_inst_1 = QuestRepository.get_instance_by_id(db_session, instance.id)
    assert db_inst_1.status == "completed"
    completed_time_1 = db_inst_1.completed_at

    # 2. Process duplicate event with different amount (should be ignored by event_id check)
    QuestProgressTracker.track_item_collected(
        session=db_session,
        event_id="evt_dup_100",
        factory_id="factory_001",
        item_id="iron_ingot",
        current_total=5,
    )

    db_progs = QuestRepository.get_progress_by_instance(db_session, instance.id)
    assert db_progs[0].current_amount == 10  # Kept original amount
    db_inst_2 = QuestRepository.get_instance_by_id(db_session, instance.id)
    assert db_inst_2.completed_at == completed_time_1  # Completion time unchanged


def test_track_progress_oldest_first(db_session: Session) -> None:
    # Two identical active quests for iron_ingot
    draft1 = SupportQuestDraft(
        title="철괴 확보 1",
        description="철괴 10개",
        objectives=[
            QuestObjective(
                id="obj_iron_a",
                target_id="resource_iron_ingot",
                target_amount=10,
            )
        ],
        rewards=[],
    )
    draft2 = SupportQuestDraft(
        title="철괴 확보 2",
        description="철괴 10개",
        objectives=[
            QuestObjective(
                id="obj_iron_b",
                target_id="resource_iron_ingot",
                target_amount=10,
            )
        ],
        rewards=[],
    )

    # Create first (older)
    inst1 = QuestManager.create_quest_from_draft(db_session, "factory_001", draft1)
    # Create second (newer)
    inst2 = QuestManager.create_quest_from_draft(db_session, "factory_001", draft2)

    # 1. Process event of 5 items. Only the oldest (inst1) should update.
    QuestProgressTracker.track_item_collected(
        session=db_session,
        event_id="evt_seq_1",
        factory_id="factory_001",
        item_id="iron_ingot",
        current_total=5,
    )

    prog1 = QuestRepository.get_progress_by_instance(db_session, inst1.id)[0]
    prog2 = QuestRepository.get_progress_by_instance(db_session, inst2.id)[0]
    assert prog1.current_amount == 5
    assert prog2.current_amount == 0

    # 2. Process event of 10 items (evt_seq_2). inst1 gets updated to 10 and completed.
    QuestProgressTracker.track_item_collected(
        session=db_session,
        event_id="evt_seq_2",
        factory_id="factory_001",
        item_id="iron_ingot",
        current_total=10,
    )

    prog1_after = QuestRepository.get_progress_by_instance(db_session, inst1.id)[0]
    prog2_after = QuestRepository.get_progress_by_instance(db_session, inst2.id)[0]
    assert prog1_after.current_amount == 10
    assert prog1_after.status == "completed"
    assert (
        prog2_after.current_amount == 0
    )  # inst2 remains untouched because it only routes to active ones and inst1 was still in_progress at the time of matching

    # 3. Process event of 8 items (evt_seq_3) after inst1 is completed. Now inst2 is the oldest *active* quest.
    QuestProgressTracker.track_item_collected(
        session=db_session,
        event_id="evt_seq_3",
        factory_id="factory_001",
        item_id="iron_ingot",
        current_total=8,
    )

    prog2_final = QuestRepository.get_progress_by_instance(db_session, inst2.id)[0]
    assert prog2_final.current_amount == 8
    assert prog2_final.status == "in_progress"


def test_track_progress_idempotent_leak(db_session: Session) -> None:
    # 1. 동일 자원에 대해 활성 상태인 퀘스트 2개 셋업
    draft1 = SupportQuestDraft(
        title="철괴 확보 1",
        description="철괴 10개",
        objectives=[
            QuestObjective(
                id="obj_iron_a",
                target_id="resource_iron_ingot",
                target_amount=10,
            )
        ],
        rewards=[],
    )
    draft2 = SupportQuestDraft(
        title="철괴 확보 2",
        description="철괴 10개",
        objectives=[
            QuestObjective(
                id="obj_iron_b",
                target_id="resource_iron_ingot",
                target_amount=10,
            )
        ],
        rewards=[],
    )

    inst1 = QuestManager.create_quest_from_draft(db_session, "factory_001", draft1)
    inst2 = QuestManager.create_quest_from_draft(db_session, "factory_001", draft2)

    # 2. 첫 번째 이벤트 유입으로 inst1 완료 처리
    QuestProgressTracker.track_item_collected(
        session=db_session,
        event_id="evt_dup_200",
        factory_id="factory_001",
        item_id="iron_ingot",
        current_total=10,
    )

    prog1 = QuestRepository.get_progress_by_instance(db_session, inst1.id)[0]
    prog2 = QuestRepository.get_progress_by_instance(db_session, inst2.id)[0]
    assert prog1.status == "completed"
    assert prog1.current_amount == 10
    assert prog2.status == "in_progress"
    assert prog2.current_amount == 0

    # 3. 동일 event_id를 가진 중복 이벤트 재유입
    # M1 결함이 있다면 inst1이 completed가 되면서 active 쿼리에서 빠져나가고,
    # 중복 이벤트임에도 inst2가 새로운 라우팅 타겟이 되어 inst2의 진행도가 업데이트되거나 완료될 수 있음.
    QuestProgressTracker.track_item_collected(
        session=db_session,
        event_id="evt_dup_200",
        factory_id="factory_001",
        item_id="iron_ingot",
        current_total=10,
    )

    # inst2의 진행도가 0으로 변경되지 않아야 멱등성이 올바르게 유지됨
    prog2_after = QuestRepository.get_progress_by_instance(db_session, inst2.id)[0]
    assert prog2_after.status == "in_progress"
    assert prog2_after.current_amount == 0


def test_track_progress_not_found(db_session: Session) -> None:
    # Try updating progress with no active quests
    QuestProgressTracker.track_item_collected(
        session=db_session,
        event_id="evt_none",
        factory_id="factory_999",
        item_id="copper_ingot",
        current_total=10,
    )
    # Just verify no error occurs
