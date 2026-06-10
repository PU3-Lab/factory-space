# 퀘스트 CSV RAG 확장 기준

## 목적

퀘스트 에이전트가 `data/game/*.csv`를 참조해 생산/납품 퀘스트를 만들 때, 현재 CSV 기반 간이 RAG를 유지할지 PostgreSQL 전문검색 또는 `pgvector`로 확장할지 판단하는 기준을 기록한다.

## 현재 CSV 규모

2026-06-10 기준 `data/game` CSV 현황:

| 파일 | 데이터 행 | 대략 토큰 |
| --- | ---: | ---: |
| `resources.csv` | 57 | 2.9k |
| `recipes.csv` | 30 | 2.7k |
| `equipment.csv` | 14 | 1.7k |
| `troubleshooting_rules.csv` | 18 | 2.4k |
| `action_policy.csv` | 21 | 0.5k |
| 합계 | 140 | 약 10k |

퀘스트 생성에 직접 필요한 `resources.csv`, `recipes.csv`, `equipment.csv`만 보면 101행, 약 7.3k 토큰이다.

## 현재 판단

현재 규모에서는 벡터 DB가 필요하지 않다. CSV가 대부분 `resource_id`, `recipe_id`, `equipment_id`, `issue_id` 같은 명시적 ID 관계를 가지므로, 임베딩 검색보다 구조화 조회가 더 정확하다.

권장 방식:

- CSV를 repository에서 읽는다.
- 이름/ID 기반 lookup을 우선한다.
- 레시피 입력/출력, 장비, 병목 issue 관계를 따라 후보를 만든다.
- LLM prompt에는 전체 CSV를 넣기보다 선택된 후보와 근거만 넣는다.

## 확장 기준

| 규모 또는 조건 | 권장 방식 |
| --- | --- |
| 현재 ~ 3배, 약 400행 이하 | CSV repository + 구조화 조회 유지 |
| 3 ~ 5배, 약 400~700행 또는 3만~5만 토큰 | CSV 전체 prompt 주입은 피하고 top-k 후보만 prompt에 주입 |
| 10배, 약 1,400행 또는 10만 토큰 이상 | PostgreSQL 테이블 + 인덱스/전문검색으로 전환 |
| 30배 이상, 약 4,000행 또는 30만 토큰 이상 | PostgreSQL + `pgvector` 도입 검토 |
| 자연어 의미 검색이 핵심이 됨 | 행 수가 적어도 `pgvector` 도입 검토 |

자연어 의미 검색이 핵심인 경우의 예:

- "초반에 막히기 쉬운 생산 문제"
- "철괴랑 비슷한 역할을 하는 재료"
- "납품하기 좋은 과잉 재고"
- "이 상황과 비슷한 병목"
- "플레이어가 다음에 배워야 할 공정"

## PostgreSQL 전환 기준

다음 중 하나가 발생하면 CSV 파일 직접 조회 대신 PostgreSQL 테이블을 기준 저장소로 옮긴다.

- CSV 행 수가 1,000행을 넘는다.
- 여러 agent가 같은 게임 데이터를 동시에 조회한다.
- 런타임에서 게임 데이터가 수정된다.
- 최근 퀘스트, 창고 상태, 해금 상태와 정적 CSV 데이터를 자주 join해야 한다.
- CSV 로딩/파싱 비용이 테스트나 요청 경로에서 눈에 띄기 시작한다.

퀘스트 에이전트는 처음부터 `QuestSituationRepository` 인터페이스 뒤에서 데이터를 읽도록 설계한다. 그래서 CSV repository에서 PostgreSQL repository로 바뀌어도 agent/service 계약은 유지한다.

## pgvector 전환 기준

PostgreSQL 전환만으로 충분하지 않고, 다음 조건이 생기면 `pgvector`를 붙인다.

- exact ID/name lookup으로 원하는 근거를 찾기 어렵다.
- 설명문, 매뉴얼, 이벤트 로그처럼 긴 비정형 텍스트가 늘어난다.
- "비슷한", "관련 있는", "다음에 배울 만한" 같은 의미 기반 질의가 주요 기능이 된다.
- keyword 검색 결과가 너무 많아서 ranking 품질이 떨어진다.
- LLM prompt에 넣기 전 후보를 5~20개로 좁히는 retrieval ranking이 필요하다.

권장 도입 형태:

- 별도 벡터 DB를 먼저 도입하지 않는다.
- 기존 PostgreSQL에 `pgvector` extension을 추가한다.
- 원본 row id, source type, title, embedding, searchable text를 함께 저장한다.
- 최종 응답에는 retrieval source id를 metadata로 남긴다.

## 퀘스트 생성에서의 원칙

생산/납품 퀘스트 생성은 구조화 데이터 성격이 강하다. 따라서 기본 판단은 다음 순서를 따른다.

1. PostgreSQL 상황 조회: 창고, 최근 퀘스트, 해금 레시피, 사용 가능 장비
2. CSV 또는 PostgreSQL 기준 데이터 조회: 자원, 레시피, 장비 관계
3. 규칙 기반 후보 생성과 점수화
4. 필요한 경우 LLM으로 5개 선택 또는 문장화
5. schema 검증 후 `quests` 5개 반환

벡터 검색은 2번의 보조 수단일 뿐, 5개 개수 보장이나 타입 제한을 LLM/벡터 검색에 맡기지 않는다.
