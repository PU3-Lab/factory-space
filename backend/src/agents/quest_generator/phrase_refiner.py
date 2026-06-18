"""QuestPhraseRefiner: 퀘스트 문구 정제 컴포넌트."""

from __future__ import annotations

import json
import logging
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from agents.quest_generator.models import QuestContext, SupportQuestDraft
    from llm.adapter import LLMAdapter

logger = logging.getLogger(__name__)


class QuestPhraseRefiner:
    """지원 퀘스트의 초안 문구(title, description)를 LLM을 사용하여 자연스럽게 다듬는 정제기입니다.

    초보자용 한글 docstring:
    이 클래스는 생성된 퀘스트의 템플릿 문구를 LLM(대형 언어 모델)을 통해 한국어 톤앤매너에 맞게 다듬는 역할을 합니다.
    기본적으로 RAG나 규칙 기반으로 생성된 퀘스트 초안(draft)이 다소 딱딱할 수 있으므로, 메인 퀘스트의 제목 맥락을 참고하여
    자연스러운 문장으로 윤색합니다. 이때 퀘스트의 중요한 핵심 정보(수량, 아이템 종류, 보상 등)가 변조되지 않도록
    안전 장치를 갖추고 있습니다.
    """

    def __init__(self, llm_adapter: LLMAdapter) -> None:
        """QuestPhraseRefiner를 초기화합니다.

        [매개변수]
        - llm_adapter: 실제 LLM API를 호출하기 위한 어댑터 객체입니다. (예: OpenAI, Google Gemini 등)
        """
        self.llm_adapter = llm_adapter

    def refine(
        self, draft: SupportQuestDraft, context: QuestContext
    ) -> SupportQuestDraft:
        """주어진 지원 퀘스트 초안의 title과 description을 다듬어 새로운 SupportQuestDraft 객체를 반환합니다.

        [동작 원리 및 데이터 흐름]
        1. 퀘스트 컨텍스트에서 메인 퀘스트의 제목을 추출해 프롬프트에 맥락 정보로 포함시킵니다.
        2. LLM이 수치나 아이템 ID 등 규칙 결정값을 변조하지 않고 제목/설명 문구만 다듬도록 강력한 프롬프트를 구성합니다.
        3. LLM의 출력이 실패하거나 JSON 파싱에 실패할 경우, 안전하게 원본 draft를 그대로 반환합니다(폴백).
        4. 성공적으로 파싱된 경우, Pydantic의 model_copy를 사용해 title과 description만 수정한 복사본을 만들어 반환함으로써,
           나머지 수치 데이터의 무결성과 불변성을 구조적으로 보장합니다.
        """
        main_quest_title = (
            context.current_main_quest.title
            if context.current_main_quest
            else "메인 퀘스트 없음"
        )

        prompt = (
            "팩토리 스페이스 지원 퀘스트 문구 윤색 에이전트입니다.\n"
            "주어진 지원 퀘스트의 제목(title)과 설명(description)을 메인 퀘스트 맥락에 맞게 한글로 자연스럽게 다듬어 주세요.\n"
            "반드시 수량(amount), 아이템 종류, 대상, 보상 등 게임플레이 규칙과 직결되는 핵심 목표 수치 정보는 절대 수정해서는 안 됩니다. "
            "오직 제목과 설명의 문구만 다듬어야 합니다.\n\n"
            f"[메인 퀘스트 제목]\n{main_quest_title}\n\n"
            f"[원본 지원 퀘스트 제목]\n{draft.title}\n"
            f"[원본 지원 퀘스트 설명]\n{draft.description}\n\n"
            "반드시 다음 JSON 형식의 JSON 객체 하나만 출력하세요. "
            "마크다운 코드 펜스(```json)나 다른 설명글은 절대로 포함하지 마세요.\n"
            "{\n"
            '  "title": "다듬어진 퀘스트 제목",\n'
            '  "description": "다듬어진 퀘스트 설명"\n'
            "}"
        )

        try:
            logger.info("Invoking LLM to refine quest phrases...")
            response = self.llm_adapter.invoke(prompt)
            if not response:
                logger.warning(
                    "LLM adapter returned empty response. Falling back to original draft."
                )
                return draft

            cleaned_response = response.strip()
            # 마크다운 코드 펜스가 있는 경우 이를 제거합니다.
            if cleaned_response.startswith("```"):
                lines = cleaned_response.splitlines()
                if lines[0].startswith("```"):
                    lines = lines[1:]
                if lines and lines[-1].startswith("```"):
                    lines = lines[:-1]
                cleaned_response = "\n".join(lines).strip()

            data = json.loads(cleaned_response)

            refined_title = data.get("title")
            refined_description = data.get("description")

            if not refined_title or not refined_description:
                logger.warning(
                    "LLM response is missing required keys 'title' or 'description'. Falling back."
                )
                return draft

            if not isinstance(refined_title, str) or not isinstance(
                refined_description, str
            ):
                logger.warning(
                    "LLM response 'title' or 'description' is not a string. Falling back."
                )
                return draft

            refined_title_stripped = refined_title.strip()
            refined_desc_stripped = refined_description.strip()

            if not refined_title_stripped or not refined_desc_stripped:
                logger.warning(
                    "LLM response 'title' or 'description' is empty string. Falling back."
                )
                return draft

            # Pydantic의 model_copy를 활용해 title과 description만 교체하고 나머지 필드는 원본 draft를 철저히 보존합니다.
            return draft.model_copy(
                update={
                    "title": refined_title_stripped,
                    "description": refined_desc_stripped,
                }
            )

        except Exception as exc:
            logger.warning(
                "Failed to refine quest phrases via LLM: %s. Falling back to original draft.",
                exc,
            )
            return draft
