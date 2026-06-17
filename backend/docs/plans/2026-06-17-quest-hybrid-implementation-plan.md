# 퀘스트 하이브리드 생성 — 구현 계획 (TDD)

> **출처:** 본 문서는 [2026-06-17-quest-main-quest-hybrid-design.md](./2026-06-17-quest-main-quest-hybrid-design.md)의 구현 계획이다.
> 설계 문서의 "미해결 세부 1~3"(design.md:60-64)을 아래 **확정 결정**으로 잠그고, 그 위에서 TDD 단계를 정의한다.

- 작성일: 2026-06-17
- 상태: 구현 계획 (착수 전)
- 관련 코드: `src/agents/quest_generator/`, `src/llm/adapter.py`

## 확정 결정 (미해결 세부 잠금)

| # | 항목 | 결정 |
|---|------|------|
| ① | LLM 문구 레이어 위치 | **`compose-support` 내부 단계**. `compose_support_quest` 핸들러 안에서 `draft 선택 → LLM polish → 재검증 → 영속`을 한 흐름으로. leaf 에이전트는 polish 함수로 재활용. |
| ② | LLM 실패 시 폴백 | **규칙 원본 `title`/`description`으로 폴백**. LLM이 `None`/빈 응답/파싱 실패면 draft 원본 문구 그대로 사용. |
| ③ | 검증 강도 | **draft 원본값으로 강제 복원**. LLM 출력에서 `title`/`description`만 취하고 `objectives`/`rewards`/`support_type`/목표는 무조건 draft 원본으로 덮어씀. 구조적으로 불변식 위반 불가. |

## 목표 아키텍처

```
compose_support_quest (quest_router.py)
  1. active 상한/중복 검사            (기존 유지)
  2. QuestContextBuilder.build_context (기존 유지)
  3. QuestRuleGenerator.generate_drafts → 후보군         (기존 유지)
  4. QuestValidator로 첫 통과 draft 선택 (selected_draft) (기존 유지)
  5. ★신규★ QuestPhraseRefiner.refine(selected_draft, context)
        → title/description만 LLM로 다듬은 새 draft 반환 (immutable copy)
        → 실패 시 원본 draft 그대로 (결정 ②)
        → objectives/rewards는 항상 원본 강제 (결정 ③)
  6. ★신규★ QuestValidator.validate(refined_draft, ...) 재검증
        → 만약 실패하면 selected_draft(원본)로 폴백 후 영속
  7. QuestManager.create_quest_from_draft (기존 유지)
```

핵심: 5·6단계만 추가. 1~4·7은 손대지 않는다 → 회귀 위험 최소화.

## 신규 컴포넌트

### `QuestPhraseRefiner` (`src/agents/quest_generator/phrase_refiner.py`)

- 책임: draft의 `title`/`description`만 LLM으로 자연스럽게 다듬어 **새 `SupportQuestDraft`를 반환**(불변, 원본 미변경).
- 의존: `LLMAdapter`(`src/llm/adapter.py`의 `invoke(prompt) -> str | None`) 주입. 테스트는 가짜 adapter로 결정적 제어.
- 계약:
  - 입력: `draft: SupportQuestDraft`, `context: QuestContext`
  - 출력: `SupportQuestDraft` — `title`/`description`만 교체, 그 외 필드는 `draft`와 동일(결정 ③, `model_copy(update=...)`).
  - 폴백(결정 ②): adapter가 `None` 반환 / JSON 파싱 실패 / `title`·`description` 키 누락 / 빈 문자열 → **원본 draft 그대로 반환**.
  - LLM 프롬프트: draft의 원본 title/description + 메인퀘 맥락(`context.current_main_quest.title`)을 주고 "수량·아이템·보상은 절대 바꾸지 말고 문구만 다듬어라. JSON `{\"title\":..,\"description\":..}`만 출력"으로 강제.
- 프롬프트 톤/스키마는 기존 `economy_quest.py`의 "JSON 객체 하나만, 마크다운 펜스 금지" 패턴 재사용(DRY).

### `compose_support_quest` 수정 (`quest_router.py`)

- 5·6단계 삽입. `QuestPhraseRefiner`는 DI 가능하게 — 기본 인스턴스를 모듈 상수로 두되 테스트에서 교체 가능한 형태(예: 기본 인자 또는 간단한 provider 함수).
- 재검증(6단계) 실패 시 `selected_draft` 원본으로 폴백(이중 안전망). 이 경우에도 201 정상 응답.

