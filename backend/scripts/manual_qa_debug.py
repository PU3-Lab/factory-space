"""Print Operator Guide Manual Q&A proto responses for representative questions."""

from __future__ import annotations

import json
from typing import Any

from agents.operator_guide.service import ManualQAService

QUESTIONS = [
    "제련기는 뭐야?",
    "철괴는 어떻게 만들어?",
    "기어 만들려면 뭐가 필요해?",
    "제련기가 왜 안 돌아가?",
    "우주 엘리베이터는 어떻게 업그레이드해?",
]


def answer_question(question: str) -> dict[str, Any]:
    result = ManualQAService().answer(question)
    return {
        "question": question,
        "final_answer": result.final_answer,
        "text": result.answer,
        "actions": [],
        "metadata": result.to_metadata(),
    }


def main() -> None:
    results = [answer_question(question) for question in QUESTIONS]
    print(json.dumps(results, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
