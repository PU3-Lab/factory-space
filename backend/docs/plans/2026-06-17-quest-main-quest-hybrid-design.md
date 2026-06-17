# 퀘스트 생성 — 메인 퀘스트 흐름 기반 하이브리드 설계

> **출처:** 본 문서는 [2026-06-04-quest-rag-plan.md](./2026-06-04-quest-rag-plan.md)의 후속/구체화다.
> 큰 방향("LLM 생성은 후속 확장이다. 현재는 결정적 템플릿 생성으로 테스트 가능한 기반을 먼저 만든다",
> 2026-06-04-quest-rag-plan.md:19)은 원 계획에 이미 있었고, 본 문서는 그 위에 **갈라진 두 생성 경로
> (compose-support vs LLM leaf)의 현재 상태 진단**과 **둘을 합치는 구체적 하이브리드 파이프라인**을 추가한다.

- 작성일: 2026-06-17
- 상태: 설계 합의 (구현 착수 전)
- 관련 코드: `src/agents/quest_generator/`, `compose-support` REST 엔드포인트(PR #279)

## 배경 / 문제

퀘스트 요청은 플레이어가 "이런 퀘스트 만들어줘"라고 자연어로 지정하는 챗봇 방식이 아니다.
**게임에서 요청이 오면 시스템이 현재 메인 퀘스트 흐름에 맞춰 적절한 퀘스트를 자동 생성**해야 한다.

현재 생성 경로가 둘로 갈라져 있고 입력 계약도 어긋나 있다.

| | A. compose-support (REST) | B. LLM leaf (production/economy) |
|---|---|---|
| 진입 | `POST /api/v1/factories/{id}/quests/compose-support` | `/ws/agent`, `/agent-test` |
| 메인퀘 인지 | O — `current_main_quest` + `known_issues`(부족 자원) | X — generic(`game_state`) |
| 생성 방식 | 규칙 기반 `QuestRuleGenerator`(결정적) | LLM tool_call / JSON |
| 결과 | 부족 자원 메우는 지원 퀘스트 → `related_main_quest_id` 연결, DB 영속, active 3개 상한 | 퀘스트 JSON, 비영속 |
| 입력 스키마 | `QuestContext`(flat) | `payload.game_state`(nested) |

"메인 퀘스트 흐름에 맞춰서"의 정답 설계는 이미 A(compose-support)에 구현돼 있으며, B는 메인퀘와
무관한 별도 generic 경로다.

## 결정 사항

- **결정 2 (생성 방식): 하이브리드** — 게임 밸런스를 결정하는 값은 규칙이 결정하고, LLM은 문구만 다듬는다.
- **결정 1 (경로 역할): 수렴** — `compose-support`가 규칙 코어, B(leaf)는 LLM 문구 레이어로 재배치.
- **결정 3 (스키마): `QuestContext`로 통일** — `current_main_quest`를 정식 입력으로. `game_state`는 이 컨텍스트를 담는 그릇으로 정리.

## 하이브리드 파이프라인

```
Unreal → QuestContext(current_main_quest + known_issues/부족 자원)
   │
   ▼  ① 규칙 레이어 (결정적, "무엇을")        ← 기존 QuestRuleGenerator
       메인퀘 목표 대비 부족 자원 계산 → SupportQuestDraft
       (item_id · quantity · rewards · related_main_quest_id 확정)
   │
   ▼  ② LLM 레이어 ("표현만")                ← leaf 에이전트 재활용
       draft의 title · description 만 자연스럽게 다듬음
       ※ item_id / quantity / rewards / 목표는 절대 변경 금지
   │
   ▼  ③ 검증 (QuestValidator)               ← LLM 출력도 재검증해 불변식 보호
   │
   ▼  ④ 영속 (QuestInstance, active 3개 상한)
```

### 핵심 원칙

- **결정값은 규칙**: 부족 자원·수량·보상·메인퀘 연결은 `QuestRuleGenerator`가 결정한다.
- **표현만 LLM**: LLM은 `title`/`description` 문구만 손댄다.
- **불변식 보호**: LLM 출력은 항상 `QuestValidator`로 재검증한다. LLM이 수량/아이템을 바꾸면 거른다.

## 미해결 세부 (구현 전 확정 필요)

1. **LLM 문구 레이어 위치** — `compose-support` 내부 단계로 넣을지 / 별도 호출로 둘지.
2. **LLM 실패 시 폴백** — 규칙이 만든 원본 `title`/`description`으로 폴백 (권장).
3. **검증 강도** — LLM이 `item_id`/`quantity`를 바꾸면 draft 원본값으로 강제 복원할지, 에러로 거를지.

## 이번 세션에서 이미 반영된 변경 (유지)

- `production_quest`/`economy_quest` leaf 프롬프트를 자연어 "만들어줘" 요청 의존에서 `game_state` 기반
  자동 생성으로 변경. 콘솔 프리셋도 `game_state: {}`로 변경.
  → 하이브리드 설계의 ② LLM 문구 레이어로 재조정될 예정. `game_state`는 `current_main_quest` 컨텍스트를
  담도록 통일한다.
- `/ws/agent` progress 이벤트의 `agent` 라벨을 요청 기준으로 수정(하드코딩 `operator_guide` 제거).

## 다음 단계

미해결 세부 1~3을 확정한 뒤, 별도 세션에서 TDD로 구현 착수.
