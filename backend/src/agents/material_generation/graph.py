"""LangGraph subgraph orchestrating the material generation workflow."""

from __future__ import annotations

import logging
import uuid
from typing import Any

from langgraph.graph import END, StateGraph
from sqlalchemy import select

from agents.material_generation.classifier import ExperimentClassifier
from agents.material_generation.graph_state import MaterialGraphState
from agents.material_generation.normalizer import (
    generate_experiment_hash,
    generate_material_hash,
    normalize_inputs,
)
from agents.material_generation.prevalidator import RecipePreValidator
from agents.material_generation.proposal_generator import MaterialProposalGenerator
from agents.material_generation.recipe_repository import RecipeRepository
from agents.material_generation.registry.experiment_registry import (
    ExperimentRegistryService,
)
from agents.material_generation.registry.material_registry import (
    MaterialRegistryService,
)
from agents.material_generation.result_validator import MaterialResultValidator
from agents.material_generation.schemas import (
    MaterialCreationResponse,
    OutputItemSchema,
)
from agents.material_generation.similarity import ExperimentSimilarityService
from db.models import GeneratedExperimentModel, GeneratedMaterialModel

logger = logging.getLogger(__name__)

_proposal_generator = None


def get_proposal_generator() -> MaterialProposalGenerator:
    """Lazy initialization of the proposal generator to prevent import-time side effects."""
    global _proposal_generator
    if _proposal_generator is None:
        _proposal_generator = MaterialProposalGenerator()
    return _proposal_generator


def normalize_node(state: MaterialGraphState) -> dict[str, Any]:
    """Node: Normalize input items and compute experiment hash."""
    request = state["request"]
    normalized = normalize_inputs(request.inputs)
    exp_hash = generate_experiment_hash(
        request.machine_type,
        normalized,
        request.process_conditions,
    )
    return {
        "normalized_inputs": normalized,
        "experiment_hash": exp_hash,
        "attempt": 1,
    }


def lookup_cache_node(state: MaterialGraphState) -> dict[str, Any]:
    """Node: Check experiment registry for cached results."""
    session = state["db"]
    exp_hash = state["experiment_hash"]

    existing_exp = ExperimentRegistryService.get_experiment_by_hash(session, exp_hash)
    if existing_exp:
        logger.info("Found cached experiment for hash: %s", exp_hash)
        if existing_exp.result_type == "new_material" and existing_exp.material_id:
            # Query material
            stmt = select(GeneratedMaterialModel).where(
                GeneratedMaterialModel.id == existing_exp.material_id
            )
            mat_model = session.execute(stmt).scalar_one_or_none()

            mat_name = mat_model.name if mat_model else "Unknown Alloy"
            mat_hash = mat_model.material_hash if mat_model else None
            mat_rarity = mat_model.rarity if mat_model else None
            visual_status = mat_model.visual_status if mat_model else None
            fallback_icon = mat_model.fallback_icon if mat_model else None

            response = MaterialCreationResponse(
                result_type="cached_experiment",
                experiment_hash=exp_hash,
                cached=True,
                material_id=existing_exp.material_id,
                material_hash=mat_hash,
                name=mat_name,
                rarity=mat_rarity,
                generation_status="cached",
                visual_status=visual_status,
                fallback_icon=fallback_icon,
                message="이미 발견된 물질입니다.",
            )
        elif existing_exp.result_type == "existing_recipe":
            outputs = []
            if existing_exp.output_items_json:
                outputs = [
                    OutputItemSchema(item_id=o["item_id"], qty=o["qty"])
                    for o in existing_exp.output_items_json
                ]
            response = MaterialCreationResponse(
                result_type="existing_recipe",
                experiment_hash=exp_hash,
                recipe_name=existing_exp.recipe_name,
                outputs=outputs,
            )
        else:
            response = MaterialCreationResponse(
                result_type=existing_exp.result_type,
                experiment_hash=exp_hash,
                failure_reason=existing_exp.failure_reason,
            )
        return {"response": response}
    return {}


