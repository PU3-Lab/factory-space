"""Event publisher to distribute material synthesis lifecycle events."""

from __future__ import annotations

import logging
from concurrent.futures import ThreadPoolExecutor

from agents.material_generation.visual_pipeline import VisualAssetPipeline

logger = logging.getLogger(__name__)

# Bounded thread pool executor for processing visual assets in the background
_executor: ThreadPoolExecutor | None = ThreadPoolExecutor(
    max_workers=4, thread_name_prefix="VisualPipeline"
)
_shutdown = False


class MaterialEventPublisher:
    """Dispatches in-memory events to decouple post-processing workflows like texturing."""

    @classmethod
    def publish_material_created(
        cls,
        material_id: str,
        visual_prompt: str,
        category: str,
    ) -> None:
        """Fire MaterialCreated event and run visual asset generation in the background."""
        global _executor, _shutdown
        if _shutdown or _executor is None:
            logger.warning(
                "MaterialEventPublisher: Attempted to publish material created event after shutdown: %s",
                material_id,
            )
            return

        # Enqueue background asset texturing to the thread pool executor to prevent unbounded thread creation
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
        """Permanently shutdown the background thread pool executor."""
        global _executor, _shutdown
        _shutdown = True
        if _executor is not None:
            _executor.shutdown(wait=wait)
            _executor = None
        logger.info("MaterialEventPublisher: Background executor permanently shutdown.")

    @classmethod
    def reset_executor(cls, wait: bool = True) -> None:
        """Shutdown the current executor and initialize a new one (for testing / reload)."""
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
        """Wait for all currently submitted jobs to complete and reset the executor (for testing)."""
        cls.reset_executor(wait=True)
