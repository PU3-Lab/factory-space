"""재료 생성 서브그래프를 위한 LangGraph 노드 및 라우팅 함수 정의입니다."""

from __future__ import annotations

import logging
import uuid
from typing import Any

from sqlalchemy import event, select
from sqlalchemy.orm import Session

from agents.material_generation.classifier import ExperimentClassifier
from agents.material_generation.derivation import (
    DerivedAttributes,
    derive_material_attributes,
)
from agents.material_generation.events import MaterialEventPublisher
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
    MaterialProposal,
    OutputItemSchema,
)
from agents.material_generation.similarity import ExperimentSimilarityService
from db.models import GeneratedExperimentModel, GeneratedMaterialModel

logger = logging.getLogger(__name__)

_proposal_generator = None


def get_proposal_generator() -> MaterialProposalGenerator:
    """제안 생성기(Proposal Generator)의 지연 초기화(Lazy initialization)입니다."""
    global _proposal_generator
    if _proposal_generator is None:
        _proposal_generator = MaterialProposalGenerator()
    return _proposal_generator


def normalize_node(state: MaterialGraphState) -> dict[str, Any]:
    """노드: 입력 아이템을 정규화하고 실험 해시를 계산합니다."""
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
    """노드: 캐시된 결과가 있는지 실험 레지스트리를 확인합니다."""
    session = state["db"]
    exp_hash = state["experiment_hash"]

    existing_exp = ExperimentRegistryService.get_experiment_by_hash(session, exp_hash)
    if existing_exp:
        logger.info("Found cached experiment for hash: %s", exp_hash)
        if existing_exp.result_type == "new_material" and existing_exp.material_id:
            stmt = select(GeneratedMaterialModel).where(
                GeneratedMaterialModel.id == existing_exp.material_id
            )
            mat_model = session.execute(stmt).scalar_one_or_none()

            mat_name = mat_model.name if mat_model else "Unknown Alloy"
            mat_hash = mat_model.material_hash if mat_model else None
            mat_rarity = mat_model.rarity if mat_model else None
            mat_state = mat_model.state if mat_model else None
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
                state=mat_state,
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
    """노드: 시스템에 작성된 레시피를 검색합니다."""
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
    """노드: 결정론적 검증 검사를 수행합니다."""
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
    """노드: 실험 구성을 분류합니다."""
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
    """노드: 합성이 아닌 카테고리에 대해 빠르게 실패(Fast-fail) 처리를 수행합니다."""
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
    """노드: 컨텍스트 분석을 위해 유사한 실험 이력을 검색합니다."""
    if state.get("response"):
        return {}

    session = state["db"]
    request = state["request"]
    normalized = state["normalized_inputs"]

    similar_exps = ExperimentSimilarityService.find_similar_experiments(
        session, request.machine_type, normalized
    )
    return {"similar_context": similar_exps}


def _apply_derived(proposal: MaterialProposal, derived: DerivedAttributes) -> None:
    """LLM 제안의 구조화 값을 결정론 산출값으로 덮어씁니다 (이름·설명은 유지)."""
    if proposal.result is None:
        return
    proposal.result.properties = derived.properties
    proposal.result.category = derived.category
    proposal.result.state = derived.state
    proposal.result.rarity = derived.rarity


def derive_node(state: MaterialGraphState) -> dict[str, Any]:
    """노드: 입력·공정으로부터 속성·category·state·rarity를 결정론적으로 산출합니다."""
    if state.get("response"):
        return {}
    request = state["request"]
    normalized = state["normalized_inputs"]
    derived = derive_material_attributes(normalized, request.process_conditions)
    return {"derived": derived}


def llm_propose_node(state: MaterialGraphState) -> dict[str, Any]:
    """노드: LLM으로 이름·설명을 제안받고 구조화 값을 derived로 덮어씁니다."""
    if state.get("response"):
        return {}

    request = state["request"]
    normalized = state["normalized_inputs"]
    similar_exps = state.get("similar_context") or []
    derived = state["derived"]
    assert derived is not None

    proposal = get_proposal_generator().generate_proposal(
        request.machine_type,
        normalized,
        request.process_conditions,
        similar_exps,
        derived_state=derived.state,
        derived_category=derived.category,
    )
    _apply_derived(proposal, derived)
    return {"proposal": proposal}


