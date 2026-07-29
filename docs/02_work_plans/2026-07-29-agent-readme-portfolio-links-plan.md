# Agent README 및 포트폴리오 링크 작업 계획

## 목표

`operator_guide`와 `process_optimizer`의 실제 구성, 실행 흐름, 안전장치, 검증 근거를 각 Agent 폴더의 README로 설명하고 포트폴리오 PPT/PDF에서 해당 문서로 연결한다.

## 작업 계획

1. 현재 public 실행 경로와 주요 소스 파일을 확인한다.
2. 각 Agent 폴더에 포트폴리오 독자를 위한 `README.md`를 작성한다.
3. README의 코드·테스트 상대 링크가 저장소 구조와 일치하는지 확인한다.
4. PPT의 Agent 코드 링크를 README 주소로 교체한다.
5. PPT 렌더링과 하이퍼링크 저장 여부를 확인한다.

## 작업 로그

- 2026-07-29: `backend/AGENTS.md`와 Agent 구조 문서를 확인했다.
- 2026-07-29: Operator Guide README에 복합 질문 분해, RAG, 상태 결합, context guard, sanitizer, 검증 근거를 정리했다.
- 2026-07-29: Process Optimizer README에 v2 public 경로, operation 흐름, 승인·revision·명령·Undo 검증과 테스트 근거를 정리했다.
- 2026-07-29: 문서 내용은 현재 구현 범위를 기준으로 작성했으며, Undo revision 검증과 효과 측정 범위를 과장하지 않도록 제한 사항을 명시했다.

