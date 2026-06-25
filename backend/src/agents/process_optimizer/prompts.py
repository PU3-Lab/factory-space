"""Prompt helpers for the Process Optimizer v2 explanation layer."""

from __future__ import annotations

import json
from typing import Any

from agents.pipeline.middleware import build_current_model_metadata


def clean_json_object_text(raw: str | None) -> str:
    """Return raw model text with optional Markdown JSON fences removed."""
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
    """Build the explanation-only prompt for process_optimizer preview responses."""
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
    """Use LLM only to enrich process_optimizer player-facing explanation text."""
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
