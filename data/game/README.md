# Manual Q&A 최신 CSV - 한글 컬럼명 버전

이 파일은 첨부된 최신 CSV 5개를 기준으로, 상단 컬럼명을 한글로 유지한 공유/검토용 CSV 세트입니다.

## 파일 구성

- resources.csv
- recipes.csv
- equipment.csv
- troubleshooting_rules.csv
- action_policy.csv

## 행 수

- resources.csv: 57개
- recipes.csv: 30개
- equipment.csv: 12개
- troubleshooting_rules.csv: 19개
- action_policy.csv: 24개

## 적용 기준

- 컬럼명은 한글로 유지했습니다.
- ID 값은 추적과 코드 연동을 위해 영어 ID를 유지했습니다.
- 관계값은 한글명과 영어 ID를 함께 표시한 형식을 유지했습니다.
  예: 제련기(equipment_smelter), 철광석(resource_iron_ore)
- 파일 인코딩은 Excel에서 한글이 깨지지 않도록 UTF-8 with BOM으로 저장했습니다.

## 주의

이 버전은 사람이 읽고 RAG 문서 생성에 활용하기 좋은 한글 컬럼명 버전입니다.
운영 코드에서 영어 컬럼명을 기준으로 파싱하고 있다면, 코드 수정 없이 바로 대체하면 안 됩니다.
