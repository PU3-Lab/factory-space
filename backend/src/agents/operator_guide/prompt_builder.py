"""Prompt builder for operator guide CSV-grounded LLM answers."""

from __future__ import annotations

import json

from agents.operator_guide.manual_context_builder import ManualQAPromptContext
from agents.operator_guide.system_prompt import OPERATOR_GUIDE_SYSTEM_PROMPT


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
        return self.build_user_prompt(
            question=question,
            topic=topic,
            sub_agent=sub_agent,
            context=context,
        )

    def build_messages(
        self,
        *,
        question: str,
        topic: str,
        sub_agent: str,
        context: ManualQAPromptContext,
    ) -> list[dict[str, str]]:
        """Return chat messages with a real system prompt."""

        return [
            {"role": "system", "content": OPERATOR_GUIDE_SYSTEM_PROMPT},
            {
                "role": "user",
                "content": self.build_user_prompt(
                    question=question,
                    topic=topic,
                    sub_agent=sub_agent,
                    context=context,
                ),
            },
        ]

    def build_user_prompt(
        self,
        *,
        question: str,
        topic: str,
        sub_agent: str,
        context: ManualQAPromptContext,
    ) -> str:
        """Render request-specific user content for the operator guide."""

        evidence_json = json.dumps(context.evidence, ensure_ascii=False, indent=2)
        actions_json = json.dumps(
            [action.model_dump() for action in context.result.recommended_actions],
            ensure_ascii=False,
            indent=2,
        )
        return f"""Answer this {self._topic_label(topic)}.
[PLAYER_QUESTION]
{question}

[LEAF_AGENT]
{sub_agent}

[QUESTION_TYPE]
{context.result.question_type}

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
            return "machine help question"
        if topic == "recipe":
            return "recipe question"
        if topic == "troubleshooting":
            return "troubleshooting question"
        return "operator guide question"