## TDD 단계

> 규칙: 각 단계 RED(실패 테스트) → GREEN(최소 구현) → REFACTOR. `pytest --cov=src` 80%+ 유지. 작업 후 `ruff check --fix .` + `ruff format .`.

### 1단계 — `QuestPhraseRefiner` 단위 테스트 (신규 `tests/test_quest_phrase_refiner.py`)

- `test_refines_title_and_description_from_llm_json`: 가짜 adapter가 정상 JSON 반환 → title/description만 교체됨.
- `test_preserves_objectives_and_rewards_when_llm_changes_them`(결정 ③ 핵심): LLM이 objectives/rewards/수량을 바꿔 응답해도 결과 draft의 objectives·rewards·support_type은 원본과 **동일**.
- `test_falls_back_to_original_when_adapter_returns_none`(결정 ②): adapter `None` → 원본 draft 반환.
- `test_falls_back_on_malformed_json` / `test_falls_back_on_missing_keys` / `test_falls_back_on_empty_strings`.
- `test_does_not_mutate_input_draft`(불변성, coding-style CRITICAL): 호출 후 원본 draft 객체 불변 확인.
- `test_prompt_includes_main_quest_context`: build된 프롬프트에 메인퀘 title·원본 문구 포함.

### 2단계 — `compose-support` 통합 테스트 (기존 라우터 테스트 파일에 추가)

- `test_compose_support_uses_refined_phrasing`: 가짜 refiner/adapter 주입 → 응답 `title`/`description`이 다듬어진 값.
- `test_compose_support_keeps_rule_decided_values`: 응답의 objective `target_id`/`target_amount`, reward `amount`가 규칙 결정값과 일치(LLM이 바꾸려 해도).
- `test_compose_support_falls_back_when_llm_unavailable`: adapter `None` → 규칙 원본 문구로 201.
- `test_revalidation_failure_falls_back_to_original_draft`: refined draft가 검증 실패하도록 강제 → 원본으로 영속, 201.
- 기존 통과 테스트(상한 초과 400, 후보 없음 400, 중복 차단)는 **그대로 통과**해야 함(회귀 가드).

### 3단계 — 리팩터/정리

- `production_quest.py`/`economy_quest.py` leaf의 위치 재조정: design.md 66-72 "이미 반영된 변경 유지" 범위 확인. 하이브리드 경로가 정식이므로, 이 leaf들이 더 이상 메인 경로가 아님을 docstring/주석에 명시(혼선 방지). **삭제는 범위 외** — 별도 결정 필요.
- `game_state` ↔ `QuestContext` 스키마 통일(결정 3, design.md:34)은 본 계획 범위에서 **문구 레이어 입력으로만** 사용. WS 경로 전면 통일은 후속.

## 범위 밖 (후속)

- `/ws/agent` 경로를 `QuestContext`로 완전 수렴시키는 작업(design.md 결정 1·3의 WS 측).
- leaf 에이전트(`production_quest`/`economy_quest`) 제거 여부.
- 보상 다양화(현재 고정 100 gold), `collect_item` 외 목표 유형.

## 리스크 / 가드

- **불변식 침해**: 결정 ③의 강제 복원 + 6단계 재검증 이중 가드로 차단. 테스트 `test_preserves_objectives_and_rewards_when_llm_changes_them`이 핵심 회귀 가드.
- **LLM 비결정성**: 라우터/리파이너 테스트는 전부 가짜 adapter로 결정적 실행. 실 LLM 호출 테스트는 만들지 않음(통합 환경 별도).
- **회귀**: 1~4·7단계 미변경 + 기존 라우터 테스트 유지로 보호.

## 완료 기준

- [ ] `QuestPhraseRefiner` 단위 테스트 전부 통과(②③ 포함).
- [ ] `compose-support` 통합 테스트 전부 통과, 기존 테스트 회귀 없음.
- [ ] LLM 결정값 변조 차단이 테스트로 증명됨.
- [ ] `pytest --cov=src` 80%+.
- [ ] `ruff check` / `ruff format` 클린.
- [ ] CLAUDE.md 규칙대로 리뷰 문서(`docs/04_reviews/`) 작성.
