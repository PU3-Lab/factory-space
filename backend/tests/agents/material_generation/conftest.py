"""재료 생성 에이전트 테스트를 위한 Pytest conftest 설정입니다."""

from __future__ import annotations

from collections.abc import Iterator

import pytest
from sqlalchemy import create_engine
from sqlalchemy.orm import Session, sessionmaker
from sqlalchemy.pool import StaticPool

from agents.material_generation.events import MaterialEventPublisher
from agents.material_generation.recipe_repository import RecipeRepository
from agents.material_generation.visual_pipeline import VisualAssetPipeline
from db.models import (
    GeneratedExperimentModel,
    GeneratedMaterialDiscoveryModel,
    GeneratedMaterialModel,
    RecipeModel,
)


@pytest.fixture
def db_session() -> Iterator[Session]:
    """기본 테스트 레시피가 미리 채워진 인메모리 SQLite 세션을 제공합니다."""
    MaterialEventPublisher.reset_executor(wait=False)
    engine = create_engine(
        "sqlite:///:memory:",
        echo=False,
        connect_args={"check_same_thread": False},
        poolclass=StaticPool,
    )

    # 동기식으로 테이블 생성
    RecipeModel.__table__.create(engine)
    GeneratedExperimentModel.__table__.create(engine)
    GeneratedMaterialModel.__table__.create(engine)
    GeneratedMaterialDiscoveryModel.__table__.create(engine)

    session_factory = sessionmaker(bind=engine, expire_on_commit=False, class_=Session)

    VisualAssetPipeline.session_factory = session_factory

    session = session_factory()
    # 실제 데이터와 일치하는 샘플 레시피 등록
    session.add(
        RecipeModel(
            recipe_name="Smelt_Iron",
            machine_type="Smelter",
            input_item_1="iron_ore",
            input_qty_1=2,
            output_item_1="iron_ingot",
            output_qty_1=1,
            crafting_time=3.0,
        )
    )
    session.add(
        RecipeModel(
            recipe_name="Crush_Iron",
            machine_type="Grinder",
            input_item_1="iron_ingot",
            input_qty_1=1,
            output_item_1="iron_powder",
            output_qty_1=2,
            crafting_time=1.5,
        )
    )
    session.commit()

    # 레시피 레포지토리 캐시 시딩(Seeding)
    RecipeRepository.reload_cache(session)
    yield session

    session.close()

    # 테스트 세션 생명주기 내에서 백그라운드 작업이 완료될 때까지 대기
    MaterialEventPublisher.wait_for_jobs()
    VisualAssetPipeline.session_factory = None

    engine.dispose()
