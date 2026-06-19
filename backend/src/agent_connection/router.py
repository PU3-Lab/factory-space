"""FastAPI connection manifest for Unreal agent clients."""

from __future__ import annotations

from fastapi import APIRouter

from agents.operator_guide.agent import OPERATOR_GUIDE_LEAF_AGENT_IDS
from agents.orchestrator import TOP_LEVEL_AGENT_IDS
from agents.pipeline.graph_edges import SINGLE_LEAF_AGENT_IDS
from agents.quest_generator.agent import QUEST_SUB_AGENT_IDS

router = APIRouter(prefix="/api/v1/agent-connection", tags=["agent-connection"])


@router.get("")
async def get_agent_connection_manifest() -> dict[str, object]:
    """Return the stable backend connection contract for Unreal clients."""

    return {
        "status": "ok",
        "health_path": "/health",
        "websocket_path": "/ws/agent",
        "request_type": "agent.request",
        "response_types": ["agent.response", "agent.error"],
        "top_level_agents": list(TOP_LEVEL_AGENT_IDS),
        "leaf_agents": {
            "process_optimizer": list(SINGLE_LEAF_AGENT_IDS["process_optimizer"]),
            "operator_guide": list(OPERATOR_GUIDE_LEAF_AGENT_IDS),
            "quest_generator": list(QUEST_SUB_AGENT_IDS),
            "material_generation": list(SINGLE_LEAF_AGENT_IDS["material_generation"]),
            "new_material_generator": list(
                SINGLE_LEAF_AGENT_IDS["new_material_generator"]
            ),
        },
        "sample_request": {
            "type": "agent.request",
            "request_id": "unreal-smoke-1",
            "session_id": "dev-session",
            "client_id": "unreal-client",
            "agent": "process_optimizer",
            "payload": {"machines": [{"id": "assembler-1"}]},
        },
    }
