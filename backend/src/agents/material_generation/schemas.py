"""Pydantic schemas for the material_generation agent."""

from __future__ import annotations

from pydantic import BaseModel, Field


class InputItemSchema(BaseModel):
    """Input item specification."""

    item_id: str
    qty: int


class ProcessConditionsSchema(BaseModel):
    """Optional environmental process conditions for synthesis."""

    temperature: str = "default"
    pressure: str = "default"
    catalyst: str | None = None


class MaterialCreationRequest(BaseModel):
    """Incoming request to synthesize a material."""

    machine_type: str
    inputs: list[InputItemSchema]
    process_conditions: ProcessConditionsSchema = Field(
        default_factory=ProcessConditionsSchema
    )
    player_id: str
    generate_visual_asset: bool = True


class MaterialProperties(BaseModel):
    """Attributes defining physical properties of a material."""

    strength: float
    conductivity: float
    stability: float
    reactivity: float


class MaterialProposalResult(BaseModel):
    """Attributes of the new material proposed by the LLM."""

    id_hint: str
    name: str
    category: str
    rarity: str
    description: str
    properties: MaterialProperties
    risks: list[str] = Field(default_factory=list)
    usage: list[str] = Field(default_factory=list)
    next_recipe_candidates: list[str] = Field(default_factory=list)
    visual_prompt: str


class MaterialProposal(BaseModel):
    """Wrapper encapsulating the LLM's classification and proposal details."""

    proposal_type: str
    confidence: float
    reason: str
    result: MaterialProposalResult | None = None


class OutputItemSchema(BaseModel):
    """Output item specification."""

    item_id: str
    qty: int


class MaterialCreationResponse(BaseModel):
    """Final outcome returned to the user."""

    result_type: str  # "existing_recipe", "new_material", "cached_experiment", "failed_result", "invalid_input"
    experiment_hash: str

    recipe_name: str | None = None
    outputs: list[OutputItemSchema] | None = None

    material_id: str | None = None
    material_hash: str | None = None
    name: str | None = None
    rarity: str | None = None
    generation_status: str | None = None
    visual_status: str | None = None
    fallback_icon: str | None = None
    message: str | None = None

    cached: bool | None = None
    failure_reason: str | None = None
