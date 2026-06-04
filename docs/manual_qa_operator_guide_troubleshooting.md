# Manual Q&A operator_guide 작업 트러블슈팅

## 목적

이 문서는 Manual Q&A CSV 프로토를 `operator_guide`와 LangGraph 실행 경로에 연결하면서 발생한 문제와 해결 방법을 정리한다.

나중에 비슷한 문제가 생겼을 때 아래 순서로 확인하면 된다.

```text
증상
-> 원인
-> 해결 방법
-> 확인한 테스트
```

## 1. PR 브랜치가 최신 main을 포함하지 않음

### 증상

PR 브랜치를 만들었지만 Git 상태를 확인하면 최신 `origin/main`이 포함되지 않은 상태였다.

```text
origin/main is NOT ancestor of HEAD
```

또는 아래처럼 브랜치가 앞서고 뒤처진 상태로 보였다.

```text
ahead N, behind N
```

### 원인

다른 팀원이 `main`에 새 커밋을 push했지만, 현재 작업 브랜치에 아직 병합하지 않았기 때문이다.

### 해결

원격 정보를 가져오고 현재 작업 브랜치에 `origin/main`을 병합했다.

```bash
git fetch origin
git merge origin/main
```

충돌이 없으면 그대로 진행하고, 충돌이 있으면 파일별로 해결한 뒤 테스트를 다시 실행한다.

### 확인

```bash
git merge-base --is-ancestor origin/main HEAD
```

성공하면 현재 브랜치가 최신 `origin/main`을 포함하고 있다는 뜻이다.

## 2. PR 브랜치를 삭제했지만 다시 PR을 만들고 싶음

### 증상

GitHub에서 PR 브랜치를 삭제했는데 다시 PR을 만들고 싶었다.

로컬에는 브랜치가 남아 있지만 원격 추적 브랜치는 사라진 상태였다.

```text
origin/feature/manual-qa-operator-guide-pr: gone
```

### 원인

GitHub의 원격 브랜치만 삭제되었고, 로컬 브랜치는 그대로 남아 있었기 때문이다.

### 해결

원격 삭제 상태를 반영한 뒤, 같은 브랜치를 다시 push했다.

```bash
git fetch --prune origin
git push -u origin feature/manual-qa-operator-guide-pr
```

### 확인

```text
feature/manual-qa-operator-guide-pr...origin/feature/manual-qa-operator-guide-pr
```

이렇게 나오면 로컬 브랜치와 원격 브랜치가 다시 연결된 상태다.

## 3. 최신 main 기준으로 cherry-pick할 때 CSV 파일이 없음

### 증상

Manual Q&A 작업 커밋을 최신 `origin/main` 기준 브랜치에 cherry-pick한 뒤 테스트가 실패했다.

대표 에러:

```text
FileNotFoundError: data/game/equipment.csv
FileNotFoundError: data/game/action_policy.csv
```

### 원인

Manual Q&A 프로토 코드는 추가되었지만, 프로토가 읽어야 하는 5개 CSV가 해당 브랜치에 포함되지 않았기 때문이다.

프로토에서 필요한 CSV:

```text
data/game/equipment.csv
data/game/resources.csv
data/game/recipes.csv
data/game/troubleshooting_rules.csv
data/game/action_policy.csv
```

### 해결

이전 작업 브랜치 또는 main에 있던 5개 CSV를 현재 브랜치에 복원했다.

```bash
git restore --source=main -- data/game/equipment.csv data/game/resources.csv data/game/recipes.csv data/game/troubleshooting_rules.csv data/game/action_policy.csv
```

그 뒤 커밋에 포함했다.

### 확인

```bash
cd backend
uv run --extra dev pytest tests/test_manual_qa_agent_smoke.py -v
```

기대 결과:

```text
10 passed
```

## 4. LangGraph 경로 테스트에서 final_answer가 없음

### 증상

LangGraph 경로에서 `operator_guide.machine_help`가 선택되었을 때 Manual Q&A 답변을 기대하는 테스트를 추가했더니 실패했다.

대표 에러:

```text
KeyError: 'final_answer'
```

### 원인

기존 `operator_guide` leaf agent fallback은 고정 안내 문장만 반환하고 있었다.

즉, 아래 흐름은 존재했다.

```text
ManualQAService.answer()
-> final_answer 생성
```

하지만 실제 LangGraph leaf fallback은 아직 이 서비스를 호출하지 않았다.

### 해결

`service.py`에 `build_manual_qa_agent_result()`를 추가했다.

이 함수가 하는 일:

```text
payload에서 question 추출
-> ManualQAService.answer(question)
-> AgentRunResult로 변환
```

그리고 아래 leaf agent fallback이 이 함수를 호출하도록 바꿨다.

```text
machine_help.py
recipe_explainer.py
troubleshooter.py
```

### 확인

```bash
cd backend
uv run --extra dev pytest tests/test_message_router.py::test_pipeline_operator_guide_fallback_returns_manual_qa_csv_answer -v
```

기대 결과:

```text
1 passed
```

## 5. 기존 leaf fallback 테스트가 실패함

### 증상

Manual Q&A 결과를 leaf fallback에 연결한 뒤 기존 leaf agent 테스트가 실패했다.

