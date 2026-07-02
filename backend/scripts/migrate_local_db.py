"""Apply Alembic migrations to the local ``factory_space.db`` SQLite file.

``run_server.py`` (production-style local run) does not auto-migrate on startup,
and ``run_dev_server.py`` targets a separate ``factory_space.test.db`` instead.
Use this script to bring ``factory_space.db`` up to date after pulling new
migrations.
"""

from __future__ import annotations

import os
from pathlib import Path

from alembic import command
from alembic.config import Config

LOCAL_DATABASE_URL = "sqlite:///./factory_space.db"


def main() -> None:
    backend_root = Path(__file__).resolve().parents[1]
    os.chdir(backend_root)
    os.environ.setdefault("DATABASE_URL", LOCAL_DATABASE_URL)

    config = Config(str(backend_root / "alembic.ini"))
    db_url = os.environ.get("DATABASE_URL")
    if db_url is not None:
        os.environ["FACTORY_ALEMBIC_DATABASE_URL"] = db_url
    print(f"[Migrate] Applying migrations (alembic upgrade head) on {db_url}")
    command.upgrade(config, "head")


if __name__ == "__main__":
    main()
