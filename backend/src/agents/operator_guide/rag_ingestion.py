"""RAG 문서를 embedding 저장소에 넣기 전 payload로 준비하는 모듈.

초보자용 설명:
    `rag_documents.py`가 CSV를 문서로 만들었다면, 이 파일은 그 문서를 DB에 넣을 수 있게
    content hash와 embedding vector를 붙인다.
"""

from __future__ import annotations

import hashlib
from dataclasses import dataclass
from typing import Protocol

from agents.operator_guide.rag_documents import ManualRagDocument


class EmbeddingProvider(Protocol):
    """텍스트를 embedding vector로 바꾸는 객체의 최소 약속."""

    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        """입력 텍스트마다 embedding vector 하나를 반환한다."""


@dataclass(frozen=True)
class ManualRagIngestionRecord:
    """DB에 저장할 RAG 문서 payload 한 개.

    문서 내용, 출처, content hash, embedding vector가 모두 포함된다.
    """

    doc_id: str
    source_file: str
    source_row_id: str
    title: str
    embedding_text: str
    content_hash: str
    metadata: dict[str, str]
    embedding: list[float]


@dataclass(frozen=True)
class ManualRagIngestionBatch:
    """한 번의 ingestion 실행에서 사용할 문서 묶음."""

    records: list[ManualRagIngestionRecord]
    content_hashes: dict[str, str]


class ManualRagIngestionService:
    """RAG 문서를 DB 저장 가능한 ingestion record로 변환한다."""

    def __init__(self, embedding_provider: EmbeddingProvider) -> None:
        self._embedding_provider = embedding_provider

    def build_records(
        self,
        documents: list[ManualRagDocument],
    ) -> list[ManualRagIngestionRecord]:
        """주어진 문서 전체를 embedding까지 포함한 record로 만든다."""

        return self._build_records_for_documents(documents)

    def build_batch(
        self,
        documents: list[ManualRagDocument],
        *,
        existing_content_hashes: dict[str, str],
        dry_run: bool = False,
        force: bool = False,
    ) -> ManualRagIngestionBatch:
        """변경된 문서만 embedding하도록 ingestion batch를 만든다.

        `dry_run=True`이면 실제 embedding API를 호출하지 않고, 어떤 문서가 바뀌었는지만
        계산한다.
        """

        content_hashes = {
            document.doc_id: _content_hash(document.content) for document in documents
        }
        documents_to_embed = [
            document
            for document in documents
            if force
            or existing_content_hashes.get(document.doc_id) != content_hashes[document.doc_id]
        ]
        records = [] if dry_run else self._build_records_for_documents(documents_to_embed)
        return ManualRagIngestionBatch(records=records, content_hashes=content_hashes)

    def _build_records_for_documents(
        self,
        documents: list[ManualRagDocument],
    ) -> list[ManualRagIngestionRecord]:
        texts = [document.content for document in documents]
        embeddings = self._embedding_provider.embed_texts(texts)
        return [
            ManualRagIngestionRecord(
                doc_id=document.doc_id,
                source_file=document.source_file,
                source_row_id=document.source_row_id,
                title=document.title,
                embedding_text=document.content,
                content_hash=_content_hash(document.content),
                metadata=document.metadata,
                embedding=embedding,
            )
            for document, embedding in zip(documents, embeddings, strict=True)
        ]


def _content_hash(content: str) -> str:
    """문서 내용이 바뀌었는지 비교하기 위한 SHA-256 hash를 만든다."""

    return hashlib.sha256(content.encode("utf-8")).hexdigest()
