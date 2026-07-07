"""TTS 요청 및 결과 검증을 위한 Pydantic 스키마 정의.

초보자용 설명:
    이 파일은 TTS 작업을 요청할 때 어떤 데이터(에이전트 이름, 응답 페이로드)가 들어와야 하는지,
    그리고 TTS 작업 결과(재생할 오디오 URL, 캐싱 여부, 오류 정보 등)가 어떤 규격으로 반환되는지 정의합니다.
"""

from __future__ import annotations

from typing import Any

from pydantic import BaseModel, Field


class TTSRequest(BaseModel):
    """에이전트 응답을 기반으로 TTS를 생성하기 위한 요청 스키마."""
    
    agent: str = Field(..., description="요청한 에이전트 식별자 (예: operator_guide, process_optimizer)")
    payload: dict[str, Any] = Field(..., description="에이전트가 생성한 원본 응답 페이로드 객체")


class TTSMetadata(BaseModel):
    """최종 에이전트 응답 페이로드의 tts 필드에 담길 메타데이터 구조."""
    
    status: str = Field(..., description="TTS 상태 ('ready', 'disabled', 'skipped', 'failed')")
    provider: str = Field(..., description="TTS 오디오를 생성한 공급자명 ('edge_tts', 'elevenlabs')")
    audio_url: str | None = Field(None, description="오디오 파일의 정적 웹 URL 경로")
    content_type: str | None = Field(None, description="오디오 포맷 MIME 타입 (예: audio/mpeg)")
    text_hash: str | None = Field(None, description="캐싱 기준이 된 본문 데이터의 고유 해시값")
    voice_id: str | None = Field(None, description="사용한 목소리/성우 ID")
    model_id: str | None = Field(None, description="사용한 AI TTS 모델 ID")
    cached: bool | None = Field(None, description="캐시 저장소에서 조회한 응답인지 여부")
    error_code: str | None = Field(None, description="실패 시 상세 원인 에러코드")
    text: str | None = Field(None, description="합성 대상 원본 텍스트")
