"""퀘스트 요청을 어떤 하위 퀘스트 에이전트가 처리할지 고르는 파일입니다."""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext

QUEST_SUB_AGENT_IDS = (
    "quest_generator.production_quest",
    "quest_generator.economy_quest",
)


class QuestGeneratorAgent:
    """퀘스트 생성 요청을 보고 사용할 하위 에이전트를 선택하는 관리자입니다."""

    agent_id = "quest_generator"
    tools = ()

    def build_routing_prompt(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> str:
        """LLM에게 선택 가능한 하위 퀘스트 에이전트 목록을 알려주는 프롬프트를 만듭니다."""

        allowed_leaf_agent_ids = "\n".join(
            f"- {sub_agent_id}" for sub_agent_id in QUEST_SUB_AGENT_IDS
        )
        return (
            "[ROLE]\n"
            "퀘스트 생성 도메인 오케스트레이터\n\n"
            "[TASK]\n"
            "퀘스트 요청을 처리할 leaf Agent id를 하나만 결정한다.\n\n"
            "[ALLOWED_LEAF_AGENT_IDS]\n"
            f"{allowed_leaf_agent_ids}\n\n"
            "[REQUEST_CONTEXT]\n"
            f"{context.metadata}\n\n"
            "[REQUEST_PAYLOAD]\n"
            f"{payload}\n\n"
            "[OUTPUT_CONTRACT]\n"
            "ALLOWED_LEAF_AGENT_IDS 중 하나의 id만 그대로 출력한다.\n"
            "JSON, markdown, 설명, reason, 따옴표, 코드블록은 출력하지 않는다."
        )
