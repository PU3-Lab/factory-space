"""add generated material visual color

Revision ID: 0005_add_material_visual_color
Revises: 0004_create_quest_tables
Create Date: 2026-07-01 00:00:00.000000

"""

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

# revision identifiers, used by Alembic.
revision: str = "0005_add_material_visual_color"
down_revision: str | None = "0004_create_quest_tables"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    """Store the Unreal RGBA color extracted from a generated texture."""
    op.add_column(
        "generated_materials",
        sa.Column("visual_color", sa.String(length=50), nullable=True),
    )


def downgrade() -> None:
    """Remove the generated texture color column."""
    op.drop_column("generated_materials", "visual_color")
