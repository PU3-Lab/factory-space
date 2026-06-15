# 워크스루 - 물질 이름·희귀도·속성·상태 결정론화 구현 완료

신물질 합성 과정에서 랜덤성을 제거하고, 입력 재료 구성 및 공정 환경에 따라 물리적 특성, 분류, 상태, 등급이 수학적으로 일정하게 도출되도록 결정론화하는 작업을 성공적으로 완료했습니다.

## 주요 변경 사항

### 1. 결정론적 속성 산출 핵심 모듈 추가
- [material_properties.py](file:///Users/kimkyungpyo/Workspaces/projects/factory-space/backend/src/agents/material_generation/material_properties.py)
  - 기초 재료 고유의 물리 스펙트럼(강도, 전도도, 안정성, 반응성) 및 금속/유기물 분류 정보를 정의합니다.
  - 아이템 식별자에서 형태 접미사(`_ingot`, `_powder` 등)를 자동 정규화합니다.
- [derivation.py](file:///Users/kimkyungpyo/Workspaces/projects/factory-space/backend/src/agents/material_generation/derivation.py)
  - **속성 합성**: 입력 재료들의 수량을 가중치로 가중 평균을 구합니다.
  - **공정 보정**: 온도, 압력, 촉매 유무에 따라 스펙트럼을 정교하게 제어 및 보정합니다.
  - **카테고리 판정**: 금속 혼합율 및 유기물 소속 비율에 따라 카테고리(`alloy`, `organic`, `chemical`, `composite`)를 논리 분류합니다.
  - **물리적 상태 판정**: 반응성 및 안정성 점수에 따라 상태(`solid`, `liquid`, `gas`, `plasma`)를 판정합니다.
  - **희귀도 공식**: 가중 평균 속성(70%)과 최대 강세 속성(30%)을 합성해 등급(`common`, `uncommon`, `rare`, `epic`)을 연산합니다.

### 2. Pydantic 스키마 및 DB 모델 확장
- [schemas.py](file:///Users/kimkyungpyo/Workspaces/projects/factory-space/backend/src/agents/material_generation/schemas.py): `MaterialProposalResult`와 `MaterialCreationResponse` 스키마에 물리 상태(`state`) 필드를 추가했습니다.
- [models.py](file:///Users/kimkyungpyo/Workspaces/projects/factory-space/backend/src/db/models.py): 생성 물질 엔티티인 `GeneratedMaterialModel`에 `state` 컬럼을 반영했습니다.
- [0003_add_material_state.py](file:///Users/kimkyungpyo/Workspaces/projects/factory-space/backend/migrations/versions/0003_add_material_state.py): `state` 컬럼 추가를 지원하는 데이터베이스 마이그레이션 스크립트를 작성하여 Alembic으로 적용 완료했습니다.

### 3. 해싱 정체성 재정의 및 LLM 역할 축소
- [normalizer.py](file:///Users/kimkyungpyo/Workspaces/projects/factory-space/backend/src/agents/material_generation/normalizer.py): `material_hash`를 (합성 장비 + 정규화 입력 + 공정 조건) 기반의 결정론적 합성 해시로 재조정했습니다.
- [result_validator.py](file:///Users/kimkyungpyo/Workspaces/projects/factory-space/backend/src/agents/material_generation/result_validator.py): LLM 검증기의 책임을 이름 및 ID 힌트 정화, 개수 상한 필터링으로 축소하였습니다.
- [proposal_generator.py](file:///Users/kimkyungpyo/Workspaces/projects/factory-space/backend/src/agents/material_generation/proposal_generator.py): 프롬프트에 상태별 명명 가이드(예: 고체는 `~정`, `~합금`, 액체는 `~용액`, `~액`)를 반영하고, LLM 실패 시 기본 fallback 물질에도 `state`를 영속화하였습니다.

### 4. LangGraph 파이프라인 배선 완료
- [graph.py](file:///Users/kimkyungpyo/Workspaces/projects/factory-space/backend/src/agents/material_generation/graph.py) & [nodes.py](file:///Users/kimkyungpyo/Workspaces/projects/factory-space/backend/src/agents/material_generation/nodes.py)
  - `similarity_context` 노드와 `llm_propose` 노드 사이에 신규 `derive` 노드를 조립했습니다.
  - LLM 제안 직후 LLM이 생성한 임의의 구조화 값들을 사전에 결정론적으로 연산한 `derived` 결과값으로 override하도록 제어했습니다.

### 5. 설계/계획 리뷰 의견 반영 및 조치 완료
- **M1 (plasma 상태 도달 조건 완화)**: plasma 판정 기준을 `reactivity >= 9.0 and stability <= 2.0`으로 현실적으로 완화하여, sulfur 극단 합성 조합 시 plasma 상태 도출이 가능하도록 수정했습니다.
- **M2 (dedup 분기 설명 주석 보강)**: `deduplicate_material_node` 내부의 `existing_mat` 분기에 캐싱 관련 안전장치 주석을 상세화했습니다.
- **m1 (LLM 프롬프트 슬림화)**: 백엔드에서 덮어쓸 구조화 필드(`properties`, `category`, `rarity`)에 대해 임의의 더미값(예: 5.0 등)을 채워 반환하도록 프롬프트 가이드를 추가하여 LLM의 연산 혼선을 방지했습니다.
- **m2 (금지어 단어 경계 필터링)**: 금지 키워드 치환 시 부분 매칭 오인 치환을 막기 위해 단어 경계(`\b`) 패턴을 적용하고 관련 단위 테스트를 추가했습니다.

---

## 검증 내용

1. **신규 단위 및 통합 테스트 작성 및 패스**
   - `test_material_properties.py`: 형태 접미사 분리 및 기준 속성 조회 검증 완료 (4개 통과)
   - `test_derivation.py`: 가중 평균, 공정 환경 보정, 카테고리/상태/희귀도 연산 검증 완료 (21개 통과)
   - `test_schemas_state.py`: Pydantic 스키마의 `state` 필드 명세 및 기본값 검증 완료 (2개 통과)
   - `test_material_state_column.py`: DB 모델에 컬럼 정상 매핑 여부 검증 완료 (1개 통과)
   - `test_normalizer.py`: 합성 조건 기반 고유 해시 검증 완료 (5개 통과)
   - `test_result_validator.py`: 이름/식별자 금지어 필터링 및 트림 검증 완료 (7개 통과)
   - `test_proposal_generator.py`: 프롬프트 내 가이드 및 fallback 검증 완료 (2개 통과)
   - `test_agent.py`: LangGraph 파이프라인 통합 테스트 및 fallback 시나리오 검증 완료 (10개 통과)
   - `material_generation` 서브 모듈 내 전체 62개 테스트 케이스 100% 통과 완료.

2. **백엔드 전체 회귀 테스트 검증**
   - 백엔드에 작성된 모든 263개의 테스트 케이스를 통과하여, 기존의 다른 시스템 기능에 영향을 주지 않음을 확증했습니다.

3. **아키텍처 문서 동기화**
   - [material_generation_current_structure.md](file:///Users/kimkyungpyo/Workspaces/projects/factory-space/docs/03_architecture/material_generation_current_structure.md) 문서에 신규 추가된 `derive` 노드 정보, 마이그레이션 컬럼, 그리고 해시 정체성 메커니즘을 업데이트 및 기록하였습니다.

### 6. 아이콘 및 언리얼 머터리얼 텍스처 분리 생성 파이프라인 도입
- **이중 마스터 이미지 생성**:
  - 인벤토리 등 UI용으로 적합한 사물 중심의 **아이콘(Icon)**과 언리얼 에디터에서 반복해서 바인딩해 쓸 수 있는 평면 이음새 없는 **텍스처(Texture)**의 요구사항 충족을 위해 DALL-E/OpenAI API를 각각의 전용 프롬프트로 2회 호출하도록 설계했습니다.
- **최적화 및 Fail-Fast**:
  - `visual/adapter.py`의 `resize_to_profile` 함수에 short-circuit을 추가하여 해상도/포맷이 타겟 프로필과 정확히 일치할 때(예: `MASTER` -> `TEXTURE`) 불필요한 Pillow 디코드/리사이즈 연산을 생략합니다.
  - 잘못된 저장소 백엔드(`FACTORY_IMAGE_STORAGE_BACKEND`) 설정 시 조용히 데이터가 버려지지 않고 즉각 `ValueError`를 발생시키며(Fail-fast), 영속 저장 경로의 기본값은 기존 SQLite 관례를 따라 `backend/var/assets`로 고정하고 `.gitignore` 처리하여 개발 편의성과 데이터 보존을 극대화했습니다.
- **최종 검증 완료**:
  - 이중 이미지 생성과 부분 실패(아이콘 또는 텍스처 실패)에 대응하는 단위 테스트 2개를 추가하여 백엔드 전체 테스트(**298개**)를 모두 통과시켰습니다.
  - 실제 API 환경 변수 연동 하에서 이중 마스터를 생성하는 파이프라인 검증 스크립트(`var/smoke_test_actual_pipeline.py`)를 가동하여, `icon.png` (367KB)와 `texture.png` (1.66MB, 원본 마스터 단락 저장)이 용도에 적합한 물리적 파일로 정확히 나뉘어 저장되는 것을 증명하였습니다.