대표적으로 기존 테스트는 metadata가 아래처럼 단순한 구조라고 기대했다.

```json
{
  "fallback": true,
  "sub_agent": "operator_guide.machine_help"
}
```

하지만 변경 후 metadata에는 Manual Q&A 정보가 추가되었다.

```text
question_type
sources
recommended_actions
confidence
primary_manual
supporting_manuals
target_ids
```

### 원인

fallback 응답 계약이 확장되었기 때문이다.

기존 테스트는 이전 고정 안내 문장 fallback 기준으로 작성되어 있었다.

### 해결

`tests/test_agent_leaf_behaviors.py`를 새 응답 구조에 맞게 갱신했다.

새로 확인하는 항목:

```text
payload.actions == []
payload.final_answer == payload.text
metadata.fallback == True
metadata.sub_agent == leaf agent id
metadata.question_type 존재
metadata.sources 존재
metadata.recommended_actions 존재
```

### 확인

```bash
cd backend
uv run --extra dev pytest tests/test_agent_leaf_behaviors.py::test_operator_guide_leaf_agents_return_normalized_fallbacks tests/test_message_router.py -v
```

기대 결과:

```text
19 passed
```

## 6. 전체 테스트에서 pipeline graph 공통 실패가 발생함

### 증상

한 시점에서 전체 테스트를 실행했을 때 여러 pipeline 관련 테스트가 실패했다.

대표 에러:

```text
ValueError: At 'validate_process_payload' node, 'route_selected_leaf_agent' branch found unknown target 'build_fallback'
```

### 원인

이 문제는 Manual Q&A CSV 프로토 자체가 아니라, 당시 최신 `origin/main`의 LangGraph pipeline graph 변경과 관련된 공통 이슈였다.

이후 최신 main을 다시 병합하자 pipeline graph 관련 수정이 들어왔고 전체 테스트가 통과했다.

### 해결

최신 `origin/main`을 다시 가져오고 현재 브랜치에 병합했다.

```bash
git fetch origin
git merge origin/main
```

### 확인

```bash
cd backend
uv run --extra dev pytest -v
```

최종 확인 결과:

```text
140 passed
```

## 7. 자원 질문을 어떤 leaf agent가 처리하는지 헷갈림

### 증상

`operator_guide` leaf agent는 3개뿐이다.

```text
operator_guide.machine_help
operator_guide.recipe_explainer
operator_guide.troubleshooter
```

그런데 Manual Q&A 내부 질문 유형에는 `resource_question`이 있다.

```text
equipment_question
resource_question
recipe_question
troubleshooting_question
unknown_question
```

그래서 자원 질문이 어디에서 처리되는지 헷갈릴 수 있다.

### 원인

LangGraph leaf agent 분류와 ManualQAService 내부 질문 유형 분류가 서로 다른 층이기 때문이다.

```text
LangGraph leaf agent
= 큰 담당자 선택

ManualQAService question_type
= 실제 매뉴얼 질문 유형 선택
```

### 해결

프로토 단계에서는 자원 질문을 `recipe_explainer` leaf agent가 함께 담당하도록 정리했다.

예시:

```text
질문: 철괴는 어떻게 만들어?

LangGraph leaf:
operator_guide.recipe_explainer

ManualQAService question_type:
resource_question
```

### 향후 확장

자원 질문이 많아지면 아래 leaf agent를 새로 추가할 수 있다.

```text
operator_guide.resource_explainer
```

하지만 현재 프로토에서는 `recipe_explainer`가 자원 생산/사용처 질문을 함께 처리해도 충분하다.

## 8. 문서 인코딩이 깨져 보임

### 증상

일부 문서나 prompt 문자열이 깨진 한글처럼 보였다.

예시:

```text
?댁쁺??媛?대뱶
```

### 원인

과거 작업 중 파일 인코딩 또는 콘솔 출력 인코딩이 맞지 않아 깨진 문자열이 저장된 것으로 보인다.

### 해결

이번에 새로 작성한 leaf agent prompt는 정상 한글 문장으로 교체했다.

예시:

```text
다음 설비 도움말 질문에 답변하세요
다음 레시피 질문에 답변하세요
다음 공장 문제를 진단하고 해결 순서를 제안하세요
```

### 남은 주의점

기존 문서 중 일부는 여전히 깨진 문자열이 남아 있을 수 있다.  
기능에는 직접 영향이 없지만, 리뷰 전에 핵심 문서는 정상 한글로 정리하는 것이 좋다.

## 최종 확인 명령

이번 작업 후 확인한 명령은 다음과 같다.

```bash
cd backend
uv run --extra dev pytest tests/test_message_router.py::test_pipeline_operator_guide_fallback_returns_manual_qa_csv_answer -v
```

```text
1 passed
```

```bash
uv run --extra dev pytest tests/test_agent_leaf_behaviors.py::test_operator_guide_leaf_agents_return_normalized_fallbacks tests/test_message_router.py -v
```

```text
19 passed
```

```bash
uv run --extra dev pytest tests/test_manual_qa_agent_smoke.py -v
```

```text
10 passed
```

```bash
uv run --extra dev pytest -v
```

```text
140 passed
```
