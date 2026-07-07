"""edge-tts 라이브러리를 사용해 오디오 합성을 수행하는 클라이언트 모듈.

초보자용 설명:
    이 모듈은 로컬/개발 환경에서 ElevenLabs API 키 없이도 빠르게 음성을 만들 수 있게
    Microsoft Edge의 무료 TTS 웹 소켓 서비스를 활용하는 edge-tts 라이브러리를 래핑합니다.
    동기(Synchronous) 실행 환경(AgentPipeline)에서 안전하게 비동기(Async) edge-tts 함수를 실행하도록
    이벤트 루프 제어와 임시 스레드 실행 방식을 지원합니다.
"""

from __future__ import annotations

import asyncio
import concurrent.futures
import tempfile
from pathlib import Path
from typing import TYPE_CHECKING

import edge_tts

if TYPE_CHECKING:
    from tts.settings import TTSSettings


class EdgeTTSClient:
    """edge-tts 서비스 API를 호출하여 동기식으로 음성 오디오를 생성하는 클라이언트."""

    provider = "edge_tts"

    def __init__(self, settings: TTSSettings) -> None:
        self.settings = settings

    async def _async_synthesize(self, text: str, temp_path: Path) -> None:
        """edge-tts의 Communicate 객체를 통해 비동기로 오디오 생성을 요청합니다."""
        communicate = edge_tts.Communicate(
            text=text,
            voice=self.settings.voice_id,
            rate=self.settings.edge_rate,
            volume=self.settings.edge_volume,
            pitch=self.settings.edge_pitch,
        )
        # 2차 재리뷰 보강: running/no-running loop 양쪽 모두 설정된 timeout을 적용하도록 wait_for 래핑
        await asyncio.wait_for(
            communicate.save(str(temp_path)),
            timeout=self.settings.timeout_seconds,
        )

    def synthesize(self, text: str) -> bytes:
        """비동기 edge-tts 호출을 동기식으로 변환하여 최종 음성 바이너리 데이터를 반환합니다."""
        try:
            # 임시 파일 경로 생성
            with tempfile.TemporaryDirectory() as tmpdir:
                temp_path = Path(tmpdir) / "edge_tts_out.mp3"
                
                # 현재 스레드에 이미 활성화된 asyncio 이벤트 루프가 있는지 체크
                try:
                    loop = asyncio.get_running_loop()
                except RuntimeError:
                    loop = None

                if loop is not None and loop.is_running():
                    # 이벤트 루프가 작동 중이면, 별도 스레드 풀에서 독립된 루프를 열고 실행 (중첩 루프 오류 방지)
                    executor = concurrent.futures.ThreadPoolExecutor(max_workers=1)
                    try:
                        def _run_in_new_thread() -> None:
                            new_loop = asyncio.new_event_loop()
                            asyncio.set_event_loop(new_loop)
                            try:
                                new_loop.run_until_complete(self._async_synthesize(text, temp_path))
                            finally:
                                new_loop.close()
                        
                        future = executor.submit(_run_in_new_thread)
                        # 타임아웃 적용하여 대기
                        future.result(timeout=self.settings.timeout_seconds)
                    finally:
                        executor.shutdown(wait=False)
                else:
                    # 루프가 없으면 asyncio.run()으로 즉시 동기 실행
                    asyncio.run(self._async_synthesize(text, temp_path))

                if not temp_path.exists() or temp_path.stat().st_size == 0:
                    raise RuntimeError("edge-tts가 음성 오디오를 생성하지 못했습니다 (결과 파일 비어있음).")

                return temp_path.read_bytes()

        except TimeoutError as exc:
            # 5차 재리뷰 보강: 타임아웃 예외는 원래 타입을 유지해 상위 서비스가 타임아웃으로 인식하게 함
            raise TimeoutError(f"EdgeTTS request timed out: {exc}") from exc
        except Exception as exc:
            # edge-tts 관련 NoAudioReceived, WebSocketError 등을 공통 런타임 에러로 래핑하여 상위에 전달
            raise RuntimeError(f"EdgeTTS provider error during synthesis: {exc}") from exc
