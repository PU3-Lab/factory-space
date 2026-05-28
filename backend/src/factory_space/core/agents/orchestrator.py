"""Agent orchestration."""

from __future__ import annotations

from typing import TypedDict, cast

from langgraph.graph import END, START, StateGraph
from langgraph.graph.state import CompiledStateGraph

from factory_space.core.agents.registry import AgentRegistry
from factory_space.core.state.context import AgentContext
from factory_space.messages.protocol import AgentRequest, AgentResponse


class OrchestratorState(TypedDict, total=False):
    """State passed between orchestrator graph nodes."""

    request: AgentRequest
    context: AgentContext
    response: AgentResponse


class AgentOrchestrator:
    """Coordinates request execution for registered agents via LangGraph."""

    def __init__(self, registry: AgentRegistry) -> None:
        self._registry = registry
        self._graph: CompiledStateGraph[
            OrchestratorState,
            None,
            OrchestratorState,
            OrchestratorState,
        ] = self._build_graph()

    async def process(self, request: AgentRequest) -> AgentResponse:
        """Run the orchestration graph for one agent request."""

        result = cast(
            OrchestratorState,
            await self._graph.ainvoke({"request": request}),
        )
        return result["response"]

    def _build_graph(
        self,
    ) -> CompiledStateGraph[
        OrchestratorState,
        None,
        OrchestratorState,
        OrchestratorState,
    ]:
        graph = StateGraph(OrchestratorState)
        graph.add_node("build_context", self._build_context)
        graph.add_node("dispatch_agent", self._dispatch_agent)
        graph.add_edge(START, "build_context")
        graph.add_edge("build_context", "dispatch_agent")
        graph.add_edge("dispatch_agent", END)
        return graph.compile()

    def _build_context(self, state: OrchestratorState) -> OrchestratorState:
        request = state["request"]
        context = AgentContext(
            session_id=request.session_id,
            client_id=request.client_id,
            request_id=request.request_id,
        )
        return {"context": context}

    async def _dispatch_agent(self, state: OrchestratorState) -> OrchestratorState:
        request = state["request"]
        context = state["context"]
        agent = self._registry.get(request.agent)
        response = await agent.process(request, context)
        return {"response": response}
