"""Prepare Manual Q&A RAG documents for embedding storage."""

from __future__ import annotations

import hashlib
from dataclasses import dataclass
from typing import Protocol

from agents.operator_guide.rag_documents import ManualRagDocument


class EmbeddingProvider(Protocol):
    """Provider contract for turning texts into embedding vectors."""

    def embed_texts(self, texts: list[str]) -> list[list[float]]:
        """Return one embedding vector per input text."""


@dataclass(frozen=True)
class ManualRagIngestionRecord:
    """One RAG document plus its embedding payload for storage."""

    doc_id: str
    source_file: str
    source_row_id: str
    title: str
    embedding_text: str
    content_hash: str
    metadata: dict[str, str]
    embedding: list[float]


class ManualRagIngestionService:
    """Build storage-ready ingestion records from normalized RAG documents."""

    def __init__(self, embedding_provider: EmbeddingProvider) -> None:
        self._embedding_provider = embedding_provider

    def build_records(
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
    return hashlib.sha256(content.encode("utf-8")).hexdigest()
