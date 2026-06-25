# operator_guide agent-test 서버 연결 가이드

이 문서는 Windows 환경에서 backend 테스트 서버를 실행하고, 브라우저의 `/agent-test` 화면에서 `operator_guide` agent를 직접 테스트하는 방법을 정리한 가이드다.

## 1. 서버 실행

PowerShell을 열고 아래 명령어를 실행한다.

```powershell
cd C:\factory-space\backend
uv run --env-file .env.prod python scripts/run_prod_server.py

```

성공하면 터미널에 아래와 비슷한 로그가 나온다.

```text
Uvicorn running on http://0.0.0.0:18000
```

브라우저에서는 아래 주소로 접속한다.

```text
http://127.0.0.1:18000/agent-test
```

## 2. 서버가 켜졌는지 확인

브라우저에서 먼저 health check를 열어볼 수 있다.

```text
http://127.0.0.1:18000/health
```

정상이라면 서버가 응답한다.

agent-test 화면은 아래 주소다.

```text
http://127.0.0.1:18000/agent-test
```

WebSocket 경로는 아래와 같다.

```text
ws://127.0.0.1:18000/ws/agent
```

## 3. 포트가 이미 사용 중일 때

`18000` 포트가 이미 사용 중이면 다른 포트로 실행한다.

```powershell
cd C:\factory-space\backend
uv run --env-file .env.prod python scripts/run_prod_server.py --port 18000
```

이 경우 브라우저 주소도 포트만 바꾼다.

```text
http://127.0.0.1:18000/agent-test
```

## 4. 왜 bash scripts/run_server.sh가 안 될 수 있나

첨부 이미지처럼 macOS/Linux/Git Bash 환경에서는 아래 명령을 사용할 수 있다.

```bash
bash scripts/run_server.sh
```

하지만 Windows PowerShell에서 bash가 없으면 아래 오류가 날 수 있다.

```text
execvpe(/bin/bash) failed: No such file or directory
```

이 경우 정상이다. Windows에서는 아래 명령을 사용한다.

```powershell
cd C:\factory-space\backend
uv run --env-file .env.prod python scripts/run_prod_server.py
```

## 5. operator_guide 설비 도움말 테스트

`/agent-test` 화면의 JSON 입력칸에 아래 JSON을 넣고 전송한다.

```json
{
  "type": "agent.request",
  "request_id": "operator-guide-equipment-001",
  "session_id": "agent-test-session",
  "client_id": "agent-test-console",
  "agent": "operator_guide",
  "payload": {
    "question": "분쇄기가 뭐야? 어디에 써?"
  },
  "context": {
    "language": "ko",
    "mode": "agent_test"
  }
}
```

기대 흐름:

```text
질문 입력
-> orchestrator/operator_guide 처리
-> 설비 도움말 성격으로 분류
-> 관련 매뉴얼 근거를 사용해 답변
```

## 6. operator_guide 레시피 설명 테스트

```json
{
  "type": "agent.request",
  "request_id": "operator-guide-recipe-001",
  "session_id": "agent-test-session",
  "client_id": "agent-test-console",
  "agent": "operator_guide",
  "payload": {
    "question": "철괴를 만들려면 어떻게 해야 돼?"
  },
  "context": {
    "language": "ko",
    "mode": "agent_test"
  }
}
```

기대 흐름:

```text
질문 입력
-> operator_guide 처리
-> 레시피/제작법 질문으로 분류
-> 철괴 관련 레시피 근거를 사용해 답변
```

## 7. operator_guide 트러블슈팅 테스트

```json
{
  "type": "agent.request",
  "request_id": "operator-guide-trouble-001",
  "session_id": "agent-test-session",
  "client_id": "agent-test-console",
  "agent": "operator_guide",
  "payload": {
    "question": "컨베이어가 멈췄는데 뭘 확인해야 해?"
  },
  "context": {
    "language": "ko",
    "mode": "agent_test"
  }
}
```

기대 흐름:

```text
질문 입력
-> operator_guide 처리
-> 트러블슈팅 질문으로 분류
-> 전력, 입력 자원, 출력 저장 공간, 연결 상태 같은 점검 항목 답변
```

## 8. 여러 질문 한 번에 테스트

Sprint 8에서는 한 문장 안의 여러 질문을 sub-question으로 나누는 구조를 추가했다.

```json
{
  "type": "agent.request",
  "request_id": "operator-guide-multi-001",
  "session_id": "agent-test-session",
  "client_id": "agent-test-console",
  "agent": "operator_guide",
  "payload": {
    "question": "분쇄기가 뭐야? 그리고 철괴를 만들려면 어떻게 해야 돼?"
  },
  "context": {
    "language": "ko",
    "mode": "agent_test"
  }
}
```

기대 흐름:

```text
원본 질문
-> sub-question 1: 분쇄기가 뭐야?
-> sub-question 2: 철괴를 만들려면 어떻게 해야 돼?
-> 각 질문별 RAG 검색
-> 이후 runtime integration 단계에서 질문별 답변으로 연결
```

주의:

```text
현재 Sprint 8-2까지는 sub-question별 RAG 검색 구조까지 구현되어 있다.
agent-test 최종 응답에 multi-question 결과를 완전히 반영하는 작업은 다음 runtime integration 단계에서 연결한다.
```

## 9. 자주 나는 오류

### bash 실행 오류

오류:

```text
execvpe(/bin/bash) failed: No such file or directory
```

해결:

```powershell
cd C:\factory-space\backend
uv run --env-file .env.prod python scripts/run_prod_server.py
```

### agent-test 화면이 안 열림

확인할 것:

```text
1. 서버 터미널이 켜져 있는지 확인
2. http://127.0.0.1:18000/health 접속 확인
3. 포트를 18001로 바꿔 실행했다면 /agent-test 주소도 18001로 접속
```

### ROUTING_UNAVAILABLE 오류

가능한 원인:

```text
- 요청 JSON의 agent 값이 잘못됨
- LLM provider/model 설정이 잘못됨
- .env.prod가 로드되지 않음
```

확인:

```text
operator_guide 테스트라면 agent 값은 "operator_guide"여야 한다.
```

### 응답이 늦게 옴

가능한 원인:

```text
- OpenAI/Gemini 같은 외부 LLM 호출 중
- 로컬 LLM/Ollama가 처음 로딩 중
- 네트워크 지연
```

조치:

```text
1. 터미널 로그를 확인한다.
2. 같은 요청을 한 번 더 보내기 전에 10~30초 정도 기다린다.
3. 계속 멈추면 서버를 끄고 다시 실행한다.
```

## 10. 빠른 실행 요약

```powershell
cd C:\factory-space\backend
uv run --env-file .env.prod python scripts/run_prod_server.py
```

브라우저:

```text
http://127.0.0.1:18000/agent-test
```

operator_guide 최소 테스트 JSON:

```json
{
  "type": "agent.request",
  "request_id": "operator-guide-demo-001",
  "session_id": "agent-test-session",
  "client_id": "agent-test-console",
  "agent": "operator_guide",
  "payload": {
    "question": "철괴를 만들려면 어떻게 해야 돼?"
  },
  "context": {
    "language": "ko",
    "mode": "agent_test"
  }
}
```
