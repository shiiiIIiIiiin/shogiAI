from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Tuple

import cshogi


@dataclass
class PositionSample:
    sfen: str
    move_usi: str


def read_kif_folder(folder: Path) -> Iterable[PositionSample]:
    """
    KIF/CSA を想定した読み込みの雛形。
    実運用では cshogi.KIFParser / CSAParser を使う。
    """
    for file in folder.rglob("*.kif"):
        # TODO: cshogi.KIFParser に置き換える
        _ = file
        continue


def board_to_input(board: cshogi.Board) -> List[int]:
    """簡易特徴量（後でNN入力に置き換え）"""
    # ダミー特徴量: 盤面のSFEN長
    return [len(board.sfen())]
