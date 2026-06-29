"""Process Optimizer의 LLM 설명 계층에 필요한 프롬프트를 구성합니다.

결정론적 코드가 만든 미리보기를 LLM이 플레이어 친화적인 한국어로 다듬게 하며,
모델 응답은 기존 제안의 설명 필드에만 제한적으로 반영합니다.
"""

from __future__ import annotations

import json
from typing import Any

from agents.pipeline.middleware import build_current_model_metadata


def clean_json_object_text(raw: str | None) -> str:
    """모델 응답에서 선택적인 Markdown JSON 코드 블록을 제거합니다.

    Args:
        raw: LLM이 반환한 원본 문자열입니다.

    Returns:
        앞뒤 공백과 코드 블록 표시를 제거한 JSON 문자열입니다.
    """
    if not raw:
        return ""
    cleaned = raw.strip()
    if cleaned.startswith("```"):
        lines = cleaned.splitlines()
        if (
            len(lines) >= 2
            and lines[0].startswith("```")
            and lines[-1].startswith("```")
        ):
            cleaned = "\n".join(lines[1:-1]).strip()
    return cleaned


def build_process_optimizer_explanation_prompt(
    response_payload: dict[str, Any],
) -> str:
    """미리보기 설명만 생성하도록 제한한 LLM 프롬프트를 만듭니다.

    Args:
        response_payload: 결정론적 분석과 검증을 마친 미리보기 응답입니다.

    Returns:
        새 명령이나 수치를 만들지 못하도록 규칙과 출력 스키마를 포함한 프롬프트입니다.
    """
    return (
        "You are the explanation layer for a factory process optimizer agent.\n"
        "Deterministic code already calculated bottlenecks, safety, risks, and expected effects.\n"
        "Do not invent new machines, new commands, new numeric metrics, or execution results.\n"
        "Only rewrite the provided preview into clear Korean text for a player who must review and approve it.\n"
        "Return JSON only with this schema:\n"
        "{\n"
        '  "summary": "player-facing one sentence summary",\n'
        '  "player_message": "short approval guidance",\n'
        '  "change_explanations": [\n'
        '    {"id": "existing change id", "reason": "why this matters", '
        '"priority_explanation": "why this priority is appropriate", '
        '"expected_effect_text": "plain-language effect"}\n'
        "  ]\n"
        "}\n\n"
        "Preview payload:\n"
        f"{json.dumps(response_payload, ensure_ascii=False, sort_keys=True)}"
    )


def apply_process_optimizer_llm_explanation(
    response_payload: dict[str, Any],
    llm_slots: tuple[Any, Any, Any],
) -> tuple[dict[str, Any], dict[str, Any]]:
    """LLM 결과를 미리보기의 플레이어용 설명 필드에만 반영합니다.

    Args:
        response_payload: 코드가 생성한 원본 Process Optimizer 응답입니다.
        llm_slots: 기본 모델과 fallback 모델 호출 정보를 순서대로 담은 슬롯입니다.

    Returns:
        설명이 보강된 응답 payload와 실제 모델 사용 정보를 담은 메타데이터입니다.
    """
    if response_payload.get("status") != "preview":
        return response_payload, {"llm": "not_applicable"}

    changes = response_payload.get("changes")
    if not isinstance(changes, list) or not changes:
        return response_payload, {"llm": "not_applicable"}

    prompt = build_process_optimizer_explanation_prompt(response_payload)
    for slot in llm_slots:
        raw = slot.adapter.invoke(prompt)
        if not raw:
            continue
        try:
            parsed = json.loads(clean_json_object_text(raw))
        except Exception:
            continue
        if not isinstance(parsed, dict):
            continue

        enriched_payload = dict(response_payload)
        summary = parsed.get("summary")
        if isinstance(summary, str) and summary.strip():
            enriched_payload["summary"] = summary.strip()

        player_message = parsed.get("player_message")
        if isinstance(player_message, str) and player_message.strip():
            enriched_payload["player_message"] = player_message.strip()

        explanations_by_id: dict[str, dict[str, Any]] = {}
        raw_explanations = parsed.get("change_explanations")
        if isinstance(raw_explanations, list):
            for item in raw_explanations:
                if isinstance(item, dict) and isinstance(item.get("id"), str):
                    explanations_by_id[item["id"]] = item

        enriched_changes = []
        for change in changes:
            if not isinstance(change, dict):
                enriched_changes.append(change)
                continue
            enriched_change = dict(change)
            explanation = explanations_by_id.get(str(change.get("id")))
            if explanation:
                for source_key, target_key in (
                    ("reason", "reason"),
                    ("priority_explanation", "priority_explanation"),
                    ("expected_effect_text", "expected_effect"),
                ):
                    value = explanation.get(source_key)
                    if isinstance(value, str) and value.strip():
                        enriched_change[target_key] = value.strip()
            enriched_changes.append(enriched_change)

        enriched_payload["changes"] = enriched_changes
        enriched_payload["suggestions"] = enriched_changes
        metadata: dict[str, Any] = {
            "llm": "used",
            "llmSlot": slot.name,
            "llmProvider": slot.provider,
        }
        if slot.model:
            metadata["llmModel"] = slot.model
        current_model = build_current_model_metadata(
            {
                "llmSlot": slot.name,
                "llmProvider": slot.provider,
                "llmModel": slot.model or "",
            }
        )
        if current_model is not None:
            metadata["currentModel"] = current_model
        return enriched_payload, metadata

    return response_payload, {
        "llm": "fallback",
        "fallbackReason": "llm_unavailable",
    }
