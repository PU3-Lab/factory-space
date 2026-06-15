"""재료 합성 생명주기 이벤트를 배포하기 위한 이벤트 게시자(Publisher)입니다."""

from __future__ import annotations

import logging
from concurrent.futures import ThreadPoolExecutor

from agents.material_generation.visual_pipeline import VisualAssetPipeline

logger = logging.getLogger(__name__)

# 백그라운드에서 시각적 자산을 처리하기 위한 바운디드(Bounded) 스레드 풀 실행자
_executor: ThreadPoolExecutor | None = ThreadPoolExecutor(
    max_workers=4, thread_name_prefix="VisualPipeline"
)
_shutdown = False


class MaterialEventPublisher:
    """텍스처링과 같은 후처리 워크플로를 디커플링하기 위해 인메모리 이벤트를 디스패치합니다."""

    @classmethod
    def publish_material_created(
        cls,
        material_id: str,
        visual_prompt: str,
        category: str,
    ) -> None:
        """MaterialCreated 이벤트를 발생시키고 백그라운드에서 시각적 자원 생성을 실행합니다."""
        global _executor, _shutdown
        if _shutdown or _executor is None:
            logger.warning(
                "MaterialEventPublisher: Attempted to publish material created event after shutdown: %s",
                material_id,
            )
            return

        # 제한 없는 스레드 생성을 방지하기 위해 백그라운드 자산 텍스처링 작업을 스레드 풀 실행자에 큐잉합니다.
        try:
            _executor.submit(
                VisualAssetPipeline.process_visual_asset,
                material_id,
                visual_prompt,
                category,
            )
            logger.info("Published MaterialCreated event for material: %s", material_id)
        except RuntimeError as exc:
            logger.error(
                "MaterialEventPublisher: Failed to submit background job (executor is shutdown): %s",
                exc,
            )

    @classmethod
    def shutdown_executor(cls, wait: bool = True) -> None:
        """백그라운드 스레드 풀 실행자를 영구적으로 종료합니다."""
        global _executor, _shutdown
        _shutdown = True
        if _executor is not None:
            _executor.shutdown(wait=wait)
            _executor = None
        logger.info("MaterialEventPublisher: Background executor permanently shutdown.")

    @classmethod
    def reset_executor(cls, wait: bool = True) -> None:
        """현재 실행자를 종료하고 새 실행자를 초기화합니다 (테스트 / 재로드용)."""
        global _executor, _shutdown
        if _executor is not None:
            _executor.shutdown(wait=wait)
        _executor = ThreadPoolExecutor(
            max_workers=4, thread_name_prefix="VisualPipeline"
        )
        _shutdown = False
        logger.info("MaterialEventPublisher: Background executor reset.")

    @classmethod
    def wait_for_jobs(cls) -> None:
        """현재 제출된 모든 작업이 완료될 때까지 대기하고 실행자를 재설정합니다 (테스트용)."""
        cls.reset_executor(wait=True)
