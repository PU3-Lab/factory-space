"""Repository for managing QuestInstance and QuestProgress persistence."""

from __future__ import annotations

from sqlalchemy import select
from sqlalchemy.orm import Session

from db.models import QuestInstanceModel, QuestProgressModel


class QuestRepository:
    """QuestInstanceModel 및 QuestProgressModel에 대한 데이터베이스 읽기 및 쓰기 작업을 담당합니다."""

    @classmethod
    def save_instance(cls, session: Session, instance: QuestInstanceModel) -> None:
        """새 퀘스트 인스턴스를 데이터베이스에 저장합니다."""
        session.add(instance)
        session.flush()

    @classmethod
    def save_progress(cls, session: Session, progress: QuestProgressModel) -> None:
        """새 퀘스트 진행도 레코드를 데이터베이스에 저장합니다."""
        session.add(progress)
        session.flush()

    @classmethod
    def get_instance_by_id(
        cls, session: Session, instance_id: str
    ) -> QuestInstanceModel | None:
        """인스턴스 ID로 퀘스트 인스턴스를 조회합니다."""
        stmt = select(QuestInstanceModel).where(QuestInstanceModel.id == instance_id)
        result = session.execute(stmt)
        return result.scalar_one_or_none()

    @classmethod
    def get_active_instances(
        cls, session: Session, factory_id: str
    ) -> list[QuestInstanceModel]:
        """해당 공장의 활성화된(in_progress) 퀘스트 인스턴스 목록을 조회합니다."""
        stmt = (
            select(QuestInstanceModel)
            .where(
                QuestInstanceModel.factory_id == factory_id,
                QuestInstanceModel.status == "in_progress",
            )
            .order_by(QuestInstanceModel.created_at)
        )
        result = session.execute(stmt)
        return list(result.scalars())

    @classmethod
    def get_all_instances(
        cls, session: Session, factory_id: str
    ) -> list[QuestInstanceModel]:
        """해당 공장의 모든 퀘스트 인스턴스 목록을 조회합니다."""
        stmt = (
            select(QuestInstanceModel)
            .where(QuestInstanceModel.factory_id == factory_id)
            .order_by(QuestInstanceModel.created_at)
        )
        result = session.execute(stmt)
        return list(result.scalars())

    @classmethod
    def get_progress_by_instance(
        cls, session: Session, instance_id: str
    ) -> list[QuestProgressModel]:
        """특정 퀘스트 인스턴스 하위의 모든 진행도 데이터를 조회합니다."""
        stmt = select(QuestProgressModel).where(
            QuestProgressModel.quest_instance_id == instance_id
        )
        result = session.execute(stmt)
        return list(result.scalars())