def validate_result_node(state: MaterialGraphState) -> dict[str, Any]:
    """노드: 규칙 강제 적용, 새니타이징 및 재시도 상태를 관리합니다."""
    if state.get("response"):
        return {}

    proposal = state["proposal"]
    attempt = state["attempt"]

    if not proposal:
        proposal = get_proposal_generator().get_fallback_proposal(
            state["normalized_inputs"]
        )
        derived = state.get("derived")
        if derived is not None:
            _apply_derived(proposal, derived)

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
    """노드: 기존 재료 해시를 확인하여 속성을 중복 제거합니다.

    참고: material_hash가 (장비+정규화 입력+공정조건) 합성 정체성 기반으로 재정의되었기 때문에,
    동일 합성 조건은 1차로 상위 `lookup_cache_node`(experiment_hash 캐시)에서 이미 걸러집니다.
    따라서 이 노드의 `existing_mat` 매칭 분기는 주로 캐시 테이블(experiments)에 기록은 안 남아있으나
    materials 테이블에는 존재하거나, 다른 장비/공정에서 같은 정체성을 유도하는 등의 드문 경로에 대한
    안전장치(fallback) 성격으로 잔존 및 작동합니다.
    """
    if state.get("response"):
        return {}

    session = state["db"]
    proposal = state["proposal"]
    exp_hash = state["experiment_hash"]
    request = state["request"]

    assert proposal.result is not None
    mat_hash = generate_material_hash(
        request.machine_type, state["normalized_inputs"], request.process_conditions
    )
    existing_mat = MaterialRegistryService.get_material_by_hash(session, mat_hash)

    generate_visual = request.generate_visual_asset

    if existing_mat:
        material_id = existing_mat.id
        mat_name = existing_mat.name
        mat_rarity = existing_mat.rarity
        mat_state = existing_mat.state
        visual_status = existing_mat.visual_status
        fallback_icon = existing_mat.fallback_icon
        is_new = False
    else:
        material_id = f"mat_{proposal.result.id_hint}_{uuid.uuid4().hex[:6]}"
        mat_name = proposal.result.name
        mat_rarity = proposal.result.rarity
        mat_state = proposal.result.state
        visual_status = "pending" if generate_visual else "skipped"
        fallback_icon = f"materials/default/{proposal.result.category}.png"
        is_new = True

    response = MaterialCreationResponse(
        result_type="new_material",
        experiment_hash=exp_hash,
        material_id=material_id,
        material_hash=mat_hash,
        name=mat_name,
        rarity=mat_rarity,
        state=mat_state,
        generation_status="created" if is_new else "cached",
        visual_status=visual_status,
        fallback_icon=fallback_icon,
        message="새로운 물질이 발견되었습니다. 아이콘과 텍스처는 생성 중입니다."
        if (is_new and generate_visual)
        else "새로운 물질이 발견되었습니다."
        if (is_new and not generate_visual)
        else "이미 발견된 물질입니다.",
    )
    return {"response": response, "is_new": is_new}


def register_material_node(state: MaterialGraphState) -> dict[str, Any]:
    """노드: 새로운 재료 및 발견 사항을 커밋하고, 자산 파이프라인 이벤트를 게시합니다."""
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

    is_new = state.get("is_new")
    if is_new:
        assert proposal.result is not None
        new_mat = GeneratedMaterialModel(
            id=material_id,
            material_hash=mat_hash,
            name=mat_name,
            category=proposal.result.category,
            state=proposal.result.state,
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

    if request.generate_visual_asset and is_new:
        assert proposal.result is not None
        v_prompt = proposal.result.visual_prompt
        cat = proposal.result.category

        def trigger_visual_pipeline(session_obj: Session) -> None:
            try:
                MaterialEventPublisher.publish_material_created(
                    material_id=material_id,
                    visual_prompt=v_prompt,
                    category=cat,
                )
            except Exception as exc:
                logger.error("Failed to trigger visual asset pipeline event: %s", exc)

        event.listen(session, "after_commit", trigger_visual_pipeline, once=True)

    return {"response": response}
