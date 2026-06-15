# 물질 이름·희귀도·상태 기준 설계

- 작성일: 2026-06-15
- 대상 에이전트: `material_generation`
- 관련 파일: `backend/src/agents/material_generation/*`

## 1. 배경 / 문제

현재 `material_generation` 에이전트는 신물질 생성 시 다음을 **전적으로 LLM 자율 판단**에 의존한다.

- **물질 이름(`name`)**: 프롬프트에 "한글 명칭" 지시만 있을 뿐 명명 기준이 없다. 검증은 금지어 치환뿐이다. (`result_validator.py`)
- **희귀도(`rarity`)**: 프롬프트에서 `common/uncommon/rare/epic` 중 하나를 고르게만 하고, 결정 기준이 전혀 없다. 검증은 화이트리스트 외 값을 `common`으로 떨구는 것뿐이다.
- **속성 4값(`strength/conductivity/stability/reactivity`)**: LLM이 0~10 실수로 생성하고, 범위 클램핑만 한다.

이로 인한 문제:

1. 희귀도가 LLM의 **독립적 환각**이라 표시 속성과 모순될 수 있다(예: 약한 속성인데 `epic`).
2. 희귀도가 `material_hash`에 포함되는데(`normalizer.py:61`), 같은 조합에 대해 LLM이 매번 다른 희귀도를 내면 **중복 물질이 양산**된다.
3. 이름에 명명 규칙이 없어 세계관 일관성이 떨어지고, 물질의 물리적 상태(고체/액체/기체)가 드러나지 않는다.

## 2. 목표

- 희귀도를 **속성 파생 결정론적 공식**으로 산출하여 표시 속성과 항상 일관되게 만든다.
- 이름을 **실제 화학 물질·신소재처럼**, 물질의 **상태(고체/액체/기체)** 가 연상되도록 명명한다.
- 물질의 **상태를 구조화 필드(`state`)로 도입**하여 데이터·이름·후속 로직(아이콘/텍스처)에서 활용 가능하게 한다.

## 3. 의사결정 요약

| 항목 | 결정 | 비고 |
|------|------|------|
| 속성 4값 | **LLM 생성 유지** | 다양성·서사 보존 |
| 희귀도 | **속성 파생 결정론 공식** (LLM 값 폐기) | 표시 속성과 일관 |
| 이름 | **LLM 자율 생성 + 규칙 검증** | 화학물질 스타일 프롬프트 가이드 |
| 상태 | **구조화 필드 `state` 신규 추가** | LLM 결정 + 화이트리스트 검증 |

### 수용한 트레이드오프

속성이 여전히 LLM 비결정 출력이므로, 희귀도를 공식화해도 **같은 입력의 cross-run 재현성·`material_hash` 안정성은 완전히 해결되지 않는다.** 대신 얻는 것은 **희귀도와 표시 속성 간 내부 일관성**과 명문화된 결정 로직이다. 속성 다양성을 살리기 위해 이 트레이드오프를 수용한다.

## 4. 상세 설계

### 4.1 상태 `state`

- 허용 값: `solid` / `liquid` / `gas`
- 결정 주체: **LLM** (이름과 동일한 호출에서 함께 생성 → 이름·상태 일관성 보장)
- 검증: 화이트리스트(`{solid, liquid, gas}`) 외 값은 기본 `solid`로 보정
- fallback 제안 시 기본값: `solid`

### 4.2 이름 `name`

**프롬프트 가이드 추가** — 다음 취지를 명명 지침으로 삽입한다.

> 실제 화학 물질·신소재처럼, 해당 물질의 상태(고체/액체/기체)가 연상되는 한글 명칭으로 지어라.
> - 고체: 「~정(晶)」, 「~합금」, 「~석」 류
> - 액체: 「~용액」, 「~유(油)」, 「~액」 류
> - 기체: 「~기체」, 「~가스」, 「~증기」 류

**검증 규칙** (순서대로 적용, 실패 시 fallback 이름 사용):

