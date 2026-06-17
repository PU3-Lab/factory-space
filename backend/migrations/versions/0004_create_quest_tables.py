"""create_quest_tables

Revision ID: 0004_create_quest_tables
Revises: 0003_add_material_state
Create Date: 2026-06-16 00:00:00.000000

"""

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

# revision identifiers, used by Alembic.
revision: str = "0004_create_quest_tables"
down_revision: str | None = "0003_manual_rag_ingest"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    """데이터베이스 업그레이드 마이그레이션:

    지원 퀘스트 진행 상태를 추적하기 위해
    quest_instances 및 quest_progress 테이블을 생성합니다.
    """
    op.create_table(
        "quest_instances",
        sa.Column("id", sa.String(length=100), nullable=False),
        sa.Column("factory_id", sa.String(length=100), nullable=False),
        sa.Column("quest_type", sa.String(length=50), nullable=False),
        sa.Column("support_type", sa.String(length=50), nullable=False),
        sa.Column("related_main_quest_id", sa.String(length=100), nullable=True),
        sa.Column("title", sa.String(length=255), nullable=False),
        sa.Column("description", sa.Text(), nullable=False),
        sa.Column(
            "status",
            sa.String(length=50),
            server_default="in_progress",
            nullable=False,
        ),
        sa.Column("objective_json", sa.JSON(), nullable=False),
        sa.Column("reward_json", sa.JSON(), nullable=False),
        sa.Column(
            "created_by", sa.String(length=50), server_default="rule", nullable=False
        ),
        sa.Column(
            "level_reward_applied",
            sa.Boolean(),
            server_default="false",
            nullable=False,
        ),
        sa.Column(
            "created_at",
            sa.DateTime(timezone=True),
            server_default=sa.func.now(),
            nullable=False,
        ),
        sa.Column("completed_at", sa.DateTime(timezone=True), nullable=True),
        sa.PrimaryKeyConstraint("id"),
    )

    # M1. quest_instances 테이블 인덱스 추가
    op.create_index(
        "ix_quest_instances_factory_id",
        "quest_instances",
        ["factory_id"],
    )
    op.create_index(
        "ix_quest_instances_factory_status",
        "quest_instances",
        ["factory_id", "status"],
    )

    op.create_table(
        "quest_progress",
        sa.Column("id", sa.String(length=100), nullable=False),
        sa.Column("quest_instance_id", sa.String(length=100), nullable=False),
        sa.Column("objective_id", sa.String(length=100), nullable=False),
        sa.Column("objective_type", sa.String(length=50), nullable=False),
        sa.Column("target_id", sa.String(length=100), nullable=False),
        sa.Column(
            "current_amount",
            sa.Integer(),
            server_default="0",
            nullable=False,
        ),
        sa.Column("target_amount", sa.Integer(), nullable=False),
        sa.Column(
            "status",
            sa.String(length=50),
            server_default="in_progress",
            nullable=False,
        ),
        sa.Column("last_event_id", sa.String(length=100), nullable=True),
        sa.ForeignKeyConstraint(
            ["quest_instance_id"],
            ["quest_instances.id"],
            ondelete="CASCADE",
        ),
        sa.UniqueConstraint(
            "quest_instance_id",
            "objective_id",
            name="uq_quest_progress_instance_objective",
        ),
        sa.PrimaryKeyConstraint("id"),
    )

    # M1. quest_progress 테이블 인덱스 추가
    op.create_index(
        "ix_quest_progress_quest_instance_id",
        "quest_progress",
        ["quest_instance_id"],
    )
    op.create_index(
        "ix_quest_progress_last_event_id",
        "quest_progress",
        ["last_event_id"],
    )


def downgrade() -> None:
    """데이터베이스 다운그레이드 마이그레이션:

    quest_progress 및 quest_instances 테이블을 제거합니다.
    """
    op.drop_table("quest_progress")
    op.drop_table("quest_instances")
