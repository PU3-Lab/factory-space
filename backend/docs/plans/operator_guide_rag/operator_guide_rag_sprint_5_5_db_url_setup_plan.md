# operator_guide RAG Sprint 5.5 DB URL 설정 계획

## 목표

Sprint 5에서 구현한 ingestion script를 실제 PostgreSQL + pgvector 환경에 연결한다.

현재 코드와 테스트는 준비되었지만, 로컬 환경에는 `DATABASE_URL` 또는 `FACTORY_DATABASE_URL`이 없어 `--dry-run` 실행이 막힌다.

## 현재 확인 결과

```text
Docker CLI: 없음
psql CLI: 없음
localhost:5432: 연결 실패
backend/.env.prod: OPENAI_API_KEY만 설정됨
backend/.env / backend/.env.prod: git ignore 대상
```

## 구현 범위

- Alembic migration이 `DATABASE_URL`뿐 아니라 `FACTORY_DATABASE_URL`도 읽도록 수정
- 로컬 pgvector 실행용 compose 파일 추가
- `.env.example`에 RAG DB URL 예시 추가
- Sprint 5 문서의 실행 명령어를 `--env-file .env` 기준으로 명확히 수정
- 실제 DB가 없는 경우 dry-run은 연결 실패가 정상이며, DB 실행 후 재시도한다.

## 추천 로컬 DB 방식

Docker Desktop 설치 후 pgvector 이미지를 사용한다.

```powershell
cd C:\factory-space\backend
docker compose -f docker-compose.rag.yml up -d
uv run --env-file .env alembic upgrade head
uv run --env-file .env python scripts/ingest_manual_rag.py --dry-run
```

## 로컬 `.env` 예시

```env
FACTORY_DATABASE_URL=postgresql+psycopg://factory_space:factory_space@127.0.0.1:5433/factory_space?connect_timeout=5
FACTORY_EMBEDDING_PROVIDER=openai
FACTORY_EMBEDDING_MODEL=text-embedding-3-small
FACTORY_EMBEDDING_DIMENSIONS=1536
```

## 완료 기준

- Alembic env가 `FACTORY_DATABASE_URL` fallback을 지원한다.
- 로컬 DB 실행 방법이 문서화된다.
- DB가 실행된 상태에서 migration과 dry-run을 실행할 수 있는 명령어가 문서에 남는다.

## 작업 로그

- 2026-06-10: Sprint 5 dry-run 실행 중 DB URL 미설정 오류를 확인했다.
- 2026-06-10: Alembic이 `FACTORY_DATABASE_URL`을 읽도록 fallback 계획을 추가했다.
- 2026-06-10: 로컬 pgvector 실행용 `docker-compose.rag.yml` 구성을 추가했다.
- 2026-06-10: 로컬 `.env`에 RAG DB URL과 embedding 설정을 추가했다.
- 2026-06-10: Alembic migration을 실제 실행해 DB 서버 미기동으로 연결 timeout이 발생하는 것을 확인했다.
- 2026-06-10: DB 미기동 상태에서 ingestion dry-run이 긴 traceback 대신 실행 안내 메시지를 출력하도록 보완했다.
- 2026-06-10: Docker/pgvector DB가 127.0.0.1:5433에서 응답하는 것을 확인했고, Alembic migration을 정상 실행했다.
- 2026-06-10: ingestion dry-run 결과 `inserted=142, updated=0, skipped=0, deactivated=0, failed=0`을 확인했다.

## 트러블슈팅 로그

- 2026-06-10: Docker, psql, localhost:5432가 모두 없어 현재 PC에서는 DB 서버를 먼저 준비해야 한다.
- 2026-06-10: DB 서버가 없는 상태에서 Alembic이 연결을 기다리다 timeout되어, local DB URL에 `connect_timeout=5`를 추가했다.
- 2026-06-10: localhost:5433 포트도 닫혀 있어 `docker compose -f docker-compose.rag.yml up -d` 또는 별도 PostgreSQL/pgvector 설치가 필요하다.
- 2026-06-10: dry-run도 저장소의 기존 hash를 읽어야 하므로 DB 서버가 켜져 있어야 실행 가능하다는 점을 확인했다.
- 2026-06-10: dry-run 출력의 실제 반영 명령어가 env 파일을 누락해 `uv run --env-file .env ...` 형태로 수정했다.
