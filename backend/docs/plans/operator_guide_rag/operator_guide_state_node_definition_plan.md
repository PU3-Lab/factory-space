# operator_guide State/Node 정의 문서 작성 계획

## 목표

operator_guide agent의 런타임 흐름에서 `state`와 `node`가 무엇을 의미하는지 문서화한다.

이 문서는 LangGraph/agent pipeline을 처음 보는 팀원도 다음 내용을 이해할 수 있게 만드는 것을 목표로 한다.

- 플레이어 질문이 어떤 state로 들어오는지
- 각 node가 state를 어떻게 읽고 갱신하는지
- RAG 검색, 현재 게임 상태, LLM 답변, Unreal 응답 JSON이 어디에서 만들어지는지
- node 단위 로그를 어떻게 남겨야 하는지

## 포함 범위

- operator_guide 기준 state 정의
- node 역할과 입출력
- state 변경 흐름
- 로그/트러블슈팅 기준
- Unreal 응답 JSON과의 연결

## 제외 범위

- 실제 LangGraph 코드 구현
- PostgreSQL/pgvector schema 상세
- Unreal UI 구현
- LLM provider/fallback 구현 상세

## 산출물

- `operator_guide_state_node_definition.md`

## 완료 기준

- `state = 런타임 데이터 묶음`, `node = state를 처리하는 단계`로 명확히 설명한다.
- operator_guide RAG 흐름의 주요 node를 모두 나열한다.
- 각 node가 읽는 state와 쓰는 state를 구분한다.
- 로그는 node 단위로 남긴다는 기준을 포함한다.
- master/sprint plan에서 이 문서를 참조할 수 있게 한다.

## 작업 로그

- 2026-06-11: 사용자 요청에 따라 operator_guide agent 기준 state/node 정의 문서 작성 계획을 추가했다.

## 트러블슈팅 로그

- 2026-06-11: state와 node 개념이 추상적으로 보일 수 있어, 실제 operator_guide RAG 흐름의 데이터 필드와 node 목록을 함께 문서화하기로 했다.