def recipe_match_node(state: MaterialGraphState) -> dict[str, Any]:
    """Node: Search authored system recipes."""
    if state.get("response"):
        return {}

    session = state["db"]
    request = state["request"]
    normalized = state["normalized_inputs"]
    exp_hash = state["experiment_hash"]

    matched_recipe = RecipeRepository.match_recipe(
        session, request.machine_type, normalized
    )
    if matched_recipe:
        outputs = []
        if matched_recipe.output_item_1:
            outputs.append(
                {
                    "item_id": matched_recipe.output_item_1,
                    "qty": matched_recipe.output_qty_1,
                }
            )
        if matched_recipe.output_item_2:
            outputs.append(
                {
                    "item_id": matched_recipe.output_item_2,
                    "qty": matched_recipe.output_qty_2,
                }
            )

        new_exp = GeneratedExperimentModel(
            id=f"exp_{uuid.uuid4().hex[:10]}",
            experiment_hash=exp_hash,
            machine_type=request.machine_type,
            inputs_json=normalized,
            normalized_inputs_json=normalized,
            process_conditions_json=request.process_conditions.model_dump(),
            classification="simple_variation",
            result_type="existing_recipe",
            recipe_name=matched_recipe.recipe_name,
            output_items_json=outputs,
        )
        ExperimentRegistryService.save_experiment(session, new_exp)

        response = MaterialCreationResponse(
            result_type="existing_recipe",
            experiment_hash=exp_hash,
            recipe_name=matched_recipe.recipe_name,
            outputs=[
                OutputItemSchema(item_id=o["item_id"], qty=o["qty"]) for o in outputs
            ],
        )
        return {"response": response}
    return {}


def prevalidate_node(state: MaterialGraphState) -> dict[str, Any]:
    """Node: Deterministic validation checks."""
    if state.get("response"):
        return {}

    session = state["db"]
    request = state["request"]
    normalized = state["normalized_inputs"]
    exp_hash = state["experiment_hash"]

    validation_err = RecipePreValidator.validate_inputs(
        session, request.machine_type, normalized
    )
    if validation_err:
        new_exp = GeneratedExperimentModel(
            id=f"exp_{uuid.uuid4().hex[:10]}",
            experiment_hash=exp_hash,
            machine_type=request.machine_type,
            inputs_json=normalized,
            normalized_inputs_json=normalized,
            process_conditions_json=request.process_conditions.model_dump(),
            classification="ambiguous",
            result_type="invalid_input",
            failure_reason=validation_err,
        )
        ExperimentRegistryService.save_experiment(session, new_exp)

        response = MaterialCreationResponse(
            result_type="invalid_input",
            experiment_hash=exp_hash,
            failure_reason=validation_err,
        )
        return {"response": response, "error": validation_err}
    return {}


def classify_node(state: MaterialGraphState) -> dict[str, Any]:
    """Node: Classify experiment configuration."""
    if state.get("response"):
        return {}

    session = state["db"]
    request = state["request"]
    normalized = state["normalized_inputs"]

    classification = ExperimentClassifier.classify(
        session, request.machine_type, normalized
    )
    return {"classification": classification}


def handle_rule_node(state: MaterialGraphState) -> dict[str, Any]:
    """Node: Fast-fail non-synthesis categories."""
    if state.get("response"):
        return {}

    session = state["db"]
    request = state["request"]
    normalized = state["normalized_inputs"]
    exp_hash = state["experiment_hash"]
    classification = state["classification"]

    if classification in ("simple_variation", "intermediate_material", "failed_result"):
        result_type = "failed_result"
        failure_reason = "Synthesis failed."
        outputs = []

        if classification == "simple_variation":
            failure_reason = "Simple variation recipe mismatch (invalid quantities)."
        elif classification == "failed_result":
            failure_reason = (
                f"Machine policies prohibit alloy synthesis in {request.machine_type}."
            )
        elif classification == "intermediate_material":
            result_type = "failed_result"
            failure_reason = "Produced an intermediate mixed powder."
            outputs = [{"item_id": "mixed_powder", "qty": 1}]

        new_exp = GeneratedExperimentModel(
            id=f"exp_{uuid.uuid4().hex[:10]}",
            experiment_hash=exp_hash,
            machine_type=request.machine_type,
            inputs_json=normalized,
            normalized_inputs_json=normalized,
            process_conditions_json=request.process_conditions.model_dump(),
            classification=classification,
            result_type=result_type,
            output_items_json=outputs if outputs else None,
            failure_reason=failure_reason,
        )
        ExperimentRegistryService.save_experiment(session, new_exp)

        response = MaterialCreationResponse(
            result_type=result_type,
            experiment_hash=exp_hash,
            failure_reason=failure_reason,
            outputs=[
                OutputItemSchema(item_id=o["item_id"], qty=o["qty"]) for o in outputs
            ]
            if outputs
            else None,
        )
        return {"response": response}
    return {}


def similarity_context_node(state: MaterialGraphState) -> dict[str, Any]:
    """Node: Retrieve history of similar experiments for context."""
    if state.get("response"):
        return {}

    session = state["db"]
    request = state["request"]
    normalized = state["normalized_inputs"]

    similar_exps = ExperimentSimilarityService.find_similar_experiments(
        session, request.machine_type, normalized
    )
    return {"similar_context": similar_exps}


