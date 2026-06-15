# 물질 이름·희귀도·상태·속성 기준 설계 (전면 결정론)

- 작성일: 2026-06-15
- 대상 에이전트: `material_generation`
- 관련 파일: `backend/src/agents/material_generation/*`

## 1. 배경 / 문제

현재 신물질 생성은 이름·희귀도·속성 4값을 **전적으로 LLM 자율 판단**에 의존한다.

- 희귀도가 LLM의 독립적 환각이라 표시 속성과 모순될 수 있다(예: 약한 속성인데 `epic`).
- 속성·희귀도·category·risks·usage가 모두 `material_hash`에 들어가는데(`normalizer.py:46`), LLM이 매번 다른 값을 내면 **같은 입력인데도 hash가 흔들려 중복 물질이 양산**된다.
- 이름에 명명 규칙이 없어 세계관 일관성이 떨어지고, 물질의 물리적 상태(고체/액체/기체)가 드러나지 않는다.

## 2. 목표 / 방향 (전면 결정론)

**같은 재료를 같은 장비·공정으로 합성하면 항상 같은 물질이 나온다**를 보장한다.

- 속성 4값을 **기초 재료 속성 테이블 + 조합 공식 + 공정 보정**으로 결정론 산출한다.
- 희귀도·category·state를 **속성/입력에서 결정론 산출**한다.
- `material_hash`를 **합성 정체성(machine + 정규화 입력 + 공정 조건)** 기반으로 재정의하여 완전 재현·중복 차단을 달성한다.
- 이름은 LLM이 **화학 물질·신소재처럼, 상태(고체/액체/기체)가 연상되도록** 짓되 규칙 검증한다. (이름·설명은 hash에 영향 없는 장식 메타데이터)

## 3. 의사결정 요약

| 항목 | 결정 |
|------|------|
| 속성 4값 | **결정론 공식** (재료 테이블 + 조합 + 공정 보정) |
| 희귀도 | **속성 파생 결정론 공식** |
| category | **입력 조합 기반 결정론 산출** |
| state | **속성 기반 결정론 산출** |
| material_hash | **합성 정체성 기반 재정의** |
| 이름 | **LLM 자율 + 화학 명명 가이드 + 규칙 검증** (hash 제외) |
| 설명/risks/usage | LLM 장식 메타데이터 (hash 제외) |

LLM의 역할은 **이름·설명·risks·usage·visual_prompt**로 축소된다. 구조화·식별 값은 모두 결정론이다.

## 4. 상세 설계

### 4.1 기초 재료 속성 테이블 (신규 데이터)

각 기초 재료의 기준 속성(0~10)을 정의한다. 실제 화학/물성에 느슨히 기반한 **제안 기본값**이며, 기획적으로 튜닝 가능하다. 저장 형식은 `RecipeTable.csv` 선례를 따라 CSV 또는 시드 DB 테이블로 한다.

| item | strength | conductivity | stability | reactivity |
|------|:--:|:--:|:--:|:--:|
| iron | 7 | 5 | 7 | 4 |
| copper | 4 | 9 | 6 | 4 |
| zinc | 3 | 5 | 5 | 6 |
| lead | 2 | 4 | 6 | 3 |
| tin | 3 | 5 | 6 | 3 |
| aluminum | 4 | 8 | 6 | 5 |
| nickel | 6 | 5 | 7 | 3 |
| tungsten | 10 | 4 | 9 | 1 |
| titanium | 8 | 3 | 9 | 2 |
| magnesium | 3 | 6 | 4 | 8 |
| gold | 2 | 10 | 10 | 1 |
| silver | 3 | 10 | 8 | 2 |
| charcoal | 1 | 2 | 4 | 6 |
| coal | 1 | 1 | 4 | 6 |
| sulfur | 1 | 1 | 3 | 8 |
| wood | 1 | 1 | 3 | 4 |

- 입력 item_id는 `iron_ingot` / `iron_powder` / `coal_dust` 등 접미사를 가진다. 테이블은 **기초 원소 키**(iron, copper, ...)로 매칭하고, 접미사(`_ingot`/`_powder`/`_dust` 등)는 정규화해 제거한다.
- (선택, v1 비범위) 형태 보정: powder/dust는 표면적이 커 `reactivity +1, strength -1` 등 보정 가능. v1에서는 적용하지 않는다.
- 테이블에 없는 미지 재료는 중립값(모두 5.0)으로 처리한다.

### 4.2 속성 조합 공식

입력 재료들의 기준 속성을 **수량 가중 평균**으로 합성한다.

```
prop = Σ(재료ᵢ.prop × qtyᵢ) / Σ(qtyᵢ)      (4속성 각각)
```

### 4.3 공정 보정

`process_conditions`의 문자열 값에 따라 가산 후 0~10 클램핑한다. 알 수 없는 값(`"default"` 포함)은 무보정.

| 조건 | 값 | 보정 |
|------|----|------|
| temperature | `high` | reactivity +1, stability −1 |
| temperature | `low` | reactivity −1, stability +1 |
| pressure | `high` | strength +1, stability +1 |
| catalyst | (존재) | reactivity +1 |

> 주의: 현재 `temperature`/`pressure`는 `"default"` 문자열 기본값이다. v1은 `high`/`low`/`default`만 인식한다. 수치 체계 도입은 비범위.

### 4.4 category 결정론 산출

