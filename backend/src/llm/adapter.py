"""LLM provider adapters."""

from __future__ import annotations


class LlmAdapter:
    """LLM adapter placeholder for the LangGraph pipeline."""

    def invoke(self, prompt: str) -> str | None:
        """Return raw model output.

        The concrete provider integration will be added after the graph contract is
        stable. Returning None forces the pipeline through deterministic fallback.
        """

        return None