def llm_propose_node(state: MaterialGraphState) -> dict[str, Any]:
    """Node: Query LLM for material proposal."""
    if state.get("response"):
        return {}

    request = state["request"]
    normalized = state["normalized_inputs"]
    similar_exps = state.get("similar_context") or []

    proposal = get_proposal_generator().generate_proposal(
        request.machine_type, normalized, request.process_conditions, similar_exps
    )
    return {"proposal": proposal}


def validate_result_node(state: MaterialGraphState) -> dict[str, Any]:
    """Node: Enforce rules, sanitization, and manage retry state."""
    if state.get("response"):
        return {}

    proposal = state["proposal"]
    attempt = state["attempt"]

    if not proposal:
        # Generate safe fallback
        proposal = get_proposal_generator().get_fallback_proposal(
            state["normalized_inputs"]
        )

    proposal = MaterialResultValidator.validate_and_correct(proposal)

    if proposal.proposal_type == "failed" or not proposal.result:
        if attempt < 3:
            logger.info(
                "Proposal validation failed. Retrying proposal generation. Attempt %d -> %d",
                attempt,
                attempt + 1,
            )
            return {"proposal": proposal, "attempt": attempt + 1}
        else:
            session = state["db"]
            request = state["request"]
            normalized = state["normalized_inputs"]
            exp_hash = state["experiment_hash"]
            classification = state["classification"]

            new_exp = GeneratedExperimentModel(
                id=f"exp_{uuid.uuid4().hex[:10]}",
                experiment_hash=exp_hash,
                machine_type=request.machine_type,
                inputs_json=normalized,
                normalized_inputs_json=normalized,
                process_conditions_json=request.process_conditions.model_dump(),
                classification=classification,
                result_type="failed_result",
                failure_reason="Synthesis rejected by LLM analysis.",
            )
            ExperimentRegistryService.save_experiment(session, new_exp)

            response = MaterialCreationResponse(
                result_type="failed_result",
                experiment_hash=exp_hash,
                failure_reason="Synthesis rejected by LLM analysis.",
            )
            return {"response": response, "proposal": proposal}

    return {"proposal": proposal}


def deduplicate_material_node(state: MaterialGraphState) -> dict[str, Any]:
    """Node: Deduplicate attributes by checking existing material hashes."""
    if state.get("response"):
        return {}

    session = state["db"]
    proposal = state["proposal"]
    exp_hash = state["experiment_hash"]

    assert proposal.result is not None
    mat_hash = generate_material_hash(proposal.result)
    existing_mat = MaterialRegistryService.get_material_by_hash(session, mat_hash)

    if existing_mat:
        material_id = existing_mat.id
        mat_name = existing_mat.name
        mat_rarity = existing_mat.rarity
        visual_status = existing_mat.visual_status
        fallback_icon = existing_mat.fallback_icon
        is_new = False
    else:
        material_id = f"mat_{proposal.result.id_hint}_{uuid.uuid4().hex[:6]}"
        mat_name = proposal.result.name
        mat_rarity = proposal.result.rarity
        visual_status = "pending"
        fallback_icon = f"materials/default/{proposal.result.category}.png"
        is_new = True

    response = MaterialCreationResponse(
        result_type="new_material",
        experiment_hash=exp_hash,
        material_id=material_id,
        material_hash=mat_hash,
        name=mat_name,
        rarity=mat_rarity,
        generation_status="created" if is_new else "cached",
        visual_status=visual_status,
        fallback_icon=fallback_icon,
        message="새로운 물질이 발견되었습니다. 아이콘과 텍스처는 생성 중입니다."
        if is_new
        else "이미 발견된 물질입니다.",
    )
    return {"response": response, "is_new": is_new}