1. 앞뒤 공백 정리 후 빈 문자열 → fallback
2. 길이 2~24자 (24자 초과 시 24자로 절단, 2자 미만 → fallback)
3. 금지어 치환: 기존 `FORBIDDEN_KEYWORDS`(`trash/error/test/dummy/...`) → `합금`
4. 특수문자 제거: 한글·영숫자·공백·하이픈(`-`)만 허용, 그 외 문자는 제거
5. 유효문자 보장: 한글 또는 영숫자가 1자 이상 없으면(순수 기호) → fallback

`id_hint`도 동일하게 금지어·특수문자 새니타이징한다(소문자 + 언더스코어 형식 유지).

### 4.3 희귀도 `rarity`

LLM이 제안한 `rarity`는 **폐기**하고, 클램핑된 속성값으로 결정론 계산한다.

```
score = 0.7 × avg(strength, conductivity, stability) + 0.3 × max(strength, conductivity, stability, reactivity)
```

- 평균 성능(70%): 밸런스형 물질 보상
- 최고 특화 스탯(30%): 단일 특화 물질 보상
- `reactivity`는 "양날"이라 평균에서 제외하고 max에만 반영(과반응 물질도 "주목할 만함"으로 인정)

**등급 구간** (튜닝 가능한 상수):

| 등급 | 조건 |
|------|------|
| `epic` | score ≥ 8.5 |
| `rare` | 7.0 ≤ score < 8.5 |
| `uncommon` | 5.0 ≤ score < 7.0 |
| `common` | score < 5.0 |

### 4.4 해시 연동

`generate_material_hash`(`normalizer.py`)의 payload에 `state`를 포함한다. 상태가 다르면 다른 물질로 식별된다. 희귀도는 속성에서 파생되고 속성은 이미 해시에 포함되므로, 희귀도를 해시에 유지해도 일관성에 문제가 없다(기존 유지).

## 5. 변경 범위

| 파일 | 변경 내용 |
|------|-----------|
| `agents/material_generation/schemas.py` | `MaterialProposalResult.state`, `MaterialCreationResponse.state` 필드 추가 |
| `db/models.py` | `GeneratedMaterialModel.state` 컬럼 추가 |
| `migrations/versions/000X_add_material_state.py` | `generated_materials.state` 컬럼 추가 마이그레이션 (신규) |
| `agents/material_generation/proposal_generator.py` | 프롬프트(상태·화학 명명 가이드, state 출력) + fallback `state` |
| `agents/material_generation/result_validator.py` | `_compute_rarity()`, `_validate_state()`, `_sanitize_name()` 추가 및 `validate_and_correct`에서 호출 |
| `agents/material_generation/normalizer.py` | `generate_material_hash`에 `state` 포함 |
| `agents/material_generation/nodes.py` | `state`를 모델 저장 및 응답에 전달 |
| `tests/agents/material_generation/...` | 희귀도 경계값·이름 검증·state 검증 테스트 추가 |

## 6. 테스트 계획

- **희귀도 경계값**: score가 4.9/5.0/6.9/7.0/8.4/8.5가 되도록 속성을 구성하여 등급 매핑 검증
- **이름 검증**: 빈 값, 25자 초과, 금지어 포함, 순수 기호, 정상 이름 각각에 대한 결과 검증
- **상태 검증**: 화이트리스트 값/비정상 값 → 보정 결과 검증
- **해시**: state 차이가 `material_hash`를 다르게 만드는지 검증
- 기존 회귀: LLM 폐기 rarity가 응답·DB에 결정론 값으로 반영되는지 검증

## 7. 비범위 (YAGNI)

- 속성값 자체의 결정론화(재료 속성 테이블) — 이번 범위 아님
- 공정 조건(온도/압력)으로부터 상태 산출 — 이번 범위 아님(LLM 결정 채택)
- 아이콘/텍스처 실제 이미지 생성 — 별도 작업
