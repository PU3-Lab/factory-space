"""Orchestrates the material synthesis process, coordinating validation, classification, and generation via LangGraph."""

from __future__ import annotations

import logging
import uuid
from typing import Any

from sqlalchemy.orm import Session

from agents.base import AgentContext, AgentRunResult
from agents.material_generation.graph import material_subgraph
from agents.material_generation.schemas import (
    MaterialCreationRequest,
    MaterialCreationResponse,
)

logger = logging.getLogger(__name__)


class MaterialCreationAgent:
    """Agent that handles player synthesis attempts by invoking a LangGraph subgraph."""

    agent_id = "material_generation"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        """Required base agent method. Unused in direct HTTP flow."""
        return "Initiating material synthesis agent."

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        """Required base agent fallback method."""
        return AgentRunResult(
            agent=self.agent_id,
            payload={
                "result_type": "failed_result",
                "experiment_hash": "fallback",
                "failure_reason": "Orchestrator fallback triggered.",
            },
        )

    def synthesize(
        self,
        session: Session,
        request: MaterialCreationRequest,
    ) -> MaterialCreationResponse:
        """Run the full synthesis pipeline by invoking the LangGraph subgraph."""
        context = AgentContext(
            request_id=f"req_{uuid.uuid4().hex[:10]}",
            session_id=request.player_id,
        )

        initial_state = {
            "db": session,
            "request": request,
            "context": context,
            "normalized_inputs": [],
            "experiment_hash": "",
            "classification": "",
            "proposal": None,
            "attempt": 0,
            "response": None,
            "error": None,
            "similar_context": None,
        }

        logger.info("Invoking material generation subgraph...")
        final_state = material_subgraph.invoke(initial_state)

        response = final_state.get("response")
        if not response:
            logger.error("Subgraph execution did not return a response object.")
            return MaterialCreationResponse(
                result_type="failed_result",
                experiment_hash="failed",
                failure_reason="Sub-graph execution failed to produce a result.",
            )

        return response
