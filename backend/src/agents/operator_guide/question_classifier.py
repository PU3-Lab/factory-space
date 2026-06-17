"""Rule-based and LLM-based question classification for the Manual Q&A proto.

이 모듈은 플레이어 질문의 의도(Intent)를 분류하고, 실시간 게임 상태가 필요한지 여부를 판별하는 역할을 합니다.
"""

from __future__ import annotations

import json
import logging

from agents.operator_guide.csv_repository import CsvManualQARepository
from agents.operator_guide.schemas import ManualQAIntent, QuestionType
from llm.adapter import LLMAdapter, NoopLLMAdapter

logger = logging.getLogger(__name__)

TROUBLESHOOTING_KEYWORDS = (
    "왜",
    "안 돌아",
    "안돌아",
    "작동하지",
    "멈",
    "고장",
    "부족",
    "막",
)
RECIPE_KEYWORDS = ("필요", "만들려면", "재료", "요구")
RESOURCE_PRODUCTION_KEYWORDS = ("어떻게 만들어", "어떻게 만들", "생산", "제작")
EQUIPMENT_KEYWORDS = ("뭐야", "무엇", "역할", "설명")


class ManualQAQuestionClassifier:
    """단순 규칙 기반으로 플레이어의 질문 카테고리를 분류하는 클래스입니다.

    초보자용 설명:
        질문 텍스트에 포함된 키워드(예: '왜', '어떻게 만들어', '뭐야') 및 데이터베이스 내의
        장비/자원/제작법 명칭 일치 여부를 기반으로 질문 유형(QuestionType)을 판별합니다.
    """

    def __init__(self, repository: CsvManualQARepository) -> None:
        self._repository = repository

    def classify(self, question: str) -> ManualQAIntent:
        equipment = self._repository.find_equipment_by_question(question)
        resource = self._repository.find_resource_by_question(question)
        recipe = self._repository.find_recipe_by_question(question)

        if self._has_any(question, TROUBLESHOOTING_KEYWORDS):
            target_ids = []
            if equipment is not None:
                target_ids.append(equipment.equipment_id)
            target_ids.append("issue_machine_stopped")
            return ManualQAIntent(
                question_type="troubleshooting_question",
                primary_manual="troubleshooting",
                supporting_manuals=[
                    "equipment",
                    "resources",
                    "recipes",
                    "action_policy",
                ],
                target_ids=target_ids,
            )

        if resource is not None and self._has_any(question, RECIPE_KEYWORDS):
            recipe_for_resource = self._repository.find_recipe_by_output_resource(
                resource.resource_id
            )
            if recipe_for_resource is not None:
                return ManualQAIntent(
                    question_type="recipe_question",
                    primary_manual="recipes",
                    supporting_manuals=["resources", "equipment"],
                    target_ids=[recipe_for_resource.recipe_id, resource.resource_id],
                )

        if resource is not None and self._has_any(
            question, RESOURCE_PRODUCTION_KEYWORDS
        ):
            target_ids = [resource.resource_id]
            recipe_for_resource = self._repository.find_recipe_by_output_resource(
                resource.resource_id
            )
            if recipe_for_resource is not None:
                target_ids.append(recipe_for_resource.recipe_id)
            return ManualQAIntent(
                question_type="resource_question",
                primary_manual="resources",
                supporting_manuals=["recipes", "equipment"],
                target_ids=target_ids,
            )

        if recipe is not None and self._has_any(question, RECIPE_KEYWORDS):
            return ManualQAIntent(
                question_type="recipe_question",
                primary_manual="recipes",
                supporting_manuals=["resources", "equipment"],
                target_ids=[recipe.recipe_id, *_recipe_resource_ids(recipe)],
            )

        if equipment is not None and self._has_any(question, EQUIPMENT_KEYWORDS):
            return ManualQAIntent(
                question_type="equipment_question",
                primary_manual="equipment",
                supporting_manuals=["recipes", "troubleshooting"],
                target_ids=[equipment.equipment_id],
            )

        return ManualQAIntent(
            question_type="unknown_question",
            primary_manual="unknown",
            supporting_manuals=[],
            target_ids=[],
        )

    def _has_any(self, question: str, keywords: tuple[str, ...]) -> bool:
        return any(keyword in question for keyword in keywords)


