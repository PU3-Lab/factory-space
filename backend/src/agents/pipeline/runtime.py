"""Common AI agent execution pipeline."""

from __future__ import annotations

import json
from collections.abc import Callable
from dataclasses import replace
from typing import Any

from langchain_core.runnables import RunnableConfig
from langgraph.graph import StateGraph
from langgraph.graph.state import CompiledStateGraph
from pydantic import ValidationError

from agents.base import AgentContext
from agents.material_generation.agent import MaterialCreationAgent
from agents.material_generation.schemas import MaterialCreationRequest
from agents.operator_guide.agent import (
    OPERATOR_GUIDE_LEAF_AGENT_IDS,
    OperatorGuideAgent,
)
from agents.operator_guide.answer_sanitizer import (
    sanitize_operator_guide_response_payload,
)
from agents.operator_guide.session_memory import (
    OPERATOR_GUIDE_RECENT_CONVERSATION_KEY,
    OperatorGuideSessionMemory,
)
from agents.orchestrator import TOP_LEVEL_AGENT_IDS, OrchestratorAgent
from agents.pipeline.graph_edges import wire_agent_graph
from agents.pipeline.llm_fallback import (
    LLMCallSlot,
    build_llm_call_slots,
    invoke_llm_call_slot,
)
from agents.pipeline.middleware import (
    append_middleware_log,
    build_current_model_metadata,
)
from agents.pipeline.state import AgentGraphState
from agents.pipeline.tool_node import (
    append_tool_metadata,
    build_agent_tool_node,
    build_tool_followup_prompt,
    build_tool_node_input,
)
from agents.pipeline.utils import (
    build_cache_key,
    build_validation_error,
    run_fallback,
)
from agents.process_optimizer.graph import compile_process_optimizer_graph
from agents.process_optimizer.middleware import (
    build_graph_payload_with_memory,
    build_state_update_response,
    build_subquest_check_response,
    validate_process_optimizer_payload,
)
from agents.process_optimizer.prompts import apply_process_optimizer_llm_explanation
from agents.quest_generator.agent import QUEST_SUB_AGENT_IDS, QuestGeneratorAgent
from agents.quest_generator.tools import PRODUCTION_QUEST_SELECTION_TOOL_NAME
from agents.router import AgentRouter, UnknownAgentError, create_default_agent_router
from cache.response_cache import ResponseCache
from db.engine import get_db_session
from llm.adapter import LLMAdapter, create_llm_adapter
from llm.settings import LLMModelSlot, LLMSettings
from protocol.errors import build_error_payload
from protocol.messages import (
    AgentErrorEnvelope,
    AgentRequestEnvelope,
    AgentResponseEnvelope,
)


class _FallbackRoutingLLM:
    """LLM adapter wrapper that tries multiple slots sequentially on failure."""

    def __init__(self, slots: list[LLMCallSlot]) -> None:
        self.slots = slots

    def invoke(self, prompt: str) -> str | None:
        for slot in self.slots:
            res = slot.adapter.invoke(prompt)
            if res is not None:
                return res
        return None


def _clean_routing_decision(raw: str | None) -> str:
    """Clean raw routing model output by stripping whitespace and outer JSON quotes."""
    if not raw:
        return ""
    cleaned = raw.strip()
    if len(cleaned) >= 2 and cleaned[0] == cleaned[-1] and cleaned[0] in {'"', "'"}:
        cleaned = cleaned[1:-1]
    return cleaned.strip()


def _routing_failure_reason(raw: str | None) -> str:
    return "invalid_model_decision" if raw else "missing_model_decision"


def _allowed_leaf_agent_ids(selected_agent: str | None) -> list[str]:
    if selected_agent == "operator_guide":
        return list(OPERATOR_GUIDE_LEAF_AGENT_IDS)
    if selected_agent == "quest_generator":
        return list(QUEST_SUB_AGENT_IDS)
    if selected_agent in {"material_generation", "process_optimizer"}:
        return [selected_agent]
    return []


