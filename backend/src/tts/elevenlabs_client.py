"""ElevenLabs API를 사용해 오디오 합성을 수행하는 클라이언트 모듈.

초보자용 설명:
    이 모듈은 운영 환경에서 고품질 음성 합성을 지원하는 ElevenLabs의 REST API를 호출합니다.
    추가 패키지 의존성(requests 등) 없이 가볍게 호출할 수 있도록 Python 표준 라이브러리인
    `urllib.request` 모듈을 이용해 HTTP POST 요청을 보냅니다.
    보안 가이드라인에 따라 API 키와 합성 대상 텍스트는 로그에 노출되지 않도록 처리합니다.
"""

from __future__ import annotations

import json
import logging
import urllib.error
import urllib.request
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from tts.settings import TTSSettings

logger = logging.getLogger("app.tts.elevenlabs")


class ElevenLabsClient:
    """ElevenLabs Text-to-Speech HTTP API와 통신하여 오디오 바이너리를 받아오는 동기 클라이언트."""

    provider = "elevenlabs"

    def __init__(self, settings: TTSSettings) -> None:
        self.settings = settings

    def synthesize(self, text: str) -> bytes:
        """ElevenLabs REST API를 호출하여 입력 텍스트에 대한 MP3 오디오 데이터 바이트를 반환합니다."""
        if not self.settings.api_key:
            raise RuntimeError("ElevenLabs API 호출에 필요한 API 키가 설정되지 않았습니다.")

        # API endpoint 구성 (stream 방식 POST 요청)
        url = (
            f"https://api.elevenlabs.io/v1/text-to-speech/{self.settings.voice_id}/stream"
            f"?output_format={self.settings.output_format}"
        )
        
        headers = {
            "xi-api-key": self.settings.api_key,
            "Content-Type": "application/json",
            "Accept": "audio/mpeg",
        }
        
        # 모델명 및 상세 목소리 환경 설정
        payload_data = {
            "text": text,
            "model_id": self.settings.model_id,
            "voice_settings": {
                "stability": 0.5,
                "similarity_boost": 0.8,
                "speed": 1.0,
            },
        }
        
        req = urllib.request.Request(
            url,
            data=json.dumps(payload_data).encode("utf-8"),
            headers=headers,
            method="POST",
        )

        try:
            # 표준 라이브러리로 타임아웃을 적용해 블로킹 요청 실행
            with urllib.request.urlopen(req, timeout=self.settings.timeout_seconds) as response:
                status_code = response.status
                if status_code == 200:
                    return response.read()
                else:
                    raise RuntimeError(f"예상치 못한 HTTP 응답 상태: {status_code}")
                    
        except urllib.error.HTTPError as exc:
            # 보안 수칙: 로그에 사용자 텍스트 및 API 키가 절대 찍히지 않고 HTTP 코드만 로깅
            logger.error("ElevenLabs HTTP error occurred: code=%d, reason=%s", exc.code, exc.reason)
            raise RuntimeError(f"ElevenLabs HTTP error: code={exc.code}") from exc
        except Exception as exc:
            logger.error("ElevenLabs client unexpected failure: %s", str(exc))
            raise RuntimeError(f"ElevenLabs synthesis failed: {exc}") from exc
