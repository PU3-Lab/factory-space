# Task 1.1 RED: cache hit metadata 보존

목표:

- cache miss에서 생성된 `responseMetadata`를 cache entry에 함께 저장한다.
- cache hit 응답은 기존 metadata를 유지하고 `cache: hit`만 추가한다.

RED 확인 명령:

```bash
uv run --extra dev pytest tests/test_pipeline_edges.py::test_pipeline_cache_hit_preserves_original_response_metadata -q
```

예상 실패:

- 현재 cache는 response payload만 저장한다.
- cache hit 경로의 `responseMetadata`가 `{"cache": "hit"}`로 새로 만들어져 원래 `llm: used` metadata가 사라진다.
