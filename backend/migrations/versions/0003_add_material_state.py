"""add_material_state

Revision ID: 0003_add_material_state
Revises: 0002_create_material_tables
Create Date: 2026-06-15 00:00:00.000000

"""

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

# revision identifiers, used by Alembic.
revision: str = "0003_add_material_state"
down_revision: str | None = "0002_create_material_tables"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    """데이터베이스 업그레이드 마이그레이션:

    generated_materials 테이블에 물리적 상태를 저장할 'state' 컬럼을 추가합니다.
    기본값은 'solid'로 설정합니다.
    """
    op.add_column(
        "generated_materials",
        sa.Column(
            "state",
            sa.String(length=20),
            server_default="solid",
            nullable=False,
        ),
    )


def downgrade() -> None:
    """데이터베이스 다운그레이드 마이그레이션:

    generated_materials 테이블에서 'state' 컬럼을 제거합니다.
    """
    op.drop_column("generated_materials", "state")
