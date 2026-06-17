"""오퍼레이터 가이드(Operator Guide) 도메인의 상위 오케스트레이터 에이전트 모듈입니다.

초보자용 설명:
    사용자의 질문을 분석하여 하위 에이전트(기계 설명, 레시피 설명, 문제 해결) 중 하나로 연결(라우팅)하는 역할을 합니다.
"""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext

OPERATOR_GUIDE_LEAF_AGENT_IDS = (
    "operator_guide.recipe_explainer",
    "operator_guide.machine_help",
    "operator_guide.troubleshooter",
)


class OperatorGuideAgent:
    """플레이어의 질문을 담당할 적절한 하위 에이전트를 결정하는 오케스트레이터 클래스입니다.

    초보자용 설명:
        플레이어가 던진 질문에 알맞은 에이전트(예: machine_help 등)를 결정합니다.
    """

    agent_id = "operator_guide"
    tools = ()

    def build_routing_prompt(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> str:
        """하위 에이전트를 선택하기 위해 LLM에 보낼 프롬프트(Prompt) 지시문을 생성합니다.

        초보자용 설명:
            전달받은 질문 페이로드와 에이전트 목록을 종합하여 라우팅 지시용 텍스트를 만듭니다.

        입력값:
            - payload (dict): 질문 데이터가 담긴 딕셔너리
            - context (AgentContext): 에이전트 컨텍스트 객체

        반환값:
            - str: LLM에 전달할 프롬프트 문자열
        """

        allowed_leaf_agent_ids = "\n".join(
            f"- {sub_agent_id}" for sub_agent_id in OPERATOR_GUIDE_LEAF_AGENT_IDS
        )
        return (
            "[ROLE]\n"
            "운영자 가이드 도메인 오케스트레이터\n\n"
            "[TASK]\n"
            "사용자 요청을 처리할 leaf Agent id를 하나만 결정한다.\n\n"
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
