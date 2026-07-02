# Sprint 20. 회귀 테스트와 운영 문서 계획

## 목표

하이브리드 의도 분류기가 실제 운영에서 안전하게 유지되도록 테스트와 문서를 정리한다.

## 포함 범위

- 통신탑 제작 질문 회귀 테스트
- 장비 설명, 자원 설명, 레시피 설명, 문제 해결 질문 혼합 테스트
- LLM intent classifier 성공/실패 테스트
- agent-test 입력 예시 문서 업데이트
- Unreal 팀 공유용 질문/응답 가이드 업데이트
- RAG/CSV 업데이트 후 확인 절차 문서화

## 제외 범위

- Unreal UI 실제 구현
- 신규 게임 데이터 추가
- LLM provider 설정 변경
- RAG DB schema 변경

## 대표 회귀 질문 세트

```text
분쇄기가 뭐야?
철근은 어디에 써?
통신탑 어떻게 만들어?
통신탑 어떻게 지어야 해?
통신탑 건설 재료 알려줘
제련기가 작동을 안 해.
철광석이 안 들어와.
```

## 운영 확인 항목

- 최신 CSV가 반영되었는지 확인한다.
- RAG ingestion이 필요한 경우 `scripts\setup_rag_db.bat`를 실행한다.
- 서버가 최신 코드로 재시작되었는지 확인한다.
- agent-test와 Unreal이 같은 backend 서버를 바라보는지 확인한다.
- `metadata.selectedLeafAgent`, `metadata.llmModel`, `metadata.fallback`을 확인한다.

## 완료 기준

- smoke/regression 테스트가 통과한다.
- agent-test 문서에 통신탑 제작 질문 예시가 포함된다.
- Unreal 팀이 “제작 가능한 설치물은 resource와 equipment 양쪽에 있을 수 있다”는 점을 이해할 수 있다.
- 실패 시 확인 순서가 문서화된다.
