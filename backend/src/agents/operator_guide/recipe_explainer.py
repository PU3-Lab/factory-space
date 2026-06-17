"""아이템 제작법(Recipe) 및 생산 체인 설명을 전담하는 하위 에이전트 모듈입니다.

초보자용 설명:
    플레이어가 "철괴는 어떻게 만들어?" 혹은 "기어 제작법이 뭐야?"와 같이 아이템 생산 공식에 대해 질문했을 때 작동합니다.
"""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult
from agents.operator_guide.service import (
    build_manual_qa_agent_result,
    build_manual_qa_prompt,
    build_manual_qa_prompt_messages,
)


class RecipeExplainerAgent:
    """게임 내 아이템 제작 공식 및 필요한 설비/자원에 대한 질문을 처리하는 에이전트 클래스입니다.

    초보자용 설명:
        플레이어가 어떤 아이템의 제작 방법이나 제작 경로에 대해 물어볼 때 알맞은 프롬프트를 구성합니다.
    """

    agent_id = "operator_guide.recipe_explainer"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        """레시피 설명에 특화된 단일 텍스트 프롬프트를 구성합니다.

        입력값:
            - payload (dict): 질문 내용이 포함된 사전 데이터
            - context (AgentContext): 에이전트 컨텍스트 객체

        반환값:
            - str: 레시피 매뉴얼 지식을 결합한 최종 프롬프트 문자열
        """
        question = str(payload.get("question") or payload.get("message") or "")
        return build_manual_qa_prompt(
            question,
            topic="recipe",
            sub_agent=self.agent_id,
            context=context.metadata,
            on_progress=getattr(context, "on_progress", None),
        )

    def build_prompt_messages(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> list[dict[str, str]]:
        """레시피 설명에 특화된 시스템 및 사용자 역할 분리 메시지 목록을 구성합니다.

        입력값:
            - payload (dict): 질문 내용이 포함된 사전 데이터
            - context (AgentContext): 에이전트 컨텍스트 객체

        반환값:
            - list[dict[str, str]]: 대화형 구조로 조립된 메시지 목록
        """
        question = str(payload.get("question") or payload.get("message") or "")
        return build_manual_qa_prompt_messages(
            question,
            topic="recipe",
            sub_agent=self.agent_id,
            context=context.metadata,
            on_progress=getattr(context, "on_progress", None),
        )

    def fallback(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> AgentRunResult:
        """LLM 호출이 불가능하거나 실패했을 때 호출되는 대체(Fallback) 처리 함수입니다.

        초보자용 설명:
            AI API 호출 장애 시, 매뉴얼 DB의 레시피 텍스트 데이터를 구조화하여 빠른 고정 답변을 생성합니다.

        입력값:
            - payload (dict): 질문 내용이 포함된 사전 데이터
            - context (AgentContext): 에이전트 컨텍스트 객체

        반환값:
            - AgentRunResult: Fallback 실행 결과 객체
        """
        return build_manual_qa_agent_result(
            payload,
            context,
            topic="recipe",
            sub_agent=self.agent_id,
        )
