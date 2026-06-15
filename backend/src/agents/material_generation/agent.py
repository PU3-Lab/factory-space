"""LangGraph를 통해 검증, 분류 및 생성을 조정하여 재료 합성 프로세스를 오케스트레이션합니다."""

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
    """LangGraph 서브그래프를 호출하여 플레이어의 합성 시도를 처리하는 에이전트입니다."""

    agent_id = "material_generation"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        """기본 에이전트 필수 메서드입니다. 직접적인 HTTP 흐름에서는 사용되지 않습니다."""
        return "Initiating material synthesis agent."

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        """기본 에이전트 필수 폴백(fallback) 메서드입니다."""
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
        """LangGraph 서브그래프를 호출하여 전체 합성 파이프라인을 실행합니다."""
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
