"""Rule generator for creating support quests based on factory known issues."""

from __future__ import annotations

import uuid

from agents.quest_generator import game_data
from agents.quest_generator.models import (
    QuestContext,
    QuestObjective,
    QuestReward,
    SupportQuestDraft,
)


class QuestRuleGenerator:
    """공장의 자원 부족 상태를 분석하여 collect_item 유형의 지원 퀘스트 초안들을 생성하는 규칙 엔진입니다."""

    # 밸런싱 및 상한선 상수 정의
    HARD_CAP = 1000
    NONPRODUCIBLE_CAP = 10
    QTY_SCALE_FACTOR = 10
    REWARD_SCALE_FACTOR = 100

    @classmethod
    def generate_drafts(
        cls, context: QuestContext, active_target_ids: set[str]
    ) -> list[SupportQuestDraft]:
        """현재 공장의 부족 자원(known_issues) 중 진행 중이지 않은 유효한 자원들에 대해 지원 퀘스트 초안 목록을 반환합니다.

        반환되는 리스트는 우선순위(known_issues 정렬 순서)에 따라 정렬되어 있습니다.
        후속 단계(Validator)에서 선행조건을 불충족하여 skip될 경우를 고려하여, 유효한 모든 후보군을 생성해 리턴합니다.

        Args:
            context: 정규화된 공장 상태 컨텍스트
            active_target_ids: 이미 진행 중인 지원 퀘스트의 대상 아이템 ID 집합

        Returns:
            생성된 지원 퀘스트 초안(SupportQuestDraft) 목록. 생성할 초안이 없는 경우 빈 리스트 반환
        """
        # 부족한 자원 이슈가 없거나 메인 퀘스트가 없는 경우 즉시 빈 리스트 반환
        if not context.known_issues or not context.current_main_quest:
            return []

        drafts = []

        # 진행 중인 지원 퀘스트 대상이 아닌 부족 자원(KnownIssue)들을 차례대로 순회
        for issue in context.known_issues:
            if issue.item_id in active_target_ids:
                continue

            # 레벨 게이트: 해당 아이템의 최소 요구 레벨이 공장 레벨보다 높은 경우 스킵
            min_level = game_data.get_item_min_level(issue.item_id)
            if min_level > context.factory_level:
                continue

            # 해당 부족 자원에 매칭되는 메인 퀘스트 목표를 검색
            main_objective = None
            for obj in context.current_main_quest.objectives:
                if obj.main_objective_id == issue.main_objective_id:
                    main_objective = obj
                    break

            # 메인 목표를 찾지 못한 경우(데이터 정합성 오류) 로그 없이 해당 자원만 건너뛰고 계속 탐색
            if not main_objective:
                continue

            # 한국어 자원 명칭 획득
            item_name = game_data.get_item_name(issue.item_id)

            # 퀘스트 정보 생성
            # 레벨 기반 수량 스케일링 적용 (기본: QTY_SCALE_FACTOR * 레벨)
            base_amount = cls.QTY_SCALE_FACTOR * context.factory_level

            # 생산성 여부(producible)에 따라 상한(Cap) 적용
            if issue.producible:
                target_amount = min(base_amount, cls.HARD_CAP)
            else:
                target_amount = min(base_amount, cls.NONPRODUCIBLE_CAP)

            title = f"{item_name} 확보 지원"
            description = (
                f"공장 지원을 위해 {item_name} {target_amount}개를 확보하세요."
            )

            # 퀘스트 목표(QuestObjective) 목록 구성 (obj_{uuid} 스킴 적용)
            objective_id = f"obj_{uuid.uuid4()}"
            objective = QuestObjective(
                id=objective_id,
                type="collect_item",
                target_id=issue.item_id,
                target_amount=target_amount,
                current_amount=0,
                status="in_progress",
            )

            # 퀘스트 보상(QuestReward) 구성: 레벨 비례 스케일링 (REWARD_SCALE_FACTOR * 레벨)
            reward = QuestReward(
                type="currency",
                target_id="gold",
                amount=cls.REWARD_SCALE_FACTOR * context.factory_level,
            )

            drafts.append(
                SupportQuestDraft(
                    title=title,
                    description=description,
                    quest_type="support",
                    support_type="collect_item",
                    objectives=[objective],
                    rewards=[reward],
                )
            )

        return drafts
