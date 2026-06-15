"""CSV 근거를 LLM 프롬프트로 조립하는 operator_guide prompt builder.

초보자용 설명:
    LLM은 아무 근거 없이 답하면 지어낼 수 있다.
    이 파일은 플레이어 질문, leaf agent 정보, CSV 근거를 하나의 prompt로 묶어
    LLM이 정해진 JSON 형식으로 답하도록 안내한다.
"""

from __future__ import annotations

import json

from agents.operator_guide.manual_context_builder import ManualQAPromptContext
from agents.operator_guide.retrieved_context_guard import wrap_retrieved_context
from agents.operator_guide.system_prompt import OPERATOR_GUIDE_SYSTEM_PROMPT


class ManualQAPromptBuilder:
    """선택된 leaf agent와 CSV evidence를 LLM이 읽을 수 있는 prompt로 만든다."""

    def build(
        self,
        *,
        question: str,
        topic: str,
        sub_agent: str,
        context: ManualQAPromptContext,
    ) -> str:
        """단일 문자열 prompt를 만든다."""

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
        """system/user 역할이 분리된 chat messages를 만든다."""

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
        """이번 질문에 필요한 CSV 근거와 출력 계약을 user prompt로 만든다."""

        evidence_json = json.dumps(context.evidence, ensure_ascii=False, indent=2)
        actions_json = json.dumps(
            [action.model_dump() for action in context.result.recommended_actions],
            ensure_ascii=False,
            indent=2,
        )
        rag_context_section = self._rag_context_section(context)
        return f"""Answer this {self._topic_label(topic)}.
[PLAYER_QUESTION]
{question}

[LEAF_AGENT]
{sub_agent}

[QUESTION_TYPE]
{context.result.question_type}

[CSV_EVIDENCE]
{evidence_json}

{rag_context_section}

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

    def _rag_context_section(self, context: ManualQAPromptContext) -> str:
        """RAG 검색 결과가 있을 때만 prompt에 검색 근거 섹션을 추가한다."""

        if not context.rag_context_text:
            return ""

        metadata_json = json.dumps(
            context.rag_metadata or {},
            ensure_ascii=False,
            indent=2,
        )
        guarded_context = wrap_retrieved_context(context.rag_context_text)
        return f"""[RAG_RETRIEVAL_CONTEXT]
{guarded_context}

[RAG_RETRIEVAL_METADATA]
{metadata_json}
"""

    def _topic_label(self, topic: str) -> str:
        """내부 topic 값을 LLM이 이해하기 쉬운 영어 설명으로 바꾼다."""

        if topic == "machine":
            return "machine help question"
        if topic == "recipe":
            return "recipe question"
        if topic == "troubleshooting":
            return "troubleshooting question"
        return "operator guide question"