def register_material_node(state: MaterialGraphState) -> dict[str, Any]:
    """Node: Commit new material and discoveries, publishing asset pipeline events."""
    session = state["db"]
    request = state["request"]
    proposal = state["proposal"]
    exp_hash = state["experiment_hash"]
    classification = state["classification"]
    response = state["response"]

    if not response or response.result_type != "new_material":
        return {}

    material_id = response.material_id
    mat_hash = response.material_hash
    mat_name = response.name
    mat_rarity = response.rarity
    visual_status = response.visual_status
    fallback_icon = response.fallback_icon

    # Use is_new flag from state to avoid duplicate DB query
    is_new = state.get("is_new")
    if is_new:
        assert proposal.result is not None
        new_mat = GeneratedMaterialModel(
            id=material_id,
            material_hash=mat_hash,
            name=mat_name,
            category=proposal.result.category,
            rarity=mat_rarity,
            description=proposal.result.description,
            properties_json=proposal.result.properties.model_dump(),
            risks_json=proposal.result.risks,
            usage_json=proposal.result.usage,
            recipe_candidates_json=proposal.result.next_recipe_candidates,
            source_experiment_hash=exp_hash,
            visual_status=visual_status,
            visual_prompt=proposal.result.visual_prompt,
            fallback_icon=fallback_icon,
        )
        MaterialRegistryService.save_material(session, new_mat)

    new_exp = GeneratedExperimentModel(
        id=f"exp_{uuid.uuid4().hex[:10]}",
        experiment_hash=exp_hash,
        machine_type=request.machine_type,
        inputs_json=state["normalized_inputs"],
        normalized_inputs_json=state["normalized_inputs"],
        process_conditions_json=request.process_conditions.model_dump(),
        classification=classification,
        result_type="new_material",
        material_id=material_id,
        llm_used=True,
        llm_confidence=proposal.confidence,
        llm_model=get_proposal_generator().settings.default.model,
    )
    ExperimentRegistryService.save_experiment(session, new_exp)

    discovery_id = f"dsc_{uuid.uuid4().hex[:10]}"
    MaterialRegistryService.record_discovery(
        session, discovery_id, material_id, request.player_id, exp_hash
    )

    try:
        from agents.material_generation.events import MaterialEventPublisher

        assert proposal.result is not None
        MaterialEventPublisher.publish_material_created(
            material_id=material_id,
            visual_prompt=proposal.result.visual_prompt,
            category=proposal.result.category,
        )
    except Exception as exc:
        logger.error("Failed to trigger visual asset pipeline event: %s", exc)

    return {"response": response}


# Conditional routing functions
def check_cache_routing(state: MaterialGraphState) -> str:
    if state.get("response"):
        return END
    return "recipe_match"


def check_recipe_routing(state: MaterialGraphState) -> str:
    if state.get("response"):
        return END
    return "prevalidate"


def check_prevalidate_routing(state: MaterialGraphState) -> str:
    if state.get("response"):
        return END
    return "classify"


def check_handle_rule_routing(state: MaterialGraphState) -> str:
    if state.get("response"):
        return END
    return "similarity_context"


def check_validate_result_routing(state: MaterialGraphState) -> str:
    if state.get("response"):
        return END

    proposal = state.get("proposal")
    attempt = state.get("attempt", 1)

    if not proposal or proposal.proposal_type == "failed" or not proposal.result:
        if attempt <= 3:
            return "llm_propose"
        else:
            return END

    return "deduplicate_material"


def build_material_subgraph() -> StateGraph:
    """Compile and return the material generation StateGraph subgraph."""
    builder = StateGraph(MaterialGraphState)

    # Add nodes
    builder.add_node("normalize", normalize_node)
    builder.add_node("lookup_cache", lookup_cache_node)
    builder.add_node("recipe_match", recipe_match_node)
    builder.add_node("prevalidate", prevalidate_node)
    builder.add_node("classify", classify_node)
    builder.add_node("handle_rule", handle_rule_node)
    builder.add_node("similarity_context", similarity_context_node)
    builder.add_node("llm_propose", llm_propose_node)
    builder.add_node("validate_result", validate_result_node)
    builder.add_node("deduplicate_material", deduplicate_material_node)
    builder.add_node("register_material", register_material_node)

    # Set entry point
    builder.set_entry_point("normalize")

    # Add linear edges and conditional routing
    builder.add_edge("normalize", "lookup_cache")

    builder.add_conditional_edges(
        "lookup_cache",
        check_cache_routing,
        {
            END: END,
            "recipe_match": "recipe_match",
        },
    )

    builder.add_conditional_edges(
        "recipe_match",
        check_recipe_routing,
        {
            END: END,
            "prevalidate": "prevalidate",
        },
    )

    builder.add_conditional_edges(
        "prevalidate",
        check_prevalidate_routing,
        {
            END: END,
            "classify": "classify",
        },
    )

    builder.add_edge("classify", "handle_rule")

    builder.add_conditional_edges(
        "handle_rule",
        check_handle_rule_routing,
        {
            END: END,
            "similarity_context": "similarity_context",
        },
    )

    builder.add_edge("similarity_context", "llm_propose")
    builder.add_edge("llm_propose", "validate_result")

    builder.add_conditional_edges(
        "validate_result",
        check_validate_result_routing,
        {
            END: END,
            "llm_propose": "llm_propose",
            "deduplicate_material": "deduplicate_material",
        },
    )

    builder.add_edge("deduplicate_material", "register_material")
    builder.add_edge("register_material", END)

    return builder.compile()


# Export the compiled subgraph
material_subgraph = build_material_subgraph()
