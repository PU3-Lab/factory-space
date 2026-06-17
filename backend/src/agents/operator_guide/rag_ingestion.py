"""RAG 문서를 embedding 저장소에 넣기 전 payload로 준비하는 모듈.

초보자용 설명:
    `rag_documents.py`가 CSV를 문서로 만들었다면, 이 파일은 그 문서를 DB에 넣을 수 있게
    content hash와 embedding vector를 붙인다.
"""

from __future__ import annotations

import hashlib
import logging
import subprocess
from dataclasses import dataclass, field
from pathlib import Path
from typing import Protocol

from agents.operator_guide.rag_documents import ManualRagDocument

logger = logging.getLogger(__name__)


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
class FailedIngestionRow:
    """임베딩이나 정규화 과정에서 실패한 개별 Row의 정보.

    초보자용 설명:
        Ingestion 수행 중 외부 API 오류나 네트워크 장애 등으로 임베딩 실패가 일어난 경우,
        프로세스를 멈추지 않고 어떤 파일의 어떤 Row가 실패했는지와 그 에러 원인을 임시 저장합니다.
    """

    doc_id: str
    source_file: str
    source_row_id: str
    title: str
    error_message: str


@dataclass(frozen=True)
class ManualRagIngestionBatch:
    """한 번의 ingestion 실행에서 사용할 문서 묶음."""

    records: list[ManualRagIngestionRecord]
    content_hashes: dict[str, str]
    failed_rows: list[FailedIngestionRow] = field(default_factory=list)


class ManualRagIngestionService:
    """RAG 문서를 DB 저장 가능한 ingestion record로 변환한다."""

    def __init__(self, embedding_provider: EmbeddingProvider) -> None:
        self._embedding_provider = embedding_provider

    def build_records(
        self,
        documents: list[ManualRagDocument],
    ) -> list[ManualRagIngestionRecord]:
        """주어진 문서 전체를 embedding까지 포함한 record로 만든다."""

        records, _ = self._build_records_for_documents(documents)
        return records

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
            or existing_content_hashes.get(document.doc_id)
            != content_hashes[document.doc_id]
        ]

        if dry_run:
            return ManualRagIngestionBatch(
                records=[],
                content_hashes=content_hashes,
                failed_rows=[],
            )

        records, failed_rows = self._build_records_for_documents(documents_to_embed)
        return ManualRagIngestionBatch(
            records=records,
            content_hashes=content_hashes,
            failed_rows=failed_rows,
        )

    def _build_records_for_documents(
        self,
        documents: list[ManualRagDocument],
    ) -> tuple[list[ManualRagIngestionRecord], list[FailedIngestionRow]]:
        """주어진 문서를 embedding API를 호출하여 record로 가공합니다.

        초보자용 설명:
            전체 문서를 한 번에 일괄 임베딩(Batch Call) 시도하고,
            만약 에러가 나거나 실패하면 각 문서를 하나씩 개별 호출(Partial Retry)하는 안전 처리를 수행합니다.
            성공한 문서만 Records로 반환하고 실패한 문서는 FailedIngestionRow로 분류해 돌려줍니다.
        """
        if not documents:
            return [], []

        texts = [document.content for document in documents]

        # 1차 시도: 일괄 호출
        try:
            embeddings = self._embedding_provider.embed_texts(texts)
            if embeddings and len(embeddings) == len(documents):
                records = [
                    ManualRagIngestionRecord(
                        doc_id=doc.doc_id,
                        source_file=doc.source_file,
                        source_row_id=doc.source_row_id,
                        title=doc.title,
                        embedding_text=doc.content,
                        content_hash=_content_hash(doc.content),
                        metadata=doc.metadata,
                        embedding=emb,
                    )
                    for doc, emb in zip(documents, embeddings, strict=True)
                ]
                return records, []
        except Exception as exc:
            logger.warning(
                "Batch embedding call failed, falling back to individual calls: %s",
                exc,
            )

        # 2차 시도: 개별 호출 & 리트라이
        records = []
        failed_rows = []
        for doc in documents:
            try:
                emb = self._embedding_provider.embed_texts([doc.content])
                if emb and len(emb) == 1:
                    records.append(
                        ManualRagIngestionRecord(
                            doc_id=doc.doc_id,
                            source_file=doc.source_file,
                            source_row_id=doc.source_row_id,
                            title=doc.title,
                            embedding_text=doc.content,
                            content_hash=_content_hash(doc.content),
                            metadata=doc.metadata,
                            embedding=emb[0],
                        )
                    )
                else:
                    failed_rows.append(
                        FailedIngestionRow(
                            doc_id=doc.doc_id,
                            source_file=doc.source_file,
                            source_row_id=doc.source_row_id,
                            title=doc.title,
                            error_message="Empty embedding returned from provider",
                        )
                    )
            except Exception as exc:
                failed_rows.append(
                    FailedIngestionRow(
                        doc_id=doc.doc_id,
                        source_file=doc.source_file,
                        source_row_id=doc.source_row_id,
                        title=doc.title,
                        error_message=str(exc),
                    )
                )

        return records, failed_rows


def calculate_source_version(data_dir: Path) -> str:
    """CSV 데이터 폴더의 Manifest 해시와 Git 커밋을 결합한 source_version을 구합니다.

    초보자용 설명:
        Ingestion에 사용되는 CSV 파일들의 내용과 Git 커밋 버전을 분석해 고유한 버전 해시를 만듭니다.
        Git이 없는 환경에서는 CSV Manifest 해시만으로 버전을 식별합니다.
    """
    manifest_hashes = []
    csv_files = sorted(data_dir.glob("*.csv"))
    for file in csv_files:
        try:
            content = file.read_bytes()
            manifest_hashes.append(hashlib.sha256(content).hexdigest())
        except Exception:
            pass

    combined_csv_hash = hashlib.sha256(
        "".join(manifest_hashes).encode("utf-8")
    ).hexdigest()[:12]

    git_commit = "unknown"
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=str(data_dir.parent),
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode == 0:
            git_commit = result.stdout.strip()
    except Exception:
        pass

    return f"csv-{combined_csv_hash}:git-{git_commit}"


def _content_hash(content: str) -> str:
    """문서 내용이 바뀌었는지 비교하기 위한 SHA-256 hash를 만든다."""

    return hashlib.sha256(content.encode("utf-8")).hexdigest()
