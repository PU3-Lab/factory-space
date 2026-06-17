"""오퍼레이터 가이드(Operator Guide) 에이전트용 CSV 저장소 호환 내보내기 모듈입니다.

초보자용 설명:
    이 파일은 기존 코드 호환성을 위해 `CsvManualQARepository` 클래스를 패키지 외부로 노출(export)하는 역할을 합니다.
"""

from agents.operator_guide.csv_repository import CsvManualQARepository

__all__ = ["CsvManualQARepository"]
