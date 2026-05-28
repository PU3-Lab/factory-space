"""Quest service."""

from __future__ import annotations

from factory_space.agents.quest.schemas import (
    Quest,
    QuestGenerationRequest,
    QuestGenerationResult,
    QuestMachine,
    QuestObjective,
    QuestWorldObject,
)
from factory_space.core.actions.schemas import Action


class QuestAgentService:
    """Generate simple quests from mock game state."""

    def generate_quest_json(self, payload: dict[str, object]) -> str:
        """Generate, validate, and serialize a quest response as JSON."""

        request = QuestGenerationRequest.model_validate(payload)
        result = self.generate_quest(request)
        validated = QuestGenerationResult.model_validate(result.model_dump())
        return validated.model_dump_json()

    def generate_quest(
        self,
        request: QuestGenerationRequest,
    ) -> QuestGenerationResult:
        """Generate a quest from normalized request data."""

        bottleneck = self._find_bottleneck_machine(request.game_state.machines)
        if bottleneck is not None:
            return self._build_bottleneck_result(bottleneck)

        control_panel = self._find_control_panel(
            request.object_id,
            request.game_state.nearby_objects,
        )
        if control_panel is not None:
            return self._build_control_panel_result(control_panel)

        target_object_id = request.object_id or "current_area"
        return self._build_exploration_result(target_object_id)

    def _find_bottleneck_machine(
        self,
        machines: list[QuestMachine],
    ) -> QuestMachine | None:
        for machine in machines:
            if machine.status in {"blocked", "warning", "error"}:
                return machine
            if machine.throughput_ratio < 0.75:
                return machine
        return None

    def _find_control_panel(
        self,
        object_id: str | None,
        nearby_objects: list[QuestWorldObject],
    ) -> QuestWorldObject | None:
        if object_id is not None and "control_panel" in object_id:
            return QuestWorldObject(id=object_id, type="control_panel")

        for nearby_object in nearby_objects:
            if nearby_object.type == "control_panel" or "control_panel" in nearby_object.id:
                return nearby_object
        return None

    def _build_bottleneck_result(
        self,
        machine: QuestMachine,
    ) -> QuestGenerationResult:
        quest = Quest(
            quest_id=f"quest-optimize-{machine.id}",
            title="Inspect the bottleneck machine",
            description=(
                f"{machine.id} is producing below the expected throughput. "
                "Inspect it before the line backs up."
            ),
            priority="high",
            objective=QuestObjective(
                description=f"Inspect {machine.id} and report the bottleneck cause.",
                target_object_id=machine.id,
                required_event="machine_inspected",
            ),
        )
        text = "A production bottleneck was detected. Inspect the highlighted machine."
        return QuestGenerationResult(
            text=text,
            quest=quest,
            actions=self._build_actions(text, quest),
            metadata={"source": "mock_game_state", "reason": "bottleneck_detected"},
        )

    def _build_control_panel_result(
        self,
        control_panel: QuestWorldObject,
    ) -> QuestGenerationResult:
        quest = Quest(
            quest_id=f"quest-inspect-{control_panel.id}",
            title="Inspect the control panel",
            description="A nearby control panel can reveal the next factory objective.",
            priority="medium",
            objective=QuestObjective(
                description=f"Interact with {control_panel.id}.",
                target_object_id=control_panel.id,
                required_event="player_interacted",
            ),
        )
        text = "A quest is available. Inspect the highlighted control panel."
        return QuestGenerationResult(
            text=text,
            quest=quest,
            actions=self._build_actions(text, quest),
            metadata={"source": "mock_game_state", "reason": "control_panel_nearby"},
        )

    def _build_exploration_result(self, target_object_id: str) -> QuestGenerationResult:
        quest = Quest(
            quest_id=f"quest-explore-{target_object_id}",
            title="Explore the current area",
            description="Gather more factory context to unlock a more specific objective.",
            priority="low",
            objective=QuestObjective(
                description="Move through the area and interact with a relevant object.",
                target_object_id=target_object_id,
                required_event="player_interacted",
            ),
        )
        text = "Explore the area to discover the next factory objective."
        return QuestGenerationResult(
            text=text,
            quest=quest,
            actions=self._build_actions(text, quest),
            metadata={"source": "mock_game_state", "reason": "fallback"},
        )

    def _build_actions(self, text: str, quest: Quest) -> list[Action]:
        return [
            Action(name="show_ui_message", args={"text": text}),
            Action(
                name="highlight_object",
                args={"object_id": quest.objective.target_object_id},
            ),
            Action(
                name="update_quest_marker",
                args={
                    "quest_id": quest.quest_id,
                    "object_id": quest.objective.target_object_id,
                    "state": "available",
                },
            ),
        ]