def _build_routing_error_details(
    state: AgentGraphState,
    envelope: AgentRequestEnvelope,
) -> dict[str, Any]:
    selected_agent = state.get("selectedAgent")
    routing_raw = state.get("routingRaw")
    if selected_agent not in TOP_LEVEL_AGENT_IDS:
        return {
            "scope": "top_level",
            "reason": _routing_failure_reason(routing_raw),
            "requestedAgent": envelope.agent,
            "routingRawPresent": bool(routing_raw),
            "allowedAgentIds": list(TOP_LEVEL_AGENT_IDS),
        }

    selected_leaf_agent = state.get("selectedLeafAgent")
    allowed_leaf_agent_ids = _allowed_leaf_agent_ids(selected_agent)
    return {
        "scope": "leaf",
        "reason": _routing_failure_reason(routing_raw),
        "requestedAgent": envelope.agent,
        "selectedAgent": selected_agent,
        "selectedLeafAgentPresent": bool(selected_leaf_agent),
        "routingRawPresent": bool(routing_raw),
        "allowedLeafAgentIds": allowed_leaf_agent_ids,
    }


def build_context(
    state: AgentGraphState,
    config: RunnableConfig,
) -> AgentGraphState:
    """파이프라인 시작 시 실행 요청 봉투(Envelope)로부터 에이전트 실행 컨텍스트를 구성하는 함수.

    초보자용 설명:
        클라이언트(Unreal 등)가 보낸 요청 정보를 기반으로 가공하기 적절한
        공통 AgentContext 객체를 만들고, 진행 상태 메시지 전송용 콜백(on_progress)을 바인딩합니다.
    """

    envelope = state["envelope"]
    on_progress = None
    if config and isinstance(config, dict) and "configurable" in config:
        on_progress = config["configurable"].get("on_progress")

    return {
        "context": AgentContext(
            request_id=envelope.request_id,
            session_id=envelope.session_id,
            client_id=envelope.client_id,
            metadata=envelope.context,
            on_progress=on_progress,
        ),
        "typedPayload": envelope.payload,
        "streams": [],
    }


