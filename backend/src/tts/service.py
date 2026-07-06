"""TTS 설정, 파일 캐시, 그리고 클라이언트를 연계하여 음성을 최종 합성하는 비즈니스 로직 서비스 모듈.

초보자용 설명:
    이 모듈의 `TTSService`는 파이프라인에서 요청이 들어왔을 때,
    1) 설정(enabled 여부)을 파악하고 비활성이면 중단,
    2) 응답 문구 정제(text_selection) 및 캐시 검사(storage),
    3) 캐시 미스 시 적절한 클라이언트(edge-tts 또는 ElevenLabs)를 통해 음성을 합성하고 디스크에 저장합니다.
    최종 결과는 항상 예외로 에이전트 실행이 실패하지 않도록 규격화된 상태('ready', 'failed', 'disabled' 등)로 리턴합니다.
"""

from __future__ import annotations

import logging
from typing import TYPE_CHECKING

from tts.edge_tts_client import EdgeTTSClient
from tts.elevenlabs_client import ElevenLabsClient
from tts.schemas import TTSMetadata, TTSRequest
from tts.settings import TTSSettings
from tts.storage import TTSAudioStorage
from tts.text_selection import select_tts_text

if TYPE_CHECKING:
    from tts.storage import StorageResult

logger = logging.getLogger("app.tts.service")


class TTSService:
    """TTS 설정, 캐시 관리, 그리고 실제 합성 클라이언트를 총괄하는 주 서비스 클래스."""

    def __init__(self, settings: TTSSettings | None = None, provider: object | None = None) -> None:
        self.settings = settings or TTSSettings.from_env()
        self.storage = TTSAudioStorage(
            storage_path=self.settings.storage_path,
            public_base_url=self.settings.public_base_url,
        )
        
        # 외부에서 모의(Mock) 프로바이더가 주입되면 이를 우선 사용하고, 없으면 설정에 맞춰 생성
        if provider is not None:
            self.provider = provider
        else:
            if self.settings.provider == "elevenlabs":
                self.provider = ElevenLabsClient(self.settings)
            else:
                self.provider = EdgeTTSClient(self.settings)

    @classmethod
    def from_env(cls) -> TTSService:
        """환경 변수로부터 TTSService 인스턴스를 빌드합니다."""
        return cls(TTSSettings.from_env())

    def synthesize_for_payload(self, request: TTSRequest) -> TTSMetadata:
        """에이전트 응답 요청을 수신하여 캐시를 확인한 후 오디오를 합성하고 결과를 반환합니다."""
        provider_name = self.settings.provider

        # 1. TTS 기능 자체가 비활성화되어 있는 경우
        if not self.settings.enabled:
            return TTSMetadata(
                status="disabled",
                provider=provider_name,
                error_code=self.settings.disabled_reason,
            )

        # 2. 페이로드에서 TTS 대상 한글 문장 추출
        text = select_tts_text(
            agent=request.agent,
            payload=request.payload,
            max_chars=self.settings.max_chars,
        )
        if not text:
            # 읽어줄 플레이어용 문장이 없는 경우 skip
            return TTSMetadata(
                status="skipped",
                provider=provider_name,
            )

        # 3. 캐시 저장용 고유 키 생성
        key = self.storage.build_key(
            agent=request.agent,
            text=text,
            voice_id=self.settings.voice_id,
            model_id=self.settings.model_id,
            output_format=self.settings.output_format,
        )

        # 4. 캐시 확인 (디스크 상에 파일이 존재하는지)
        try:
            cached_result = self.storage.get_audio_result(request.agent, key)
            if cached_result is not None:
                return TTSMetadata(
                    status="ready",
                    provider=provider_name,
                    audio_url=cached_result.audio_url,
                    content_type="audio/mpeg",
                    text_hash=key,
                    voice_id=self.settings.voice_id,
                    model_id=self.settings.model_id,
                    cached=True,
                    text=text,
                )
        except Exception as exc:
            logger.warning("TTS 캐시 조회 중 오류 발생: %s", exc)

        # 5. 캐시 미스 - 실제 오디오 합성 호출 및 저장
        try:
            import concurrent.futures
            
            executor = concurrent.futures.ThreadPoolExecutor(max_workers=1)
            try:
                future = executor.submit(self.provider.synthesize, text)
                audio_bytes = future.result(timeout=self.settings.timeout_seconds)
            finally:
                executor.shutdown(wait=False)

            save_result: StorageResult = self.storage.write_audio(
                agent=request.agent,
                key=key,
                audio_bytes=audio_bytes,
                extension="mp3",
            )
            
            return TTSMetadata(
                status="ready",
                provider=provider_name,
                audio_url=save_result.audio_url,
                content_type="audio/mpeg",
                text_hash=key,
                voice_id=self.settings.voice_id,
                model_id=self.settings.model_id,
                cached=False,
                text=text,
            )
        except Exception as exc:
            logger.error("TTS 오디오 합성 또는 저장 실패: %s", exc)
            
            # Timeout 판별
            import socket
            import urllib.error
            is_timeout = False
            if isinstance(exc, TimeoutError):
                is_timeout = True
            elif isinstance(exc, socket.timeout):
                is_timeout = True
            elif isinstance(exc, urllib.error.URLError) and isinstance(exc.reason, socket.timeout):
                is_timeout = True
            elif "timeout" in str(exc).lower():
                is_timeout = True
                
            error_code = "TTS_TIMEOUT" if is_timeout else "TTS_PROVIDER_ERROR"
            
            return TTSMetadata(
                status="failed",
                provider=provider_name,
                error_code=error_code,
            )
