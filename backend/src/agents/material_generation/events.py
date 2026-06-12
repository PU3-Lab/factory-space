"""Event publisher to distribute material synthesis lifecycle events."""

from __future__ import annotations

import logging
from concurrent.futures import ThreadPoolExecutor

logger = logging.getLogger(__name__)

# Bounded thread pool executor for processing visual assets in the background
_executor = ThreadPoolExecutor(max_workers=4, thread_name_prefix="VisualPipeline")


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
        from agents.material_generation.visual_pipeline import VisualAssetPipeline

        # Enqueue background asset texturing to the thread pool executor to prevent unbounded thread creation
        _executor.submit(
            VisualAssetPipeline.process_visual_asset,
            material_id,
            visual_prompt,
            category,
        )
        logger.info("Published MaterialCreated event for material: %s", material_id)
