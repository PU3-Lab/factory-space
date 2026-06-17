# 2026-06-17 operator_guide 문서 정리 계획

## 목적

operator_guide 관련 문서가 `manual_qa` 이름과 긴 파일명으로 흩어져 있어, 날짜순으로 찾기 어렵고 문서 역할을 구분하기 어렵다.

이번 작업에서는 사용자에게 직접 공유되는 `docs/` 문서를 먼저 정리한다.

## 정리 기준

```text
docs/operator_guide/YYYY-MM-DD_operator_guide_역할.md
docs/operator_guide/YYYY-MM-DD_operator_guide_역할.pdf
docs/operator_guide/YYYY-MM-DD_operator_guide_역할.html
```

원칙은 다음과 같다.

- 파일명에서 `manual_qa` 문구를 제거한다.
- 파일명은 문서 생성 날짜와 `operator_guide`로 시작한다.
- `docs/operator_guide/` 폴더에 모아 날짜순으로 정렬되게 한다.
- 내부 구현 계획 문서(`backend/docs/plans/operator_guide_rag`)는 링크 깨짐 위험이 크므로 이번 작업에서는 이름을 대량 변경하지 않고, 필요한 참조만 보정한다.
- 문서 이름 변경은 가능한 경우 `git mv`로 처리해 이력을 보존한다.

## 변경 대상

```text
기존 Manual Q&A 접두사로 시작하던 md/pdf/html 문서
기존 operator_guide 접두사로 docs 루트에 있던 md 문서
```

## 완료 기준

- `docs/operator_guide/` 아래에서 날짜순으로 문서를 확인할 수 있다.
- 파일명에 `manual_qa`가 남지 않는다.
- 주요 문서 참조가 새 경로를 가리킨다.
- 날짜별 문서 인덱스가 존재한다.

## 작업 로그

- 2026-06-17: git 이력 기준으로 주요 operator_guide 문서 생성 날짜를 확인했다.
- 2026-06-17: 사용자 공유용 문서를 `docs/operator_guide/` 폴더로 이동하고 날짜 prefix를 붙이는 방향으로 정리했다.

## 트러블슈팅 로그

- 2026-06-17: 일부 오래된 문서는 git 추적 대상이 아니어서 `git mv`가 실패했다. 해당 파일은 일반 이동 후 새 파일로 추가되는 방식으로 정리한다.
