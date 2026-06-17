"""Tests for operator_guide multi-question decomposition."""

from __future__ import annotations

from agents.operator_guide.question_decomposer import decompose_question


def test_decomposer_keeps_single_question_as_one_sub_question() -> None:
    result = decompose_question("What is a crusher?")

    assert result.original_question == "What is a crusher?"
    assert result.is_multi_question is False
    assert [item.question for item in result.sub_questions] == ["What is a crusher?"]
    assert result.metadata == {
        "sub_question_count": 1,
        "max_sub_questions": 3,
        "truncated": False,
    }


def test_decomposer_splits_question_mark_and_korean_connector() -> None:
    result = decompose_question("분쇄기가 뭐야? 그리고 철괴를 만들려면 어떻게 해야 돼?")

    assert result.is_multi_question is True
    assert [item.question for item in result.sub_questions] == [
        "분쇄기가 뭐야?",
        "철괴를 만들려면 어떻게 해야 돼?",
    ]
    assert [item.index for item in result.sub_questions] == [1, 2]


def test_decomposer_limits_sub_questions_to_three() -> None:
    result = decompose_question(
        "What is a crusher? And how do I make iron ingot? "
        "Also why is the belt stopped? And what is copper ore?"
    )

    assert result.is_multi_question is True
    assert [item.question for item in result.sub_questions] == [
        "What is a crusher?",
        "how do I make iron ingot?",
        "why is the belt stopped?",
    ]
    assert result.metadata["sub_question_count"] == 3
    assert result.metadata["truncated"] is True