class AgentPipeline:
    """LangGraph-backed execution pipeline for agent requests."""

    def __init__(
        self,
        *,
        router: AgentRouter | None = None,
        cache: ResponseCache | None = None,
        llm: LLMAdapter | None = None,
        llm_settings: LLMSettings | None = None,
        llm_adapter_factory: Callable[[LLMModelSlot], LLMAdapter] = create_llm_adapter,
        tts_service: Any = None,  # noqa: ANN401
    ) -> None:
        self.router = router or create_default_agent_router()
        self.cache = cache or ResponseCache()
        self.llm = llm
        self.llm_settings = llm_settings
        self.llm_adapter_factory = llm_adapter_factory
        
        if tts_service is None:
            from tts.service import TTSService
            self.tts_service = TTSService.from_env()
        else:
            self.tts_service = tts_service
            
        self.graph = self._build_graph()

    def run(
        self,
        message: AgentRequestEnvelope | dict[str, Any],
        on_progress: Callable[[str, str], None] | None = None,
    ) -> dict[str, Any]:
        """Run one request through the compiled graph."""

        try:
            envelope = (
                message
                if isinstance(message, AgentRequestEnvelope)
                else AgentRequestEnvelope.model_validate(message)
            )
        except ValidationError as exc:
            return build_validation_error(exc, message)

        state = self.graph.invoke(
            {"envelope": envelope},
            config={"configurable": {"on_progress": on_progress}},
        )
        return state["responseEnvelope"]

    def _attach_tts(self, agent: str, payload: dict[str, Any]) -> dict[str, Any]:
        """에이전트 응답 페이로드에 TTS 메타데이터를 결합합니다.
        
        초보자용 설명:
            응답을 최종 반환하기 직전, 대상 에이전트(operator_guide, process_optimizer)가 맞고
            기존에 생성된 tts 메타데이터가 없는 경우, 오디오 합성 서비스(TTSService)를 실행하여
            결과 정보(audio_url, cached 여부 등)를 페이로드에 'tts' 필드로 추가합니다.
        """
        if agent not in {"operator_guide", "process_optimizer"}:
            return payload

        # process_optimizer의 경우 highlight-only(state_update 등) 응답에는 TTS를 생성하지 않음 (오류/불일치 방지)
        if agent == "process_optimizer" and payload.get("status") != "preview":
            return payload

        # 4차/5차 재리뷰 보강: tts.text만 포함된 경우는 입력 힌트로 취급하여 합성을 수행하고,
        # 완성된 메타데이터(ready + audio_url 또는 failed/disabled 단말 상태)는 그대로 유지
        has_completed_tts = False
        if "tts" in payload and isinstance(payload["tts"], dict):
            tts_data = payload["tts"]
            status = tts_data.get("status")
            if status == "ready" and tts_data.get("audio_url"):
                has_completed_tts = True
            elif status in {"failed", "disabled"}:
                has_completed_tts = True

        if has_completed_tts:
            return payload

        new_payload = dict(payload)

        from tts.schemas import TTSRequest
        try:
            req = TTSRequest(agent=agent, payload=payload)
            tts_meta = self.tts_service.synthesize_for_payload(req)
            if hasattr(tts_meta, "model_dump"):
                new_payload["tts"] = tts_meta.model_dump(mode="json")
            else:
                new_payload["tts"] = tts_meta
        except Exception:
            provider_name = "edge_tts"
            if hasattr(self.tts_service, "settings") and hasattr(self.tts_service.settings, "provider"):
                provider_name = self.tts_service.settings.provider
            new_payload["tts"] = {
                "status": "failed",
                "provider": provider_name,
                "error_code": "TTS_PROVIDER_ERROR",
            }

        return new_payload

    def _prepare_process_optimizer_tts_payload(self, payload: dict[str, Any]) -> dict[str, Any]:
        """process_optimizer 응답을 가공하여 display_message와 TTS 텍스트를 정렬합니다.
        
        초보자용 설명:
            화면에 표시할 요약 텍스트인 display_message가 설정되어 있지 않고,
            최적화 경보가 발견되지 않았다면 기본 메시지("문제가 발견되지 않았습니다.")를 채워줍니다.
        """
        new_payload = dict(payload)
        
        if new_payload.get("status") != "preview":
            return new_payload
            
        if "display_message" in new_payload:
            return new_payload
            
        tts_dict = new_payload.get("tts")
        if isinstance(tts_dict, dict) and tts_dict.get("text"):
            return new_payload
            
        opt_alert = new_payload.get("optimization_alert")
        is_alert_needed = False
        if isinstance(opt_alert, dict):
            is_alert_needed = bool(opt_alert.get("needed"))
            
        if not is_alert_needed:
            new_payload["display_message"] = "문제가 발견되지 않았습니다."
        else:
            problem = opt_alert.get("problem") or ""
            action = opt_alert.get("recommended_action") or ""
            parts = [p.strip() for p in (problem, action) if p.strip()]
            if parts:
                new_payload["display_message"] = " ".join(parts)
                
        return new_payload

    def _build_graph(self) -> CompiledStateGraph:
        """Build and compile the LangGraph agent pipeline."""

        agent_router = self.router
        response_cache = self.cache
        llm_slots = build_llm_call_slots(
            llm=self.llm,
            settings=self.llm_settings,
            adapter_factory=self.llm_adapter_factory,
        )
        llm_slots_by_name = {slot.name: slot for slot in llm_slots}
        routing_llm = self.llm or _FallbackRoutingLLM(list(llm_slots))
        orchestrator = OrchestratorAgent()
        operator_guide = OperatorGuideAgent()
        operator_guide_memory = OperatorGuideSessionMemory()
        quest_generator = QuestGeneratorAgent()

        def log_agent_started(state: AgentGraphState) -> AgentGraphState:
            return append_middleware_log(
                state,
                "agent.middleware.before",
                "agent_started",
                {
                    "selectedAgent": state["selectedAgent"],
                    "selectedLeafAgent": state["selectedLeafAgent"],
                },
            )

        def validate_envelope(state: AgentGraphState) -> AgentGraphState:
            envelope = state["envelope"]
            if envelope.type != "agent.request":
                return {
                    "error": build_error_payload(
                        "INVALID_MESSAGE_TYPE",
                        "Only agent.request messages can enter the agent pipeline.",
                        details={"type": envelope.type},
                    )
                }
            if not isinstance(envelope.payload, dict):
                return {
                    "error": build_error_payload(
                        "INVALID_PAYLOAD",
                        "Agent request payload must be an object.",
                    )
                }
            return {}

        def route_top_agent(state: AgentGraphState) -> AgentGraphState:
            if state.get("error"):
                return {}

            envelope = state["envelope"]
            context = state["context"]
            payload = state["typedPayload"]

            if envelope.agent in {"operator_guide", "process_optimizer"}:
                return {
                    "routingPrompt": "",
                    "routingRaw": envelope.agent,
                    "selectedAgent": envelope.agent,
                }

            routing_prompt = orchestrator.build_routing_prompt(
                payload,
                context,
                requested_agent=envelope.agent,
            )
            routing_raw = routing_llm.invoke(routing_prompt)
            return {
                "routingPrompt": routing_prompt,
                "routingRaw": routing_raw,
                "selectedAgent": _clean_routing_decision(routing_raw),
            }

        def validate_process_payload(state: AgentGraphState) -> AgentGraphState:
            error = validate_process_optimizer_payload(
                state["typedPayload"],
                state["context"],
            )
            if error:
                return {"error": error}
            return {"selectedLeafAgent": "process_optimizer"}

        def process_optimizer_state_update(state: AgentGraphState) -> AgentGraphState:
            return build_state_update_response(state["context"], state["typedPayload"])

        def process_optimizer_subquest_check(
            state: AgentGraphState,
        ) -> AgentGraphState:
            return build_subquest_check_response(
                state["context"], state["typedPayload"]
            )

        def process_optimizer_v2_graph(state: AgentGraphState) -> AgentGraphState:
            context = state["context"]
            envelope = state["envelope"]
            payload = build_graph_payload_with_memory(context, state["typedPayload"])

            graph_result = compile_process_optimizer_graph().invoke(
                {
                    "payload": payload,
                    "context": envelope.context,
                    "session_id": context.session_id,
                }
            )
            response_payload = graph_result.get("previewPayload")
            if not isinstance(response_payload, dict):
                return {
                    "responsePayload": {
                        "status": "error",
                        "factoryRevision": payload.get("factoryRevision") or 0,
                        "goal": payload.get("goal") or "balance",
                        "summary": (
                            "Process optimizer v2 graph did not return a response "
                            "payload."
                        ),
                        "changes": [],
                        "suggestions": [],
                        "expected_effect": {},
                        "ui_hints": {"highlight_targets": []},
                    }
                }
            response_payload, response_metadata = (
                apply_process_optimizer_llm_explanation(response_payload, llm_slots)
            )
            return {
                "responsePayload": response_payload,
                "responseMetadata": response_metadata,
            }

        def route_new_material_sub_agent(state: AgentGraphState) -> AgentGraphState:
            explicit_sub_agent = state["typedPayload"].get("sub_agent")
            if (
                explicit_sub_agent is not None
                and explicit_sub_agent != "new_material_generator"
            ):
                return {
                    "error": build_error_payload(
                        "INVALID_SUB_AGENT",
                        "Explicit sub_agent is not valid for new_material_generator.",
                        details={"sub_agent": explicit_sub_agent},
                    )
                }
            return {"selectedLeafAgent": "new_material_generator"}

        def synthesize_material(state: AgentGraphState) -> AgentGraphState:
            if state.get("error"):
                return {}

            payload = state["typedPayload"]
            explicit_sub_agent = payload.get("sub_agent")
            if (
                explicit_sub_agent is not None
                and explicit_sub_agent != "material_generation"
            ):
                return {
                    "error": build_error_payload(
                        "INVALID_SUB_AGENT",
                        "Explicit sub_agent is not valid for material_generation.",
                        details={"sub_agent": explicit_sub_agent},
                    )
                }

            try:
                envelope = state["envelope"]
                req_data = {
                    "machine_type": payload.get("machine_type"),
                    "inputs": payload.get("inputs"),
                    "process_conditions": payload.get("process_conditions") or {},
                    "player_id": envelope.session_id or "default_player",
                    "generate_visual_asset": payload.get("generate_visual_asset", True),
                }
                req = MaterialCreationRequest.model_validate(req_data)
            except Exception as exc:
                return {
                    "error": build_error_payload(
                        "INVALID_REQUEST_PAYLOAD",
                        f"Request payload validation failed: {exc}",
                    )
                }
            agent = MaterialCreationAgent()
            try:
                with get_db_session() as db:
                    response = agent.synthesize(db, req)
            except Exception as exc:
                return {
                    "error": build_error_payload(
                        "SYNTHESIS_ERROR",
                        f"Failed to synthesize material: {exc}",
                    )
                }

            return {
                "selectedLeafAgent": "material_generation",
                "responsePayload": response.model_dump(),
            }

        def route_operator_guide_sub_agent(state: AgentGraphState) -> AgentGraphState:
            explicit_sub_agent = state["typedPayload"].get("sub_agent")
            if explicit_sub_agent is not None:
                if (
                    isinstance(explicit_sub_agent, str)
                    and explicit_sub_agent in OPERATOR_GUIDE_LEAF_AGENT_IDS
                ):
                    return {"selectedLeafAgent": explicit_sub_agent}
                return {
                    "error": build_error_payload(
                        "INVALID_SUB_AGENT",
                        "Explicit sub_agent is not valid for operator_guide.",
                        details={"sub_agent": explicit_sub_agent},
                    )
                }

            routing_prompt = operator_guide.build_routing_prompt(
                state["typedPayload"],
                state["context"],
            )
            routing_raw = routing_llm.invoke(routing_prompt)
            return {
                "routingPrompt": routing_prompt,
                "routingRaw": routing_raw,
                "selectedLeafAgent": _clean_routing_decision(routing_raw),
            }

        def route_quest_sub_agent(state: AgentGraphState) -> AgentGraphState:
            explicit_sub_agent = state["typedPayload"].get("sub_agent")
            if explicit_sub_agent is not None:
                if (
                    isinstance(explicit_sub_agent, str)
                    and explicit_sub_agent in QUEST_SUB_AGENT_IDS
                ):
                    return {"selectedLeafAgent": explicit_sub_agent}
                return {
                    "error": build_error_payload(
                        "INVALID_SUB_AGENT",
                        "Explicit sub_agent is not valid for quest_generator.",
                        details={"sub_agent": explicit_sub_agent},
                    )
                }

            routing_prompt = quest_generator.build_routing_prompt(
                state["typedPayload"],
                state["context"],
            )
            routing_raw = routing_llm.invoke(routing_prompt)
            return {
                "routingPrompt": routing_prompt,
                "routingRaw": routing_raw,
                "selectedLeafAgent": _clean_routing_decision(routing_raw),
            }

        def cache_lookup(state: AgentGraphState) -> AgentGraphState:
            cache_key = build_cache_key(
                state["selectedAgent"],
                state["selectedLeafAgent"],
                state["typedPayload"],
                state["context"],
            )
            cached_entry = response_cache.get_entry(cache_key)
            output: AgentGraphState = {"cacheKey": cache_key}
            if cached_entry is not None:
                output["cachedPayload"] = cached_entry.payload
                output["cachedMetadata"] = cached_entry.metadata
            return output

        def build_cached_response(state: AgentGraphState) -> AgentGraphState:
            cached_payload = state["cachedPayload"]
            if state.get("selectedAgent") == "operator_guide":
                cached_payload = sanitize_operator_guide_response_payload(
                    cached_payload
                )

            return {
                "responsePayload": cached_payload,
                "responseMetadata": {
                    **state.get("cachedMetadata", {}),
                    "cache": "hit",
                },
            }

        def operator_guide_memory_context(
            state: AgentGraphState,
        ) -> tuple[AgentContext, dict[str, Any]]:
            context = state["context"]
            if state.get("selectedAgent") != "operator_guide":
                return context, {}

            question = str(
                state["typedPayload"].get("question")
                or state["typedPayload"].get("message")
                or ""
            )
            operator_guide_memory.update_facts_from_question(
                context.session_id, question
            )

            recent_turns = operator_guide_memory.recent_turns(context.session_id)
            confirmed_facts = operator_guide_memory.confirmed_facts(context.session_id)
            summary_version = operator_guide_memory.summary_version(context.session_id)

            memory_metadata = {
                "used": bool(recent_turns) or bool(confirmed_facts),
                "turn_count": len(recent_turns),
                "max_turns": operator_guide_memory.max_turns,
                "confirmed_facts": confirmed_facts,
                "confirmedFacts": confirmed_facts,
                "summary_version": summary_version,
                "summaryVersion": summary_version,
            }

            return (
                AgentContext(
                    request_id=context.request_id,
                    session_id=context.session_id,
                    client_id=context.client_id,
                    metadata={
                        **context.metadata,
                        OPERATOR_GUIDE_RECENT_CONVERSATION_KEY: [
                            turn.to_prompt_dict() for turn in recent_turns
                        ],
                        "confirmed_facts": confirmed_facts,
                    },
                    on_progress=context.on_progress,
                ),
                memory_metadata,
            )

        def build_prompt(state: AgentGraphState) -> AgentGraphState:
            try:
                agent = agent_router.get(state["selectedLeafAgent"])
            except UnknownAgentError:
                return {
                    "error": build_error_payload(
                        "UNKNOWN_AGENT",
                        f"Unknown leaf agent: {state['selectedLeafAgent']}",
                        details={"agent": state["selectedLeafAgent"]},
                    )
                }
            runtime_context, memory_metadata = operator_guide_memory_context(state)
            prompt = agent.build_prompt(state["typedPayload"], runtime_context)
            output: AgentGraphState = {"prompt": prompt}
            if memory_metadata:
                output["operatorGuideMemory"] = memory_metadata
            build_prompt_messages = getattr(agent, "build_prompt_messages", None)
            if callable(build_prompt_messages):
                # build_prompt에서 이미 progress를 보냈으므로 messages 생성 시에는 중복 전송을 막는다.
                prompt_messages_context = replace(runtime_context, on_progress=None)
                output["promptMessages"] = build_prompt_messages(
                    state["typedPayload"],
                    prompt_messages_context,
                )
            return output

        def call_llm_default(state: AgentGraphState) -> AgentGraphState:
            if state.get("error"):
                return {}
            return invoke_llm_call_slot(
                llm_slots[0],
                state["prompt"],
                state.get("promptMessages"),
            )

        def call_llm_fallback1(state: AgentGraphState) -> AgentGraphState:
            if state.get("error"):
                return {}
            return invoke_llm_call_slot(
                llm_slots[1],
                state["prompt"],
                state.get("promptMessages"),
            )

        def call_llm_fallback2(state: AgentGraphState) -> AgentGraphState:
            if state.get("error"):
                return {}
            return invoke_llm_call_slot(
                llm_slots[2],
                state["prompt"],
                state.get("promptMessages"),
            )

        def call_llm_tool_followup(state: AgentGraphState) -> AgentGraphState:
            if state.get("error"):
                return {}
            slot = llm_slots_by_name.get(state.get("llmSlot", ""))
            if slot is None:
                return {}
            return invoke_llm_call_slot(slot, state["toolFollowupPrompt"])

        def parse_llm_response(state: AgentGraphState) -> AgentGraphState:
            raw = state.get("llmRaw")
            if not raw:
                return {}

            cleaned = raw.strip()
            if cleaned.startswith("```"):
                lines = cleaned.splitlines()
                if (
                    len(lines) >= 2
                    and lines[0].startswith("```")
                    and lines[-1].startswith("```")
                ):
                    cleaned = "\n".join(lines[1:-1]).strip()

            is_operator_guide = state.get("selectedAgent") == "operator_guide"
            try:
                payload = json.loads(cleaned)
                if not isinstance(payload, dict):
                    raise ValueError("LLM response must be a JSON object.")
            except Exception as exc:
                if is_operator_guide:
                    fallback_context = replace(state["context"], on_progress=None)
                    result = run_fallback(
                        agent_router,
                        {
                            **state,
                            "context": fallback_context,
                        },
                    )
                    metadata = dict(result.metadata)
                    metadata["fallbackReason"] = "json_decode_failed"
                    metadata["fallbackDetails"] = str(exc)
                    current_model = build_current_model_metadata(state)
                    if current_model is not None:
                        metadata["currentModel"] = current_model
                    payload = result.payload
                    if is_operator_guide:
                        payload = sanitize_operator_guide_response_payload(payload)
                    return {
                        "responsePayload": payload,
                        "responseMetadata": metadata,
                        "fallbackReason": "json_decode_failed",
                    }
                return {
                    "error": build_error_payload(
                        "INVALID_LLM_RESPONSE",
                        "LLM response must be a JSON object.",
                    )
                }

            if state.get("selectedLeafAgent") == "quest_generator.production_quest":
                if not _has_successful_tool_call(
                    state,
                    PRODUCTION_QUEST_SELECTION_TOOL_NAME,
                ):
                    return {
                        "error": build_error_payload(
                            "INVALID_LLM_RESPONSE",
                            "퀘스트 LLM 응답은 production quest 선택 tool을 먼저 호출해야 합니다.",
                        )
                    }

            metadata = {"llm": "used"}
            if state.get("llmSlot"):
                metadata["llmSlot"] = state["llmSlot"]
            if state.get("llmProvider"):
                metadata["llmProvider"] = state["llmProvider"]
            if state.get("llmModel"):
                metadata["llmModel"] = state["llmModel"]
            current_model = build_current_model_metadata(state)
            if current_model is not None:
                metadata["currentModel"] = current_model
            if state.get("operatorGuideMemory"):
                metadata["memory"] = state["operatorGuideMemory"]
            metadata = {
                **metadata,
                **append_tool_metadata(state),
            }
            if state.get("selectedAgent") == "operator_guide":
                payload = sanitize_operator_guide_response_payload(payload)
            return {"responsePayload": payload, "responseMetadata": metadata}

        def build_fallback(state: AgentGraphState) -> AgentGraphState:
            fallback_context = replace(state["context"], on_progress=None)
            result = run_fallback(
                agent_router,
                {
                    **state,
                    "context": fallback_context,
                },
            )
            metadata = dict(result.metadata)
            current_model = build_current_model_metadata(state)
            if current_model is not None:
                metadata["currentModel"] = current_model
            if state.get("operatorGuideMemory"):
                metadata["memory"] = state["operatorGuideMemory"]
            metadata = {
                **metadata,
                **append_tool_metadata(state),
            }
            output: AgentGraphState = {
                "fallbackReason": "llm_unavailable",
                "responsePayload": result.payload,
                "responseMetadata": metadata,
            }
            return {
                **output,
                **append_middleware_log(
                    state,
                    "agent.middleware.fallback",
                    "deterministic_fallback",
                    {
                        "reason": "llm_unavailable",
                        "selectedAgent": state["selectedAgent"],
                        "selectedLeafAgent": state["selectedLeafAgent"],
                    },
                ),
            }

        def validate_response_schema(state: AgentGraphState) -> AgentGraphState:
            payload = state.get("responsePayload")
            if not isinstance(payload, dict):
                return {
                    "error": build_error_payload(
                        "INVALID_AGENT_RESPONSE",
                        "Agent response payload must be an object.",
                    )
                }
            return {}

        def cache_write(state: AgentGraphState) -> AgentGraphState:
            if not state.get("cacheKey"):
                return {}

            response_cache.set(
                state["cacheKey"],
                state["responsePayload"],
                state.get("responseMetadata", {}),
            )
            if state.get("selectedAgent") == "operator_guide":
                payload = state.get("responsePayload", {})
                operator_guide_memory.remember(
                    state["context"].session_id,
                    question=str(payload.get("question") or ""),
                    answer=str(payload.get("final_answer") or ""),
                )
            return {}

        def log_agent_finished(state: AgentGraphState) -> AgentGraphState:
            return append_middleware_log(
                state,
                "agent.middleware.after",
                "agent_finished",
                {
                    "selectedAgent": state["selectedAgent"],
                    "selectedLeafAgent": state["selectedLeafAgent"],
                },
            )

        def build_agent_response(state: AgentGraphState) -> AgentGraphState:
            envelope = state["envelope"]
            metadata = {
                **state.get("responseMetadata", {}),
                "selectedAgent": state["selectedAgent"],
                "selectedLeafAgent": state["selectedLeafAgent"],
            }
            if state.get("middlewareLogs"):
                metadata["middlewareLogs"] = state["middlewareLogs"]

            raw_payload = state.get("responsePayload") or {}
            
            # 4차 재리뷰 보강: 만약 요청 payload에 tts 입력 힌트(tts.text)가 있다면 응답 payload에 전파
            req_payload = getattr(envelope, "payload", {}) if hasattr(envelope, "payload") else {}
            if isinstance(req_payload, dict) and "tts" in req_payload:
                req_tts = req_payload["tts"]
                if isinstance(req_tts, dict) and "text" in req_tts:
                    # 응답 payload에 이미 완성된 tts가 없는 경우에만 입력 힌트 복사
                    if "tts" not in raw_payload:
                        raw_payload = {**raw_payload, "tts": req_tts}

            if state.get("selectedAgent") == "process_optimizer":
                raw_payload = self._prepare_process_optimizer_tts_payload(raw_payload)
            payload_with_tts = self._attach_tts(state["selectedAgent"], raw_payload)

            response = AgentResponseEnvelope(
                request_id=envelope.request_id,
                session_id=envelope.session_id,
                client_id=envelope.client_id,
                agent=state["selectedAgent"],
                payload={
                    **payload_with_tts,
                    "metadata": metadata,
                },
                streams=state.get("streams", []),
            )
            return {"responseEnvelope": response.model_dump(mode="json")}

        def build_agent_error(state: AgentGraphState) -> AgentGraphState:
            envelope = state["envelope"]
            selected_agent = state.get("selectedAgent")
            response_agent = (
                selected_agent
                if selected_agent in TOP_LEVEL_AGENT_IDS
                else envelope.agent
            )
            default_error = build_error_payload(
                "ROUTING_UNAVAILABLE",
                "Agent routing requires a valid orchestrator model decision.",
                details=_build_routing_error_details(state, envelope),
            )
            response = AgentErrorEnvelope(
                request_id=envelope.request_id,
                session_id=envelope.session_id,
                client_id=envelope.client_id,
                agent=response_agent,
                error=state.get("error") or default_error,
            )
            return {"responseEnvelope": response.model_dump(mode="json")}

        graph = StateGraph(AgentGraphState)
        graph.add_node("build_context", build_context)
        graph.add_node("agent.middleware.before", log_agent_started)
        graph.add_node("validate_envelope", validate_envelope)
        graph.add_node("route_top_agent", route_top_agent)
        graph.add_node("validate_process_payload", validate_process_payload)
        graph.add_node("process_optimizer_state_update", process_optimizer_state_update)
        graph.add_node(
            "process_optimizer_subquest_check",
            process_optimizer_subquest_check,
        )
        graph.add_node("process_optimizer_v2_graph", process_optimizer_v2_graph)
        graph.add_node("operator_guide.route_sub_agent", route_operator_guide_sub_agent)
        graph.add_node("quest_generator.route_sub_agent", route_quest_sub_agent)
        graph.add_node(
            "new_material_generator.route_sub_agent", route_new_material_sub_agent
        )
        graph.add_node("synthesize_material", synthesize_material)
        graph.add_node("cache_lookup", cache_lookup)
        graph.add_node("build_cached_response", build_cached_response)
        graph.add_node("build_prompt", build_prompt)
        graph.add_node("call_llm.default", call_llm_default)
        graph.add_node("call_llm.fallback1", call_llm_fallback1)
        graph.add_node("call_llm.fallback2", call_llm_fallback2)
        graph.add_node("prepare_tool_call", build_tool_node_input(agent_router))
        graph.add_node("agent.tool_node", build_agent_tool_node(agent_router))
        graph.add_node("build_tool_followup_prompt", build_tool_followup_prompt)
        graph.add_node("call_llm.tool_followup", call_llm_tool_followup)
        graph.add_node("parse_llm_response", parse_llm_response)
        graph.add_node("agent.middleware.fallback", build_fallback)
        graph.add_node("validate_response_schema", validate_response_schema)
        graph.add_node("cache_write", cache_write)
        graph.add_node("agent.middleware.after", log_agent_finished)
        graph.add_node("build_agent_response", build_agent_response)
        graph.add_node("build_agent_error", build_agent_error)

        wire_agent_graph(graph)
        return graph.compile()


def _has_successful_tool_call(state: AgentGraphState, tool_name: str) -> bool:
    return any(
        tool_call.get("name") == tool_name and tool_call.get("ok") is True
        for tool_call in state.get("toolCalls", [])
    )


def run_agent_pipeline(
    message: AgentRequestEnvelope | dict[str, Any],
) -> dict[str, Any]:
    """Run one message through a default agent pipeline."""

    try:
        return AgentPipeline().run(message)
    except ValidationError as exc:
        return build_validation_error(exc, message)
