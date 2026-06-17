"""생산 정체 및 공장 에러 상태 진단을 전담하는 하위 에이전트 모듈입니다.

초보자용 설명:
    플레이어가 "기계가 왜 안 돌지?" 혹은 "벨트가 막혔어"와 같이 공장 트러블 상황에 대해 질문했을 때 작동합니다.
"""

from __future__ import annotations

from typing import Any

from agents.base import AgentContext, AgentRunResult
from agents.operator_guide.service import (
    build_manual_qa_agent_result,
    build_manual_qa_prompt,
    build_manual_qa_prompt_messages,
)


class TroubleshooterAgent:
    """기계 고장, 벨트 정체 등 공장의 에러 상황을 진단하고 해결책을 제시하는 문제 해결 에이전트 클래스입니다.

    초보자용 설명:
        플레이어가 공장 생산 라인의 정체나 기계 오작동 원인을 물어볼 때 알맞은 해결 지침 프롬프트를 구성합니다.
    """

    agent_id = "operator_guide.troubleshooter"
    tools = ()

    def build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str:
        """트러블슈팅에 특화된 단일 텍스트 프롬프트를 구성합니다.

        입력값:
            - payload (dict): 질문 내용이 포함된 사전 데이터
            - context (AgentContext): 에이전트 컨텍스트 객체

        반환값:
            - str: 문제 해결 매뉴얼 지식을 결합한 최종 프롬프트 문자열
        """
        question = str(payload.get("question") or payload.get("message") or "")
        return build_manual_qa_prompt(
            question,
            topic="troubleshooting",
            sub_agent=self.agent_id,
            context=context.metadata,
            on_progress=getattr(context, "on_progress", None),
        )

    def build_prompt_messages(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> list[dict[str, str]]:
        """트러블슈팅에 특화된 시스템 및 사용자 역할 분리 메시지 목록을 구성합니다.

        입력값:
            - payload (dict): 질문 내용이 포함된 사전 데이터
            - context (AgentContext): 에이전트 컨텍스트 객체

        반환값:
            - list[dict[str, str]]: 대화형 구조로 조립된 메시지 목록
        """
        question = str(payload.get("question") or payload.get("message") or "")
        return build_manual_qa_prompt_messages(
            question,
            topic="troubleshooting",
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
            AI API 호출 장애 시, 문제 해결 매뉴얼 DB의 에러 원인 및 점검 순서 데이터를 사용하여 고정 답변을 생성합니다.

        입력값:
            - payload (dict): 질문 내용이 포함된 사전 데이터
            - context (AgentContext): 에이전트 컨텍스트 객체

        반환값:
            - AgentRunResult: Fallback 실행 결과 객체
        """
        return build_manual_qa_agent_result(
            payload,
            context,
            topic="troubleshooting",
            sub_agent=self.agent_id,
        )
