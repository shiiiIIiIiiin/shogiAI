from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional, Tuple

import cshogi


@dataclass
class SearchResult:
    move: Optional[int]
    score: int
    nodes: int
    depth: int


class ShogiEngine:
    def __init__(self) -> None:
        self.nodes = 0

    def search(self, board: cshogi.Board, depth: int) -> SearchResult:
        self.nodes = 0
        score, move = self._negamax(board, depth, -10**9, 10**9)
        return SearchResult(move=move, score=score, nodes=self.nodes, depth=depth)

    def _negamax(self, board: cshogi.Board, depth: int, alpha: int, beta: int) -> Tuple[int, Optional[int]]:
        self.nodes += 1

        if depth == 0 or board.is_game_over():
            return self.evaluate(board), None

        best_move: Optional[int] = None
        best_score = -10**9

        for move in self._generate_moves(board):
            board.push(move)
            score, _ = self._negamax(board, depth - 1, -beta, -alpha)
            score = -score
            board.pop()

            if score > best_score:
                best_score = score
                best_move = move

            if score > alpha:
                alpha = score
            if alpha >= beta:
                break

        return best_score, best_move

    def _generate_moves(self, board: cshogi.Board) -> List[int]:
        """手順最適化: 取る手 → 王手 → その他の順で返す"""
        captures = []
        checks = []
        quiet = []

        for move in board.legal_moves:
            to_sq = cshogi.move_to(move)
            # 取る手（駒を食べる手）
            if board.piece(to_sq) != 0:
                captures.append(move)
            # 王手
            else:
                board.push(move)
                is_check = board.is_check()
                board.pop()
                if is_check:
                    checks.append(move)
                # その他
                else:
                    quiet.append(move)

        # 優先順: 取る手 → 王手 → その他
        return captures + checks + quiet

    def evaluate(self, board: cshogi.Board) -> int:
        # まずは駒割り評価（超シンプル）
        # 盤上と持ち駒の合計で評価
        material = 0

        for piece in board.pieces:
            if piece == 0:
                continue
            if 1 <= piece <= 14:
                material += self._piece_value(piece)
            elif 17 <= piece <= 30:
                material -= self._piece_value(piece - 16)

        black_hand, white_hand = board.pieces_in_hand
        for i, count in enumerate(black_hand, start=1):
            material += count * self._piece_value(i)
        for i, count in enumerate(white_hand, start=1):
            material -= count * self._piece_value(i)

        # 手番視点で返す
        if board.turn == cshogi.WHITE:
            material = -material

        return material

    def _piece_value(self, piece: int) -> int:
        #1:歩,2:香,3:桂,4:銀,5:金,6:角,7:飛,8-14:成り
        values = {
            1: 100,
            2: 300,
            3: 300,
            4: 400,
            5: 500,
            6: 700,
            7: 800,
            8: 500,
            9: 500,
            10: 500,
            11: 600,
            12: 500,
            13: 900,
            14: 900,
        }
        return values.get(piece, 0)
