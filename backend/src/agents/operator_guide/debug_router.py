"""Debug endpoint for manual RAG search in operator_guide domain.

초보자용 설명:
    이 파일은 LLM의 답변 생성(Answer Generation) 과정 없이,
    입력한 플레이어 질문이 pgvector DB(벡터 데이터베이스)에서 어떻게 매칭되는지
    RAG(검색 증강 생성) 검색 결과와 신뢰도(Confidence) 점수만 빠르게 진단하는 전역 디버그 API입니다.
"""

from __future__ import annotations

import os

from fastapi import APIRouter, HTTPException, status
from pydantic import BaseModel, Field

from agents.operator_guide.service import ManualQAService

router = APIRouter(prefix="/debug/manual-rag", tags=["debug-manual-rag"])


class DebugSearchRequest(BaseModel):
    """RAG 디버그 검색 요청 스키마.

    초보자용 설명:
        플레이어가 던질 질문 문장과, 몇 개의 유사 문서를 가져올지(top_k)를 설정합니다.
    """

    question: str = Field(..., description="검색할 질문 문장")
    top_k: int | None = Field(None, description="가장 유사한 문서 검색 개수")


class DebugSearchItem(BaseModel):
    """검색된 단일 매뉴얼 문서 항목 스키마.

    초보자용 설명:
        pgvector 유사도 검색을 통해 찾아낸 개별 문서의 ID, 제목, 본문 내용 및 유사성 점수(score)입니다.
    """

    doc_id: str
    title: str
    content: str
    source_file: str
    source_row_id: str
    score: float


class DebugSubQuestionResult(BaseModel):
    """분해된 하위 질문(Sub-question)별 검색 결과 스키마.

    초보자용 설명:
        질문 분해기(Decomposer)에 의해 쪼개진 개별 질문과 그 질문에 대해 RAG 검색을 수행한 결과들의 목록입니다.
    """

    index: int
    question: str
    confidence: str
    results: list[DebugSearchItem]


class DebugSearchResponse(BaseModel):
    """RAG 디버그 검색 응답 스키마.

    초보자용 설명:
        전체 검색 결과 보고서입니다. 다중 질문 여부와 하위 질문별 상세 매칭 내역을 한눈에 제공합니다.
    """

    query: str
    is_multi_question: bool
    confidence: str
    sub_questions: list[DebugSubQuestionResult]


@router.post("/search", response_model=DebugSearchResponse)
async def debug_rag_search(request: DebugSearchRequest) -> DebugSearchResponse:
    """LLM 호출 없이 pgvector RAG 검색 결과만을 확인하는 디버그 API입니다.

    동작 흐름:
        1. FACTORY_RAG_DEBUG_ENABLED 환경변수가 'true'인지 검증합니다. (아니면 403 Forbidden)
        2. ManualQAService에 주입된 글로벌 RAG runtime을 가져와 검색을 실행합니다.
        3. 분해된 하위 질문(Sub-question)별로 pgvector 매칭 리스트를 정리해 반환합니다.
    """

    # 1. 디버그 플래그 검증
    debug_enabled = os.environ.get("FACTORY_RAG_DEBUG_ENABLED", "").lower() == "true"
    if not debug_enabled:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="RAG debug endpoint is disabled.",
        )

    # 2. RAG runtime 유효성 체크
    service = ManualQAService()
    if service._rag_runtime is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="RAG runtime is not active (CSV-only mode).",
        )

    # 3. RAG 검색 실행 (분해 및 매칭)
    try:
        rag_result = service._rag_runtime.retrieve(request.question)
    except Exception as exc:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"RAG search execution failed: {exc}",
        )

    # 4. 하위 질문별 매칭 결과 파싱 및 top_k 슬라이싱
    sub_questions_list = []
    if hasattr(rag_result, "sub_question_results"):
        for sub_res in rag_result.sub_question_results:
            results_items = []
            for r in sub_res.retrieval.results:
                results_items.append(
                    DebugSearchItem(
                        doc_id=r.doc_id,
                        title=r.title,
                        content=r.content,
                        source_file=r.source_file,
                        source_row_id=r.source_row_id,
                        score=r.score,
                    )
                )

            # top_k 필터가 주어졌다면 슬라이싱
            if request.top_k is not None:
                results_items = results_items[: request.top_k]

            sub_questions_list.append(
                DebugSubQuestionResult(
                    index=sub_res.index,
                    question=sub_res.question,
                    confidence=sub_res.retrieval.confidence,
                    results=results_items,
                )
            )

    # 5. 대표 신뢰도(Confidence) 도출
    confidence = "low"
    if hasattr(rag_result, "metadata") and "confidence_counts" in rag_result.metadata:
        counts = rag_result.metadata["confidence_counts"]
        # 하나라도 low 매칭이면 안전을 위해 전체 low로 판정
        if counts.get("low", 0) > 0:
            confidence = "low"
        elif counts.get("medium", 0) > 0:
            confidence = "medium"
        else:
            confidence = "high"

    return DebugSearchResponse(
        query=request.question,
        is_multi_question=getattr(rag_result, "is_multi_question", False),
        confidence=confidence,
        sub_questions=sub_questions_list,
    )
