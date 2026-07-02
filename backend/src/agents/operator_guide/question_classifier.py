"""Rule-based and LLM-based question classification for the Manual Q&A proto.

이 모듈은 플레이어 질문의 의도(Intent)를 분류하고, 실시간 게임 상태가 필요한지 여부를 판별하는 역할을 합니다.
"""

from __future__ import annotations

import json
import logging
from typing import Any

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
RECIPE_KEYWORDS = ("필요", "만들려면", "재료", "요구", "레시피")
RESOURCE_PRODUCTION_KEYWORDS = (
    "어떻게 만들어",
    "어떻게 만들",
    "만드는",
    "만들기",
    "지어",
    "지으",
    "짓",
    "건설",
    "건축",
    "조립",
    "생산",
    "제작",
)
EQUIPMENT_KEYWORDS = ("뭐야", "무엇", "역할", "설명")


class ManualQAQuestionClassifier:
    """단순 규칙 기반으로 플레이어의 질문 카테고리를 분류하는 클래스입니다.

    초보자용 설명:
        질문 텍스트에 포함된 키워드(예: '왜', '어떻게 만들어', '뭐야') 및 데이터베이스 내의
        장비/자원/제작법 명칭 일치 여부를 기반으로 질문 유형(QuestionType)을 판별합니다.
    """

    def __init__(self, repository: CsvManualQARepository) -> None:
        self._repository = repository

    def classify(self, question: str, context: dict[str, Any] | None = None) -> ManualQAIntent:
        """단순 규칙 기반으로 플레이어의 질문 카테고리를 분류하고 애매성(ambiguity)을 판별합니다.

        초보자용 설명:
            질문에 대상 기기/아이템 이름이 있으나 의도(생산법, 역할, 트러블슈팅 등)를 명확히 구분하기
            어려운 경우(예: '통신탑 알려줘')를 감지하여 is_ambiguous 플래그를 True로 설정합니다.
            이후 Sprint 19에서 이 플래그가 켜진 질문들을 LLM 분류기로 보강하게 됩니다.
        """
        equipment = self._repository.find_equipment_by_question(question)
        resource = self._repository.find_resource_by_question(question)
        recipe = self._repository.find_recipe_by_question(question)

        question_type = "unknown_question"
        primary_manual = "unknown"
        supporting_manuals = []
        target_ids = []

        if self._has_any(question, TROUBLESHOOTING_KEYWORDS):
            question_type = "troubleshooting_question"
            primary_manual = "troubleshooting"
            supporting_manuals = [
                "equipment",
                "resources",
                "recipes",
                "action_policy",
            ]
            if equipment is not None:
                target_ids.append(equipment.equipment_id)
            target_ids.append("issue_machine_stopped")

        elif resource is not None and self._has_any(question, RECIPE_KEYWORDS):
            recipe_for_resource = self._repository.find_recipe_by_output_resource(
                resource.resource_id
            )
            if recipe_for_resource is not None:
                question_type = "recipe_question"
                primary_manual = "recipes"
                supporting_manuals = ["resources", "equipment"]
                target_ids = [recipe_for_resource.recipe_id, resource.resource_id]

        elif resource is not None and self._has_any(
            question, RESOURCE_PRODUCTION_KEYWORDS
        ):
            target_ids = [resource.resource_id]
            recipe_for_resource = self._repository.find_recipe_by_output_resource(
                resource.resource_id
            )
            if recipe_for_resource is not None:
                target_ids.append(recipe_for_resource.recipe_id)
            question_type = "resource_question"
            primary_manual = "resources"
            supporting_manuals = ["recipes", "equipment"]

        elif recipe is not None and self._has_any(question, RECIPE_KEYWORDS):
            question_type = "recipe_question"
            primary_manual = "recipes"
            supporting_manuals = ["resources", "equipment"]
            target_ids = [recipe.recipe_id, *_recipe_resource_ids(recipe)]

        elif equipment is not None and self._has_any(question, EQUIPMENT_KEYWORDS):
            question_type = "equipment_question"
            primary_manual = "equipment"
            supporting_manuals = ["recipes", "troubleshooting"]
            target_ids = [equipment.equipment_id]

        # 애매함(Ambiguity) 판별 로직
        is_ambiguous = False

        # 조건 1: 동일 표시명이 장비(equipment)와 자원(resource) 양쪽에 있는 경우
        is_double_presence = (equipment is not None and resource is not None)
        if is_double_presence:
            # 명확한 의도 지시어(만들어, 지어, 뭐야 등)가 없는 경우 애매한 것으로 판단
            specific_keywords = (
                "만들어", "만들기", "만드는", "지어", "짓", "조립", "건설", "생산", "제작",
                "뭐야", "무엇", "역할", "설명"
            )
            if not self._has_any(question, specific_keywords):
                is_ambiguous = True
                if equipment.equipment_id not in target_ids:
                    target_ids.append(equipment.equipment_id)
                if resource.resource_id not in target_ids:
                    target_ids.append(resource.resource_id)

        # 조건 2/3: 결과가 unknown_question이지만 매칭된 대상(CSV 후보)이 있거나, 
        # 혹은 context 상의 selectedMachine이 있어 대상 추론이 가능한 경우
        if question_type == "unknown_question":
            has_question_target = (equipment is not None or resource is not None or recipe is not None)
            
            has_context_target = False
            if context:
                game_state = context.get("current_game_state")
                if isinstance(game_state, dict):
                    selected_machine = game_state.get("selectedMachine")
                    if selected_machine and self._repository.get_equipment(selected_machine):
                        has_context_target = True

            if has_question_target or has_context_target:
                is_ambiguous = True

        # unknown_question 상태에서 대상이 식별되었다면 target_ids에 후보군 추가
        if question_type == "unknown_question":
            if equipment is not None and equipment.equipment_id not in target_ids:
                target_ids.append(equipment.equipment_id)
            if resource is not None and resource.resource_id not in target_ids:
                target_ids.append(resource.resource_id)
            if recipe is not None and recipe.recipe_id not in target_ids:
                target_ids.append(recipe.recipe_id)

        return ManualQAIntent(
            question_type=question_type,
            primary_manual=primary_manual,
            supporting_manuals=supporting_manuals,
            target_ids=target_ids,
            is_ambiguous=is_ambiguous,
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

    def _rule_based_classify(
        self, question: str, intent_type: QuestionType
    ) -> tuple[bool, list[str]]:
        if intent_type == "troubleshooting_question":
            return True, list(self._all_scopes)
        return False, []


def _recipe_resource_ids(recipe: object) -> list[str]:
    return [item.split(":", 1)[0] for item in getattr(recipe, "input_resources")]


class LLMIntentClassifier:
    """LLM 보조 의도 분류기.

    초보자용 설명:
        규칙 기반 분류기에서 모호함(is_ambiguous=True)을 감지했을 때만 실행됩니다.
        전체 CSV 데이터를 전달하지 않고, 규칙 매칭으로 추출된 후보군(Intents, Targets)만을
        LLM에 제공하여 안전하고 토큰 효율적인 방식으로 의도를 보정합니다.
    """

    def __init__(self, llm_adapter: LLMAdapter | None, repository: CsvManualQARepository) -> None:
        self._llm_adapter = llm_adapter
        self._repository = repository
        self._candidate_intents = [
            "equipment_question",
            "resource_question",
            "recipe_question",
            "troubleshooting_question",
            "unknown_question",
        ]

    def classify_ambiguous(self, question: str, rule_intent: ManualQAIntent) -> ManualQAIntent:
        """모호한 질문에 대해 LLM에게 후보군을 질의하여 의도(question_type, target_ids)를 보정합니다.

        실패 조건 (다음 조건 중 하나에 해당하면 원래 규칙 기반 결과를 사용):
            1. llm_adapter가 제공되지 않았거나 Noop인 경우
            2. LLM 호출 오류/타임아웃 발생
            3. 반환된 JSON 파싱 실패
            4. 허용되지 않은 question_type이거나 존재하지 않는 target_ids인 경우
        """
        if self._llm_adapter is None or isinstance(self._llm_adapter, NoopLLMAdapter):
            return rule_intent

        try:
            # 후보 타겟 구성
            candidate_targets = []
            for target_id in rule_intent.target_ids:
                target_type = None
                title = None
                
                # 장비 확인
                if (eq := self._repository.get_equipment(target_id)) is not None:
                    target_type = "equipment"
                    title = eq.name
                # 자원 확인
                elif (res := self._repository.get_resource(target_id)) is not None:
                    target_type = "resource"
                    title = res.name
                # 레시피 확인
                elif (rec := self._repository.get_recipe(target_id)) is not None:
                    target_type = "recipe"
                    title = rec.name
                
                if target_type:
                    candidate_targets.append({
                        "id": target_id,
                        "type": target_type,
                        "title": title
                    })

            # 프롬프트 생성
            prompt = self._build_prompt(question, candidate_targets)
            response = self._llm_adapter.invoke(prompt)
            if not response:
                return rule_intent

            # 파싱 및 검증
            parsed = self._parse_json_response(response)
            if parsed is None:
                return rule_intent

            q_type = parsed.get("question_type")
            target_ids = parsed.get("target_ids", [])

            # Validation 1: 허용된 question_type 인지 검증
            if q_type not in self._candidate_intents:
                return rule_intent

            # Validation 2: 반환된 target_id들이 저장소에 존재하는지 검증
            validated_targets = []
            for tid in target_ids:
                is_valid = (
                    self._repository.get_equipment(tid) is not None
                    or self._repository.get_resource(tid) is not None
                    or self._repository.get_recipe(tid) is not None
                )
                if is_valid:
                    validated_targets.append(tid)

            # 유효한 타겟이 없다면 (unknown_question이 아닌데 타겟이 없으면 룰 기반 폴백)
            if q_type != "unknown_question" and not validated_targets:
                return rule_intent

            # primary_manual 매핑
            primary_manual = "unknown"
            supporting_manuals = []
            if q_type == "equipment_question":
                primary_manual = "equipment"
                supporting_manuals = ["recipes", "troubleshooting"]
            elif q_type == "resource_question":
                primary_manual = "resources"
                supporting_manuals = ["recipes", "equipment"]
            elif q_type == "recipe_question":
                primary_manual = "recipes"
                supporting_manuals = ["resources", "equipment"]
            elif q_type == "troubleshooting_question":
                primary_manual = "troubleshooting"
                supporting_manuals = ["equipment", "resources", "recipes", "action_policy"]

            return ManualQAIntent(
                question_type=q_type,
                primary_manual=primary_manual,
                supporting_manuals=supporting_manuals,
                target_ids=validated_targets,
                is_ambiguous=False, # LLM 보정으로 애매함 해결!
            )

        except Exception as e:
            logger.warning("LLMIntentClassifier classification failed: %s. Falling back to rule-based.", e)
            return rule_intent

    def _build_prompt(self, question: str, candidate_targets: list[dict[str, str]]) -> str:
        targets_json = json.dumps(candidate_targets, ensure_ascii=False, indent=2)
        intents_json = json.dumps(self._candidate_intents, ensure_ascii=False)
        return f"""You are a factory guide assistant. Clarify the player's intent for the given question.
The question is ambiguous, and you must select the most appropriate question type and identify the relevant target IDs.

Player Question: {question}

Candidate Intents: {intents_json}
Candidate Targets: {targets_json}

Return exactly one JSON object. Do not wrap in markdown code blocks.
Required JSON keys:
- "question_type": One of the candidate intents.
- "target_ids": List of relevant target IDs from the candidate targets.
- "confidence": "high", "medium", or "low".
- "reason": A short reason for classification in Korean.

Example Output:
{{
  "question_type": "resource_question",
  "target_ids": ["resource_TeleCommunicationTower", "recipe_make_telecommunication_tower"],
  "confidence": "high",
  "reason": "질문이 통신탑의 제작 방법을 묻고 있습니다."
}}
"""

    def _parse_json_response(self, text: str) -> dict[str, Any] | None:
        try:
            cleaned = text.strip()
            if cleaned.startswith("`"):
                lines = cleaned.splitlines()
                if lines[0].startswith("`"):
                    lines = lines[1:]
                if lines and lines[-1].startswith("`"):
                    lines = lines[:-1]
                cleaned = "\n".join(lines).strip()

            return json.loads(cleaned)
        except Exception:
            return None
