"""In-memory session storage for process optimizer agent."""

from __future__ import annotations

from typing import Any


class ProcessOptimizerSessionMemory:
    """Stores the latest factory_state and factoryRevision per session.

    This ensures that when an optimization is requested, we can reference
    the latest state sent during periodic state updates if none is provided
    in the active request payload.
    """

    def __init__(self) -> None:
        self._states: dict[str, dict[str, Any]] = {}
        self._revisions: dict[str, int] = {}

    def get_state(self, session_id: str | None) -> dict[str, Any]:
        """Retrieve the latest factory_state for a session."""
        if not session_id:
            return {}
        return self._states.get(session_id, {})

    def get_revision(self, session_id: str | None) -> int:
        """Retrieve the latest factoryRevision for a session."""
        if not session_id:
            return 0
        return self._revisions.get(session_id, 0)

    def update(
        self,
        session_id: str | None,
        factory_state: dict[str, Any],
        revision: int,
    ) -> None:
        """Update the latest state and revision for a session."""
        if not session_id:
            return
        self._states[session_id] = factory_state
        self._revisions[session_id] = revision

    def clear(self, session_id: str | None) -> None:
        """Clear memory for a session."""
        if not session_id:
            return
        self._states.pop(session_id, None)
        self._revisions.pop(session_id, None)


# Global process optimizer memory singleton
process_optimizer_memory = ProcessOptimizerSessionMemory()
