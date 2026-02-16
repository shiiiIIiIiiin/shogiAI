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
        
        # デバッグ: 返された手を確認
        if move is None:
            print(f"DEBUG: _negamax returned None", flush=True)
            # フォールバック：最初の合法手を使う
            legal_moves = list(board.legal_moves)
            if legal_moves:
                move = legal_moves[0]
                move_usi = cshogi.move_to_usi(move)
                print(f"DEBUG: FALLBACK - using first legal move: {move_usi}", flush=True)
            else:
                print(f"DEBUG: ERROR - no legal moves available!", flush=True)
        elif not board.is_legal(move):
            print(f"DEBUG: WARNING - illegal move detected!", flush=True)
            legal_moves = list(board.legal_moves)
            if legal_moves:
                move = legal_moves[0]
                move_usi = cshogi.move_to_usi(move)
                print(f"DEBUG: FALLBACK - using first legal move: {move_usi}", flush=True)
        else:
            move_usi = cshogi.move_to_usi(move)
            print(f"DEBUG: Selected: {move_usi}, Score: {score}, Nodes: {self.nodes}", flush=True)
        
        return SearchResult(move=move, score=score, nodes=self.nodes, depth=depth)

    def _negamax(self, board: cshogi.Board, depth: int, alpha: int, beta: int) -> Tuple[int, Optional[int]]:
        self.nodes += 1

        if depth == 0:
            # 静止探索：キャプチャーが続く場合のみ深く探索
            return self._quiescence(board, 0, alpha, beta), None

        if board.is_game_over():
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

    def _quiescence(self, board: cshogi.Board, depth: int, alpha: int, beta: int) -> int:
        """静止探索：キャプチャーが続く局面のみ深く探索"""
        self.nodes += 1
        
        if board.is_game_over():
            return self.evaluate(board)
        
        # 現在の局面の評価
        stand_pat = self.evaluate(board)
        if stand_pat >= beta:
            return beta
        if stand_pat > alpha:
            alpha = stand_pat
        
        # キャプチャーのみ探索（最大3手分）
        best_score = stand_pat
        if depth < 3:
            for move in self._generate_moves(board):
                # キャプチャーでない手はスキップ
                to_sq = cshogi.move_to(move)
                if board.piece(to_sq) == 0:
                    continue
                
                board.push(move)
                score = -self._quiescence(board, depth + 1, -beta, -alpha)
                board.pop()
                
                if score > best_score:
                    best_score = score
                
                if score > alpha:
                    alpha = score
                if alpha >= beta:
                    break
        
        return best_score

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
        # 駒割り評価
        material = 0

        # 盤上の駒を評価
        for sq in range(81):
            piece = board.piece(sq)
            if piece == 0:
                continue
            
            piece_type = board.piece_type(sq)
            value = self._piece_value(piece_type)
            
            # 先手の駒なら加算、後手の駒なら減算
            # pieceの値: 先手=1-14, 後手=17-30
            if piece < 16:
                material += value
            else:
                material -= value

        # 持ち駒を評価
        black_hand, white_hand = board.pieces_in_hand
        for i, count in enumerate(black_hand, start=1):
            material += count * self._piece_value(i)
        for i, count in enumerate(white_hand, start=1):
            material -= count * self._piece_value(i)

        # 手番視点で返す
        if board.turn == cshogi.WHITE:
            material = -material

        return material

    def _piece_value(self, piece_type: int) -> int:
        # cshogiの駒種別: 1=歩,2=香,3=桂,4=銀,5=角,6=飛,7=金,8=王,9-14=成り駒
        values = {
            1: 100,   # 歩
            2: 300,   # 香
            3: 300,   # 桂
            4: 400,   # 銀
            5: 700,   # 角
            6: 800,   # 飛
            7: 500,   # 金
            8: 10000, # 玉
            9: 500,   # と
            10: 500,  # 成香
            11: 500,  # 成桂
            12: 600,  # 成銀
            13: 900,  # 馬
            14: 900,  # 龍
        }
        return values.get(piece_type, 0)
