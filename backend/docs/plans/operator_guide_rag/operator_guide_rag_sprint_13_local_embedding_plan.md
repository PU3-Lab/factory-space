# operator_guide RAG Sprint 13 기획서 (Local Embedding Provider)

## 1. 개요
현재 공장 매뉴얼 RAG 검색은 기본적으로 OpenAI가 제공하는 `text-embedding-3-small` 임베딩 API를 사용하고 있습니다. 하지만 인터넷 연결이 불가능한 온프레미스(On-premises) 환경이나 로컬 개발 환경(예: Ollama, LM Studio)에서도 RAG 기능을 원활하게 수행할 수 있어야 합니다.

따라서 Sprint 13에서는 로컬 임베딩을 지원하는 `LocalEmbeddingProvider`를 추가하고, 환경변수 설정에 따라 OpenAI와 로컬(Ollama / OpenAI 호환 로컬 엔드포인트) 임베딩 공급자를 동적으로 전환할 수 있도록 구조를 확장합니다.

## 2. 목표
- `rag_embedding.py`에 로컬 전용 임베딩 프로바이더(`LocalEmbeddingProvider`)를 추가합니다.
- `EmbeddingProvider` 프로토콜을 그대로 준수하여, 기존 모듈(Ingestion, Retriever)과의 호환성을 유지합니다.
- 다중 엔드포인트 지원:
  - OpenAI 호환 로컬 API (예: LM Studio, LocalAI, Ollama의 OpenAI 호환 API `/v1/embeddings`)
  - Ollama 네이티브 API (예: `http://localhost:11434/api/embeddings`)
- 로컬 임베딩 설정 매핑 및 실패 처리(로그 출력 후 빈 리스트 반환)를 구현합니다.

---

## 3. 상세 기획 및 구현 명세

### 3.1. 환경변수 및 EmbeddingSettings 연동 ([rag_embedding.py](file:///c:/factory-space/backend/src/agents/operator_guide/rag_embedding.py))
- `EmbeddingProviderName` 리터럴 타입에 `"local"`을 추가합니다.
- `_EMBEDDING_PROVIDERS` 세트에 `"local"`을 추가합니다.
- `EmbeddingSettings.from_env` 메소드에서 `FACTORY_EMBEDDING_PROVIDER`가 `"local"`인 경우 다음 환경변수를 바탕으로 설정을 생성합니다:
  - `FACTORY_EMBEDDING_MODEL` (기본값: `"nomic-embed-text"`)
  - `FACTORY_EMBEDDING_DIMENSIONS` (기본값: `768`)
  - `FACTORY_EMBEDDING_BASE_URL` (기본값: `"http://localhost:11434"`)
  - `FACTORY_EMBEDDING_API_KEY` 또는 `OPENAI_API_KEY` (기본값: `"noop"`)

### 3.2. LocalEmbeddingProvider 클래스 구현 ([rag_embedding.py](file:///c:/factory-space/backend/src/agents/operator_guide/rag_embedding.py))
- `LocalEmbeddingProvider` 클래스를 추가합니다.
- `embed_texts(self, texts: list[str]) -> list[list[float]]` 메소드를 구현합니다.
- **OpenAI 호환 모드**:
  - `settings.base_url`을 공백 및 후행 슬래시를 제거한 뒤 `rstrip("/")`하여 분석합니다.
  - 만약 `base_url`이 `/v1`로 끝나면 `OpenAI` 호환 클라이언트(`_create_openai_client`)를 사용하여 기존 OpenAI처럼 한 번에 여러 텍스트의 임베딩을 얻어옵니다.
- **Ollama 네이티브 모드**:
  - `base_url`이 `/v1`로 끝나지 않으면 파이썬 표준 라이브러리인 `urllib.request`를 사용합니다.
  - `{base_url}/api/embeddings` POST API에 `{"model": settings.model, "prompt": text}`를 본문으로 전송합니다.
  - `texts` 리스트의 각 텍스트에 대해 루프를 돌며 API 요청을 처리합니다.
  - 런타임 에러(네트워크 단절, 타임아웃, 올바르지 않은 모델 등) 발생 시 에러 상세 로그를 경고(`logger.warning`) 레벨로 기록하고, `[]`를 반환하여 에러가 전체 파이프라인으로 전파되지 않도록 방지합니다.

### 3.3. 프로바이더 생성 팩토리 함수 수정 ([rag_embedding.py](file:///c:/factory-space/backend/src/agents/operator_guide/rag_embedding.py))
- `create_embedding_provider` 함수를 수정하여 `resolved_settings.provider == "local"` 일 때 `LocalEmbeddingProvider` 인스턴스를 빌드해 반환합니다.

---

## 4. 검증 계획

### 4.1. 유닛 테스트 작성 ([test_operator_guide_rag_embedding.py](file:///c:/factory-space/backend/tests/test_operator_guide_rag_embedding.py))
- `test_embedding_settings_from_env_uses_local_defaults`: 로컬 프로바이더 선택 시 기본값(nomic-embed-text, 768 등)이 잘 설정되는지 테스트합니다.
- `test_local_embedding_provider_openai_compatible_mode`: `base_url`이 `/v1`로 끝날 때 OpenAI 클라이언트를 통해 모킹된 임베딩 응답이 제대로 획득되는지 테스트합니다.
- `test_local_embedding_provider_ollama_native_mode`: `base_url`이 `/v1`로 끝나지 않을 때 `urllib.request.urlopen`을 모킹하여 Ollama 네이티브 API로 정상적인 임베딩 요청과 응답을 수행하는지 테스트합니다.
- `test_local_embedding_provider_ollama_native_failure`: API 요청이 실패(HTTP 404, 500 또는 Connection Refused 등)할 때 예외를 안전하게 catch하고 빈 리스트 `[]`를 반환하는지 테스트합니다.

### 4.2. 자동화 테스트 수행
```powershell
uv run pytest tests/test_operator_guide_rag_embedding.py -v
uv run pytest -q
uv run ruff check
```
