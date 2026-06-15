# operator_guide RAG PDF 디자인 개선 계획

## 목표

`operator_guide_rag_master_plan.pdf`와 `operator_guide_rag_sprint_plan.pdf`를 발표/포트폴리오 문서처럼 읽기 좋게 다시 렌더링한다.

## 개선 방향

- 남색 중심 스타일을 제거한다.
- 첫 페이지에 문서 제목이 명확히 보이도록 cover page를 추가한다.
- 색상은 따뜻한 흰색 배경, 짙은 차콜 텍스트, muted green, muted amber accent를 사용한다.
- 본문은 긴 기술 문서를 읽기 쉽게 spacing, heading, code block, list 스타일을 정리한다.
- 원본 markdown 내용은 유지하고 PDF 렌더링 스타일만 개선한다.

## 대상 파일

- `operator_guide_rag_master_plan.md`
- `operator_guide_rag_sprint_plan.md`
- `operator_guide_rag_master_plan.pdf`
- `operator_guide_rag_sprint_plan.pdf`

## 검증 기준

- PDF 첫 페이지에 제목과 설명이 보인다.
- 기존 남색 계열 강조색이 주 색상으로 사용되지 않는다.
- PDF 파일이 정상 생성되고 파일 크기/수정 시간이 갱신된다.

## 작업 로그

- 2026-06-10: PDF 디자인 개선 계획을 작성했다.
- 2026-06-10: 남색 중심 스타일을 제거하고 warm neutral, muted green, muted amber 기반 스타일로 PDF를 다시 렌더링했다.
- 2026-06-10: master plan과 sprint plan PDF 첫 페이지에 제목, 설명, 문서 유형, 프로젝트 태그가 보이는 cover page를 추가했다.
- 2026-06-10: RAG 폴더의 PDF 2개와 브라우저 확인용 루트 복사본 2개를 최신 출력으로 갱신했다.
- 2026-06-10: 사용자가 초기 `operator_guide_rag_sprint_plan.pdf` 스타일 복원을 요청했다. 커버 페이지 없는 단순 문서형 PDF 스타일로 되돌린다.
- 2026-06-10: master/sprint PDF를 초기 단순 문서형 스타일로 재생성했다. 커버 페이지는 제거했고, 본문 H1 제목부터 시작한다.
- 2026-06-10: 사용자가 `Downloads/operator_guide_rag_sprint_plan.pdf`를 레퍼런스로 제공했다. 해당 파일은 초기 단순 문서형 PDF 스타일과 동일한 방향으로 판단하여, 최신 문서 내용은 유지하고 초기 렌더링 스타일을 유지한다.
- 2026-06-10: 사용자가 code block/flow block의 남색 배경을 밝은 회색 박스로 바꾸길 요청했다. 내용은 유지하고 `pre` 스타일만 light gray 계열로 조정해 PDF를 재생성한다.
- 2026-06-10: master/sprint PDF를 밝은 회색 code block 스타일로 재생성했다. RAG 흐름 박스와 env/code 예시 박스가 동일한 회색 계열로 출력된다.

## 트러블슈팅 로그

- 2026-06-10: 이전 Chrome file URL 확인에서 경로 인식 문제가 있었으므로, 생성 후 RAG 폴더 PDF와 루트 확인용 복사본을 함께 갱신한다.
- 2026-06-10: 초기 PDF 파일은 git에 추적되지 않은 생성물이어서 이전 바이너리 그대로 복원할 수 없다. 대신 처음 생성할 때 사용한 단순 HTML/CSS 렌더링 방식으로 재생성한다.
