"""TTS 생성 오디오 파일을 디스크 캐시에 저장하고 정적 URL을 생성하는 모듈.

초보자용 설명:
    매번 동일한 텍스트에 대해 TTS API를 호출하면 중복된 리소스를 낭비하고 비용이 발생합니다.
    이 모듈은 텍스트와 성우 설정의 해시값을 파일명 삼아 오디오 파일을 디바이스 디스크에 캐싱하고,
    클라이언트가 접근할 수 있는 공개 웹 주소(URL)를 매핑해 줍니다.
"""

from __future__ import annotations

import hashlib
import os
import tempfile
from pathlib import Path
from typing import NamedTuple


class StorageResult(NamedTuple):
    """저장 작업 완료 후 디스크 경로와 접근 URL을 담는 결과 객체."""
    
    path: Path
    audio_url: str


class TTSAudioStorage:
    """에이전트별 오디오 파일을 저장하고 캐시 키를 관리하는 클래스."""

    # 보안 및 올바른 디렉터리 분리를 위한 에이전트 허용 목록
    ALLOWED_AGENTS = {"operator_guide", "process_optimizer"}

    def __init__(self, storage_path: Path | str, public_base_url: str = "/tts") -> None:
        self.storage_path = Path(storage_path).resolve()
        self.public_base_url = public_base_url.rstrip("/")

    def build_key(
        self,
        agent: str,
        text: str,
        voice_id: str,
        model_id: str,
        output_format: str,
    ) -> str:
        """텍스트와 메타데이터의 조합을 고유한 sha256 해시값으로 변환합니다.
        
        해시 충돌 방지를 위해 각 구분을 널 문자(\\0)로 조인합니다.
        """
        raw_key = f"{agent}\0{voice_id}\0{model_id}\0{output_format}\0{text}"
        return hashlib.sha256(raw_key.encode("utf-8")).hexdigest()

    def _validate_params(self, agent: str, key: str, extension: str) -> None:
        """7차 재리뷰 보강: 허용된 에이전트, lowercase 64-character SHA-256 hex key, 'mp3' 확장자만 허용합니다."""
        if agent not in self.ALLOWED_AGENTS:
            raise ValueError(f"지원하지 않는 에이전트 대상입니다: {agent}")
        
        # 64자리 소문자 hex 해시값 형식 체크 (path traversal 원천 방지)
        import re
        if not re.match(r"^[a-f0-9]{64}$", key):
            raise ValueError(f"잘못된 캐시 키 형식입니다: {key}")
            
        # 오직 mp3 확장자만 허용
        if extension.lower() != "mp3":
            raise ValueError(f"지원하지 않는 파일 확장자입니다: {extension}")

    def get_audio_result(self, agent: str, key: str, extension: str = "mp3") -> StorageResult | None:
        """기존 캐시에 파일이 존재하는 경우 StorageResult를 반환합니다."""
        self._validate_params(agent, key, extension)

        target_path = self.storage_path / agent / f"{key}.{extension}"
        if target_path.exists():
            audio_url = f"{self.public_base_url}/{agent}/{key}.{extension}"
            return StorageResult(path=target_path, audio_url=audio_url)
        return None

    def write_audio(
        self,
        agent: str,
        key: str,
        audio_bytes: bytes,
        extension: str = "mp3",
    ) -> StorageResult:
        """오디오 바이너리를 임시 파일에 먼저 쓴 후 실제 위치로 이동(원자적 쓰기)하여 저장합니다."""
        self._validate_params(agent, key, extension)

        # 에이전트별 서브 디렉터리 경로 생성
        agent_dir = self.storage_path / agent
        agent_dir.mkdir(parents=True, exist_ok=True)

        target_path = agent_dir / f"{key}.{extension}"

        # 임시 파일을 만든 후 교체하여 부분적 작성 상태에서 읽히는 위험을 방지 (atomic write)
        fd, temp_path_str = tempfile.mkstemp(dir=str(agent_dir), suffix=".tmp")
        try:
            with os.fdopen(fd, "wb") as f:
                f.write(audio_bytes)
            
            # 원자적으로 파일 교체
            os.replace(temp_path_str, str(target_path))
        except Exception:
            if os.path.exists(temp_path_str):
                os.remove(temp_path_str)
            raise

        audio_url = f"{self.public_base_url}/{agent}/{key}.{extension}"
        return StorageResult(path=target_path, audio_url=audio_url)
