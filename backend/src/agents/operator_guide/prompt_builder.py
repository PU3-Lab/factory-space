"""Prompt builder for operator guide CSV-grounded LLM answers."""

from __future__ import annotations

import json

from agents.operator_guide.manual_context_builder import ManualQAPromptContext


class ManualQAPromptBuilder:
    """Render the selected leaf-agent prompt with CSV evidence."""

    def build(
        self,
        *,
        question: str,
        topic: str,
        sub_agent: str,
        context: ManualQAPromptContext,
    ) -> str:
        evidence_json = json.dumps(context.evidence, ensure_ascii=False, indent=2)
        actions_json = json.dumps(
            [action.model_dump() for action in context.result.recommended_actions],
            ensure_ascii=False,
            indent=2,
        )
        return f"""다음 {self._topic_label(topic)} 질문에 답변하세요.

[ROLE]
You are Factory Space's friendly tutorial NPC.
플레이어가 막히지 않도록 부드러운 안내형 한국어로 답변하세요.

[PLAYER_QUESTION]
{question}

[LEAF_AGENT]
{sub_agent}

[STYLE_RULES]
- 명령조보다 제안형으로 말하세요.
- 플레이어를 탓하지 마세요.
- 답변은 4~6문장으로 작성하세요.
- CSV 근거를 중심으로 답하세요.
- CSV 근거에 없는 구체적인 수치, 장비, 레시피, 효과는 만들지 마세요.
- 전력, 입력, 출력, 저장 공간 확인 같은 일반적인 플레이 안내는 짧게 보충할 수 있습니다.
- 추천 행동이 있으면 본문에 자연스럽게 녹이되, 별도 불릿 목록으로 반복하지 마세요.

[CSV_EVIDENCE]
{evidence_json}

[RECOMMENDED_ACTIONS]
{actions_json}

[OUTPUT_CONTRACT]
Return only one JSON object.
Do not wrap it in markdown.
Use exactly these keys:
{{
  "final_answer": "4~6 Korean sentences in friendly tutorial NPC tone",
  "actions": [],
  "question": "{question}",
  "topic": "{topic}"
}}
"""

    def _topic_label(self, topic: str) -> str:
        if topic == "machine":
            return "설비 도움말"
        if topic == "recipe":
            return "레시피"
        if topic == "troubleshooting":
            return "공장 문제를 진단"
        return "운영자 가이드"
