"""Registry service for managing discovered and generated materials."""

from __future__ import annotations

from sqlalchemy import select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from db.models import GeneratedMaterialDiscoveryModel, GeneratedMaterialModel


class MaterialRegistryService:
    """Handles query and persist operations for materials and player discovery records."""

    @classmethod
    def get_material_by_hash(
        cls,
        session: Session,
        material_hash: str,
    ) -> GeneratedMaterialModel | None:
        """Retrieve a material by its structural property hash."""
        stmt = select(GeneratedMaterialModel).where(
            GeneratedMaterialModel.material_hash == material_hash
        )
        result = session.execute(stmt)
        return result.scalar_one_or_none()

    @classmethod
    def save_material(
        cls,
        session: Session,
        material: GeneratedMaterialModel,
    ) -> None:
        """Save a new material profile to the database."""
        nested = session.begin_nested()
        try:
            session.add(material)
            session.flush()
        except IntegrityError:
            nested.rollback()
            session.merge(material)
            session.flush()

    @classmethod
    def record_discovery(
        cls,
        session: Session,
        discovery_id: str,
        material_id: str,
        player_id: str,
        experiment_hash: str,
    ) -> None:
        """Create a discovery association tracking when a player first synthesizes this material."""
        discovery = GeneratedMaterialDiscoveryModel(
            id=discovery_id,
            material_id=material_id,
            player_id=player_id,
            experiment_hash=experiment_hash,
        )
        session.add(discovery)
        session.flush()
