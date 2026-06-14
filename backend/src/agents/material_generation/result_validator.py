"""Validates, sanitizes, and balances LLM-proposed material specifications."""

from __future__ import annotations

from agents.material_generation.schemas import MaterialProposal

FORBIDDEN_KEYWORDS = {"trash", "error", "test", "dummy", "fuck", "shit", "invalid"}
VALID_RARITIES = {"common", "uncommon", "rare", "epic"}


class MaterialResultValidator:
    """Verifies material balance constraints and performs safety sanitization."""

    @classmethod
    def validate_and_correct(cls, proposal: MaterialProposal) -> MaterialProposal:
        """Correct properties out of bounds, sanitize forbidden names, and cap candidate outputs."""
        if not proposal.result:
            return proposal

        result = proposal.result

        # 1. Properties Clamping (Enforce range: 0.0 <= property_val <= 10.0)
        p = result.properties
        p.strength = max(0.0, min(10.0, float(p.strength)))
        p.conductivity = max(0.0, min(10.0, float(p.conductivity)))
        p.stability = max(0.0, min(10.0, float(p.stability)))
        p.reactivity = max(0.0, min(10.0, float(p.reactivity)))

        # 2. Rarity Sanitization
        rarity_clean = result.rarity.strip().lower()
        if rarity_clean not in VALID_RARITIES:
            result.rarity = "common"
        else:
            result.rarity = rarity_clean

        # 3. Sanitize names and hints containing forbidden terms
        name_lower = result.name.lower()
        id_lower = result.id_hint.lower()

        for term in FORBIDDEN_KEYWORDS:
            if term in name_lower:
                result.name = result.name.replace(term, "alloy").replace(
                    term.capitalize(), "Alloy"
                )
            if term in id_lower:
                result.id_hint = result.id_hint.replace(term, "alloy")

        # 4. Limit recipe candidate recommendations to a maximum of 5 to avoid clutter
        if len(result.next_recipe_candidates) > 5:
            result.next_recipe_candidates = result.next_recipe_candidates[:5]

        return proposal
