from __future__ import annotations

import logging
import re

from fastapi import APIRouter, HTTPException
from fastapi.responses import FileResponse

from tts.settings import TTSSettings

logger = logging.getLogger("app.tts.router")

tts_router = APIRouter()

# 에이전트 허용 목록 (agent allowlist)
ALLOWED_AGENTS = {"operator_guide", "process_optimizer"}

# SHA-256 해시 정규식 (64자리 hex 소문자)
HASH_REGEX = re.compile(r"^[a-f0-9]{64}$")


@tts_router.get("/tts/{agent}/{audio_key}.mp3")
def serve_agent_audio(agent: str, audio_key: str) -> FileResponse:
    """합성된 오디오 파일을 보안 규칙에 따라 서빙하는 엔드포인트.

    초보자용 설명:
        전체 디렉터리를 마운트하는 방식은 의도하지 않은 파일 노출 위험이 있으므로,
        에이전트 이름(allowlist 검사)과 오디오 파일 고유 키(SHA-256 규격 검사)를 엄격히 필터링하고
        경로 탐색(path traversal) 공격을 차단한 상태에서 특정 오디오 파일만 리턴합니다.
    """
    # 1. 에이전트 허용 목록 검증
    if agent not in ALLOWED_AGENTS:
        logger.warning("비허가 에이전트 정적 오디오 서빙 요청 차단: agent=%s", agent)
        raise HTTPException(status_code=403, detail="Forbidden agent directory")

    # 2. 오디오 키(SHA-256) 형식 검증
    if not HASH_REGEX.match(audio_key):
        logger.warning("잘못된 오디오 키 서빙 요청 차단: key=%s", audio_key)
        raise HTTPException(status_code=400, detail="Invalid audio key format")

    # 3. 환경 설정을 로드하여 파일 저장 경로 획득
    settings = TTSSettings.from_env()

    # 4. 물리 경로 매핑 및 경로 탐색(path traversal) 방지
    base_dir = settings.storage_path.resolve()
    target_path = (base_dir / agent / f"{audio_key}.mp3").resolve()

    try:
        # target_path가 base_dir의 하위 경로인지 확인
        target_path.relative_to(base_dir)
    except ValueError:
        logger.warning("경로 탐색 공격 감지: agent=%s, key=%s", agent, audio_key)
        raise HTTPException(status_code=403, detail="Path traversal not allowed")

    # 5. 파일 존재 여부 검증
    if not target_path.is_file():
        logger.debug("요청한 오디오 파일을 찾을 수 없음: %s", target_path)
        raise HTTPException(status_code=404, detail="Audio file not found")

    return FileResponse(
        path=target_path,
        media_type="audio/mpeg",
        filename=f"{audio_key}.mp3",
    )
