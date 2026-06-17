# 코드 리뷰: operator_guide RAG Sprint 13 (Local Embedding Provider)

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `feature/operator-guide-rag-runtime-docs` |
| 리뷰 일자 | 2026-06-16 |
| 리뷰 범위 | 로컬 LLM 환경 임베딩 연동 지원 및 LocalEmbeddingProvider 구현 |
| 리뷰어 | kimkyungpyo |

## 1. 변경 요약

- **로컬 임베딩 설정 매핑**: [rag_embedding.py](file:///c:/factory-space/backend/src/agents/operator_guide/rag_embedding.py)의 `EmbeddingSettings.from_env` 메소드에서 `FACTORY_EMBEDDING_PROVIDER`가 `"local"`인 경우, 로컬 LLM 환경(Ollama 등)에 적절한 기본값(`nomic-embed-text` 모델, `768` 차원, `http://localhost:11434` 주소 등)을 할당해 주도록 확장하였습니다.
- **다중 엔드포인트 지원 (LocalEmbeddingProvider)**:
  - **OpenAI 호환 모드**: `base_url`이 `/v1`로 끝날 때 기존 OpenAI 클라이언트를 사용하여 일괄(Batch) 텍스트 임베딩을 요청합니다.
  - **Ollama 네이티브 모드**: `base_url`이 `/v1`로 끝나지 않는 경우, 파이썬 표준 라이브러리인 `urllib.request`를 사용하여 `{base_url}/api/embeddings` POST endpoint로 개별 임베딩을 요청합니다.
- **에러 격리 (Safety Net)**: 로컬 API 통신 실패, 모델 누락 등으로 예외가 발생하는 경우 경고 로그(`logger.warning`)를 출력하고 빈 리스트 `[]`를 반환하여 에러가 전체 RAG 파이프라인으로 전파되지 않도록 조치하였습니다.
- **유닛 테스트 추가**: [test_operator_guide_rag_embedding.py](file:///c:/factory-space/backend/tests/test_operator_guide_rag_embedding.py)에 로컬 설정 매핑 검증, OpenAI 호환 로컬 클라이언트 모킹 테스트, Ollama 네이티브 API 호출 모킹 테스트 및 예외 포착 테스트를 추가하였습니다.

---

## 2. 검증 결과

### 2.1. 자동화 테스트 결과
총 245개의 백엔드 전체 테스트 케이스가 성공적으로 통과하였습니다.
- `uv run pytest tests/test_operator_guide_rag_embedding.py -v` 통과.
- `uv run pytest -q` 전체 백엔드 테스트 suite 통과.
- `uv run ruff check` 코드 포맷 및 린트 검사 통과.

### 2.2. 테스트 결과 출력 전문
```text
tests/test_operator_guide_rag_embedding.py::test_embedding_settings_from_env_uses_openai_defaults PASSED [  9%]
tests/test_operator_guide_rag_embedding.py::test_embedding_settings_prefers_slot_api_key PASSED [ 18%]
tests/test_operator_guide_rag_embedding.py::test_embedding_settings_rejects_unsupported_provider PASSED [ 27%]
tests/test_operator_guide_rag_embedding.py::test_openai_embedding_provider_sends_expected_payload PASSED [ 36%]
tests/test_operator_guide_rag_embedding.py::test_openai_embedding_provider_returns_empty_without_api_key PASSED [ 45%]
tests/test_operator_guide_rag_embedding.py::test_create_embedding_provider_returns_noop_for_none_provider PASSED [ 54%]
tests/test_operator_guide_rag_embedding.py::test_embedding_settings_from_env_uses_local_defaults PASSED [ 63%]
tests/test_operator_guide_rag_embedding.py::test_local_embedding_provider_openai_compatible_mode PASSED [ 72%]
tests/test_operator_guide_rag_embedding.py::test_local_embedding_provider_ollama_native_mode PASSED [ 81%]
tests/test_operator_guide_rag_embedding.py::test_local_embedding_provider_ollama_native_failure PASSED [ 90%]
tests/test_operator_guide_rag_embedding.py::test_create_embedding_provider_returns_local_provider PASSED [100%]

============================= 11 passed in 0.49s ==============================
```

---

## 3. 종합 평가

이번 Sprint 13 작업을 통해, 인터넷이 단절된 프라이빗 온프레미스 인프라에서도 로컬 AI 모델 서버(Ollama, LM Studio 등)를 연동하여 공장 매뉴얼 RAG 검색을 완전하게 수행할 수 있는 유연성을 확보하였습니다.
특히, 표준 라이브러리(`urllib.request`)와 OpenAI 클라이언트를 적절히 혼용하여 다형성을 지원하면서도, 기존의 검색 인터페이스 규격을 완벽하게 충족해 기존 컴포넌트의 결합도를 낮췄습니다.
전체 테스트 스위트 통과 및 린터 검증이 모두 안정적으로 확보되었으므로, 본 변경 사항의 프로덕션 반영을 승인합니다.