class ContextNeedClassifier:
    """질문 텍스트와 의도 종류를 종합적으로 분석하여 게임 상태 정보(Game State)의 필요성 유무를 판단하는 분류기입니다.

    초보자용 설명:
        플레이어가 던진 질문이 현재 공장의 상황(예: 전력, 기계 속의 아이템 목록, 컨베이어 연결, 에러 등)을
        참고해야만 답할 수 있는 문제 해결성 질문인지 판단합니다.
        이 클래스는 LLMAdapter를 통해 지능적으로 판단하거나, LLM 호출 실패 시 규칙 기반으로 안전하게 fallback합니다.
    """

    def __init__(self, llm_adapter: LLMAdapter | None = None) -> None:
        self._llm_adapter = llm_adapter
        self._all_scopes = [
            "selectedMachine",
            "inputInventory",
            "outputInventory",
            "powerStatus",
            "currentRecipe",
            "connectedConveyors",
            "recentErrorEvents",
        ]

    def classify_need(
        self,
        question: str,
        intent_type: QuestionType,
    ) -> tuple[bool, list[str]]:
        """플레이어 질문과 질문 유형을 보고 실시간 게임 상태가 필요한지와 필요한 정보 범위(Scope)를 반환합니다.

        동작 흐름:
            1. LLMAdapter가 제공된 경우, LLM에게 질의하여 판단을 내립니다.
            2. LLM 응답을 파싱하여 결과를 도출하고, 실패 시 규칙 기반 fallback 로직을 수행합니다.
        """
        if self._llm_adapter is not None and not isinstance(
            self._llm_adapter,
            NoopLLMAdapter,
        ):
            try:
                prompt = self._build_prompt(question, intent_type)
                response = self._llm_adapter.invoke(prompt)
                if response:
                    parsed = self._parse_json_response(response)
                    if parsed is not None:
                        req_state, scopes = parsed
                        filtered_scopes = [s for s in scopes if s in self._all_scopes]
                        return req_state, filtered_scopes
            except Exception as e:
                logger.warning(
                    "LLM ContextNeedClassifier failed: %s. Falling back to rule-based.",
                    e,
                )

        return self._rule_based_classify(question, intent_type)

    def _build_prompt(self, question: str, intent_type: str) -> str:
        scopes_list = ", ".join(self._all_scopes)
        return f"""You are a factory guide assistant. Analyze if the player's question requires the current game state (like machine status, inventory, conveyors, or errors) to answer.

Player Question: {question}
Intent Type: {intent_type}

Return exactly one JSON object. Do not wrap in markdown code blocks.
Required JSON keys:
- "requires_current_game_state": true or false
- "required_state_scopes": list of scopes needed from [{scopes_list}]

Example Output:
{{
  "requires_current_game_state": true,
  "required_state_scopes": ["powerStatus", "inputInventory"]
}}
"""

    def _parse_json_response(self, text: str) -> tuple[bool, list[str]] | None:
        try:
            cleaned = text.strip()
            if cleaned.startswith("`"):
                lines = cleaned.splitlines()
                if lines[0].startswith("`"):
                    lines = lines[1:]
                if lines and lines[-1].startswith("`"):
                    lines = lines[:-1]
                cleaned = "\n".join(lines).strip()

            data = json.loads(cleaned)
            req_state = bool(data.get("requires_current_game_state", False))
            scopes = list(data.get("required_state_scopes", []))
            return req_state, [str(s) for s in scopes]
        except Exception:
            return None

    def _rule_based_classify(self, question: str, intent_type: QuestionType) -> tuple[bool, list[str]]:
        if intent_type == "troubleshooting_question":
            return True, list(self._all_scopes)
        return False, []


def _recipe_resource_ids(recipe: object) -> list[str]:
    return [item.split(":", 1)[0] for item in getattr(recipe, "input_resources")]
