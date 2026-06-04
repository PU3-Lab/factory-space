# Task RED - Operator Guide Manual Q&A Proto 구조 정리

## Scope

기존 `qa_chatbot` 프로토 패키지를 최신 backend agent 구조와 네이밍 결정에 맞춰 `operator_guide` 내부 구현으로 정리한다.

## Naming decision

- 최신 main의 Q&A/매뉴얼 안내 도메인은 `operator_guide`를 사용한다.
- `qa_chatbot`은 이전 프로토 이름이므로 active backend package name으로 유지하지 않는다.
- 내부 질문 유형 분류는 agent routing이 아니므로 `intent_router.py` 대신 `question_classifier.py`로 둔다.

## RED verification

Production code 수정 전에 테스트 기대값을 먼저 `operator_guide` 기준으로 바꾼다.

예상 실패:

- `agents.operator_guide.service` 또는 `agents.operator_guide.question_classifier`가 아직 없어서 import 실패한다.
- `agents.qa_chatbot` 패키지가 남아 있어 구조 정리 기준을 만족하지 못한다.

## Acceptance

- Manual Q&A proto runtime 파일은 `backend/src/agents/operator_guide/` 아래에 있다.
- active backend code/tests/scripts에서 `agents.qa_chatbot` import를 사용하지 않는다.
- `backend/src/agents/qa_chatbot/` 패키지는 제거한다.
- 질문 분류 파일명은 `question_classifier.py`다.
- 대표 질문 smoke test가 `operator_guide` 구조로 통과한다.
- 전체 backend 테스트가 통과한다.
