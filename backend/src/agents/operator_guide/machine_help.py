"""장비 정보 제공을 전담하는 하위 에이전트 모듈입니다.

초보자용 설명:
    플레이어가 특정 기계의 기능, 작동 방식, UI 구성에 대해 질문했을 때 그에 맞는 시스템 설명 프롬프트를 만듭니다.
"""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult
from agents.operator_guide.service import (
    build_manual_qa_agent_result,
    build_manual_qa_prompt,
    build_manual_qa_prompt_messages,
)


class MachineHelpAgent:
    """장비의 상태, 사용법 및 UI 맥락 정보를 사용자 친화적으로 설명하기 위한 에이전트 클래스입니다.

    초보자용 설명:
        플레이어가 "분쇄기가 뭐야?"와 같이 장비 자체에 대해 질문할 때 이 클래스가 작동합니다.
    """

    agent_id = "operator_guide.machine_help"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        """장비 설명과 관련하여 LLM에 보낼 단일 텍스트 프롬프트를 구성합니다.

        입력값:
            - payload (dict): 질문 내용이 포함된 사전 데이터
            - context (AgentContext): 에이전트 컨텍스트 객체

        반환값:
            - str: 관련 장비 매뉴얼 지식을 병합한 최종 프롬프트 문자열
        """
        question = str(payload.get("question") or payload.get("message") or "")
        return build_manual_qa_prompt(
            question,
            topic="machine",
            sub_agent=self.agent_id,
            context=context.metadata,
            on_progress=getattr(context, "on_progress", None),
        )

    def build_prompt_messages(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> list[dict[str, str]]:
        """장비 설명과 관련하여 LLM에 보낼 시스템 및 사용자 역할 분리 메시지 목록을 구성합니다.

        입력값:
            - payload (dict): 질문 내용이 포함된 사전 데이터
            - context (AgentContext): 에이전트 컨텍스트 객체

        반환값:
            - list[dict[str, str]]: 대화형 구조로 조립된 메시지 목록
        """
        question = str(payload.get("question") or payload.get("message") or "")
        return build_manual_qa_prompt_messages(
            question,
            topic="machine",
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
            AI API 호출 장애 시, 매뉴얼 DB에 있는 텍스트만으로 정형화된 답변을 빠르게 생성합니다.

        입력값:
            - payload (dict): 질문 내용이 포함된 사전 데이터
            - context (AgentContext): 에이전트 컨텍스트 객체

        반환값:
            - AgentRunResult: Fallback 실행 결과 객체
        """
        return build_manual_qa_agent_result(
            payload,
            context,
            topic="machine",
            sub_agent=self.agent_id,
        )
