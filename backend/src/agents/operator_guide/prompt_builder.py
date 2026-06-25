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
    """선택된 leaf agent와 CSV evidence를 LLM이 읽을 수 있는 prompt로 만드는 클래스입니다."""

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
        recent_conversation_section = self._recent_conversation_section(context)
        confirmed_facts_section = self._confirmed_facts_section(context)
        current_game_state_section = self._current_game_state_section(context)
        rag_context_section = self._rag_context_section(context)
        response_style_section = self._response_style_section(context)
        return f"""Answer this {self._topic_label(topic)}.
[PLAYER_QUESTION]
{question}

{recent_conversation_section}

{confirmed_facts_section}

[LEAF_AGENT]
{sub_agent}

[QUESTION_TYPE]
{context.result.question_type}

[CSV_EVIDENCE]
{evidence_json}

{current_game_state_section}

{rag_context_section}

{response_style_section}

[RECOMMENDED_ACTIONS]
{actions_json}

[OUTPUT_CONTRACT]
Return only one JSON object.
Do not wrap it in markdown.
The final_answer field is shown directly to the player.
Write final_answer in Korean as a friendly player-facing response.
For two short related questions, answer as one natural paragraph instead of numbered labels.
Use numbered sections only for three or more questions or complex checklists.
Do not use markdown emphasis such as **bold**, bullet-heavy formatting, or English labels in parentheses.
Avoid slash-separated lists and repeated examples. Prefer one natural example only.
Use at most one concrete example unless the player asks for examples.
For simple equipment/resource/recipe questions, keep final_answer to 2~3 short Korean sentences.
For equipment "what/where used" questions, do not enumerate input materials or recipe names. Explain only the role and how the output is used next.
Do not expose raw IDs such as equipment_*, resource_*, recipe_*, issue_*, or action_* in final_answer unless the player explicitly asks for IDs.
Do not add troubleshooting checks unless the player asked about a failure or stoppage.
Use exactly these keys:
{{
  "final_answer": "Korean player-facing answer with concise line breaks",
  "actions": [],
  "question": "{question}",
  "topic": "{topic}"
}}
"""

    def _response_style_section(self, context: ManualQAPromptContext) -> str:
        """답변 길이 스타일을 LLM에게 알려주는 prompt 섹션을 만든다."""

        style = context.response_style
        if style == "short":
            instruction = (
                "Answer in 1~2 short Korean sentences. "
                "Prioritize the direct answer over extra background."
            )
        elif style == "detailed":
            instruction = (
                "Answer in 4~6 Korean sentences. "
                "Include the role, relevant flow, and one practical check when evidence supports it."
            )
        else:
            instruction = (
                "Answer in 2~3 short Korean sentences for simple questions, "
                "and stay concise for NPC dialogue."
            )
        return f"""[RESPONSE_STYLE]
{style}
{instruction}"""

    def _recent_conversation_section(self, context: ManualQAPromptContext) -> str:
        """같은 세션의 최근 대화를 LLM이 참고할 수 있는 prompt 섹션으로 만든다.

        초보자용 설명:
            후속 질문은 "그럼?", "그 장비는?"처럼 앞 대화를 알아야 이해됩니다.
            그래서 최근 질문/답변을 짧은 목록으로 넣어주되, 사용자가 처음 질문한
            경우에는 이 섹션을 아예 만들지 않습니다.
        """

        if not context.recent_conversation:
            return ""

        lines = ["[RECENT_CONVERSATION_CONTEXT]"]
        for index, turn in enumerate(context.recent_conversation, start=1):
            lines.append(f"Turn {index}")
            lines.append(f"Player: {turn.get('question', '')}")
            lines.append(f"Operator: {turn.get('answer', '')}")
        return "\n".join(lines)

    def _confirmed_facts_section(self, context: ManualQAPromptContext) -> str:
        """대화 중 플레이어가 직접 확인해 준 사실 목록을 LLM이 참고할 수 있는 prompt 섹션으로 만듭니다.

        초보자용 설명:
            "전력은 정상인데?" 같은 사실들이 세션 메모리에 누적되어 넘어옵니다.
            LLM이 답변할 때 이 상태 사실들을 확인하여 추론할 수 있도록 프롬프트에 기재해 줍니다.
        """

        confirmed_facts = getattr(context, "confirmed_facts", [])
        if not confirmed_facts:
            return ""

        lines = ["[CONFIRMED_FACTS]"]
        for fact in confirmed_facts:
            lines.append(f"- {fact}")
        return "\n".join(lines)

    def _current_game_state_section(self, context: ManualQAPromptContext) -> str:
        """실시간 게임 상태 컨텍스트가 존재하면 [CURRENT_GAME_STATE] 섹션을 추가합니다.

        초보자용 설명:
            RAG 검색 결과처럼 게임 상태 정보가 유효하게 수집된 경우에만 이 섹션을 프롬프트에 포함합니다.
        """

        state_text = getattr(context, "current_game_state_text", "")
        if not state_text:
            return ""

        return f"[CURRENT_GAME_STATE]\n{state_text}"

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
