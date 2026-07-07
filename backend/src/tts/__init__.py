"""Factory Space TTS (Text-to-Speech) 패키지.

초보자용 설명:
    이 패키지는 AI 에이전트(operator_guide, process_optimizer)의 답변 텍스트를
    음성으로 변환하여 게임 내에서 재생할 수 있게 돕는 모듈입니다.
    로컬/개발 환경(development)에서는 edge-tts를 사용하며,
    운영 환경(production)에서는 ElevenLabs API를 활용합니다.
"""

from __future__ import annotations
