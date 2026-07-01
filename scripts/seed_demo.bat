@echo off
@rem Windows CMD UTF-8 인코딩 설정 (한글 주석 및 출력 깨짐 방지)
chcp 65001 > nul

@rem 스크립트 파일이 위치한 디렉터리 경로 설정 (scripts\)
set DIR=%~dp0

@rem backend/.venv 가상환경 폴더 확인 후 factory_space.db 데모 시드 스크립트 실행
if exist "%DIR%..\backend\.venv" (
    "%DIR%..\backend\.venv\Scripts\python.exe" "%DIR%..\backend\scripts\seed_demo.py" %*
) else (
    python "%DIR%..\backend\scripts\seed_demo.py" %*
)
