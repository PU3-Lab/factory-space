"""Database engine configuration for the Factory Space backend (Synchronous)."""

from __future__ import annotations

import os
from collections.abc import Iterator
from contextlib import contextmanager

from sqlalchemy import create_engine
from sqlalchemy.orm import Session, sessionmaker


def get_database_url() -> str:
    """Return the configured sync database URL, substituting psycopg driver for Postgres if needed."""
    url = os.environ.get("DATABASE_URL")
    if not url:
        return "sqlite:///./factory_space.db"

    if url.startswith("postgresql://"):
        url = url.replace("postgresql://", "postgresql+psycopg://", 1)
    return url


DATABASE_URL = get_database_url()

connect_args = {}
if DATABASE_URL.startswith("sqlite"):
    connect_args = {"check_same_thread": False}

engine = create_engine(
    DATABASE_URL,
    echo=False,
    connect_args=connect_args,
)

sync_session_factory = sessionmaker(
    bind=engine,
    class_=Session,
    expire_on_commit=False,
)


@contextmanager
def get_db_session() -> Iterator[Session]:
    """Provide a transactional sync session context."""
    session = sync_session_factory()
    try:
        yield session
        session.commit()
    except Exception:
        session.rollback()
        raise
    finally:
        session.close()
