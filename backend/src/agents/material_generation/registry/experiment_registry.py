"""Registry service for saving and retrieving generated experiments."""

from __future__ import annotations

from sqlalchemy import select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from db.models import GeneratedExperimentModel


class ExperimentRegistryService:
    """Handles read and write transactions for the generated_experiments table."""

    @classmethod
    def get_experiment_by_hash(
        cls,
        session: Session,
        experiment_hash: str,
    ) -> GeneratedExperimentModel | None:
        """Find an existing experiment by its unique combination hash."""
        stmt = select(GeneratedExperimentModel).where(
            GeneratedExperimentModel.experiment_hash == experiment_hash
        )
        result = session.execute(stmt)
        return result.scalar_one_or_none()

    @classmethod
    def save_experiment(
        cls,
        session: Session,
        experiment: GeneratedExperimentModel,
    ) -> None:
        """Insert or merge an experiment record in the database."""
        nested = session.begin_nested()
        try:
            session.add(experiment)
            session.flush()
        except IntegrityError:
            nested.rollback()
            # If it already exists, find it by hash and sync its id before merging
            existing = cls.get_experiment_by_hash(session, experiment.experiment_hash)
            if existing:
                experiment.id = existing.id
            session.merge(experiment)
            session.flush()