입력 재료 구성으로 판정한다. (금속군 = iron~silver, 탄소군 = charcoal/coal/wood, 화학군 = sulfur)

| 조건 | category |
|------|----------|
| 입력이 모두 금속 | `alloy` |
| 입력이 모두 탄소/목재 계열 | `organic` |
| 입력이 모두 비금속(탄소+황 등) | `chemical` |
| 금속 + 비금속 혼합 | `composite` |

### 4.5 state 결정론 산출

조합·보정이 끝난 최종 속성으로 판정한다.

| 조건 | state |
|------|-------|
| reactivity ≥ 7 그리고 stability ≤ 3 | `gas` |
| reactivity ≥ 5 그리고 stability ≤ 5 (위 미해당) | `liquid` |
| 그 외 | `solid` |

### 4.6 희귀도 결정론 공식

```
score = 0.7 × avg(strength, conductivity, stability) + 0.3 × max(strength, conductivity, stability, reactivity)
```

| 등급 | 조건 |
|------|------|
| `epic` | score ≥ 8.5 |
| `rare` | 7.0 ≤ score < 8.5 |
| `uncommon` | 5.0 ≤ score < 7.0 |
| `common` | score < 5.0 |

### 4.7 material_hash 재정의

`generate_material_hash`를 **합성 정체성**으로 계산한다. 결과적으로 속성·희귀도·category·state가 모두 이 입력에서 파생되므로 일관된다.

```
material_hash = sha256( machine_type | 정규화_입력(item:qty 정렬) | temp | pressure | catalyst )
```

- `name`·`description`·`risks`·`usage`는 hash에서 **제외**(장식 메타데이터).
- 이는 사실상 `experiment_hash`와 동일한 정체성을 가지므로, 두 해시의 관계/중복 여부를 구현 시 정리한다(같은 합성 = 같은 물질 = 같은 실험).

### 4.8 이름 `name` (LLM 자율 + 검증)

**프롬프트 가이드**: 결정론으로 산출된 `state`·`category`를 프롬프트에 주입하고, "실제 화학 물질·신소재처럼, 해당 상태가 연상되는 한글 명칭으로 지어라"를 지시한다.
- 고체: 「~정(晶)」, 「~합금」, 「~석」
- 액체: 「~용액」, 「~유(油)」, 「~액」
- 기체: 「~기체」, 「~가스」, 「~증기」

**검증 규칙** (순서대로, 실패 시 fallback):
1. 공백 정리 후 빈 값 → fallback
2. 길이 2~24자 (초과 절단 / 미만 fallback)
3. 금지어 치환(`FORBIDDEN_KEYWORDS` → `합금`)
4. 특수문자 제거 (한글·영숫자·공백·하이픈만 허용)
5. 유효문자(한글 or 영숫자) 1자 이상 없으면 → fallback

`id_hint`도 동일 새니타이징.

## 5. 변경 범위

| 파일 | 변경 |
|------|------|
| `agents/material_generation/material_properties.py` (신규) 또는 시드 CSV | 기초 재료 속성 테이블 + 조회/정규화 |
| `agents/material_generation/derivation.py` (신규) | 속성 조합·공정 보정·category·state·rarity 결정론 산출 함수 |
| `agents/material_generation/proposal_generator.py` | LLM은 이름·설명·risks·usage·visual_prompt만 생성. 구조화 값은 derivation 결과로 대체. 프롬프트에 state·category 주입 |
| `agents/material_generation/result_validator.py` | 이름·id_hint 검증/새니타이징 (구조화 값 검증은 derivation으로 이동) |
| `agents/material_generation/normalizer.py` | `generate_material_hash`를 합성 정체성 기반으로 재정의 |
| `agents/material_generation/schemas.py` | `MaterialProposalResult.state`, `MaterialCreationResponse.state` 추가 |
| `db/models.py` | `GeneratedMaterialModel.state` 컬럼 추가 |
| `migrations/versions/000X_add_material_state.py` (신규) | state 컬럼 마이그레이션 |
| `agents/material_generation/nodes.py` | derivation 호출, state 전달 |
| `tests/agents/material_generation/...` | 속성 조합·공정 보정·category·state·rarity 경계·이름 검증·hash 재현성 테스트 |

## 6. 테스트 계획

- **속성 조합**: 단일/복수 입력 수량 가중 평균 검증
- **공정 보정**: high/low/catalyst 가산 + 클램핑 검증
- **category**: 금속/비금속/혼합 케이스
- **state**: gas/liquid/solid 경계 케이스
- **rarity**: score 4.9/5.0/6.9/7.0/8.4/8.5 경계
- **hash 재현성**: 같은 입력 → 같은 material_hash, 다른 입력 → 다른 hash
- **이름 검증**: 빈 값/초과 길이/금지어/순수 기호/정상

## 7. 비범위 (YAGNI)

- 공정 조건의 수치 체계화(현재 high/low/default 문자열만 인식)
- 형태(ingot/powder) 세분 보정
- 아이콘/텍스처 실제 이미지 생성

## 8. 열린 항목 (스펙 리뷰에서 확정)

- 기초 재료 속성 테이블의 **실제 수치**(4.1은 제안 기본값) — 기획 확정 필요
- 속성 테이블 저장 위치: 신규 Python 상수 vs 시드 CSV vs DB 테이블
- `material_hash` ≡ `experiment_hash` 통합 여부(중복 정의 정리)
