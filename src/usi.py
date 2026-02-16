from __future__ import annotations

import sys
import cshogi
from datetime import datetime

from engine import ShogiEngine


class USIEngine:
    def __init__(self) -> None:
        self.board = cshogi.Board()
        self.engine = ShogiEngine()
        # デバッグログファイル
        self.debug_log = open("shogialgo_debug.log", "a")
        self.debug_log.write(f"\n=== Session started at {datetime.now()} ===\n")
        self.debug_log.flush()

    def log_debug(self, message: str) -> None:
        """デバッグメッセージをファイルとUSI infoで出力"""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        log_msg = f"[{timestamp}] {message}"
        self.debug_log.write(log_msg + "\n")
        self.debug_log.flush()
        # USI info で将棋所にも送る
        print(f"info string {message}", flush=True)

    def run(self) -> None:
        while True:
            try:
                line = input().strip()
            except EOFError:
                break

            if not line:
                continue

            parts = line.split()
            cmd = parts[0]

            if cmd == "usi":
                print("id name ShogiAlgo")
                print("id author kimura")
                print("usiok")
                sys.stdout.flush()

            elif cmd == "isready":
                print("readyok")
                sys.stdout.flush()

            elif cmd == "usinewgame":
                self.board = cshogi.Board()

            elif cmd == "position":
                self._handle_position(parts[1:])

            elif cmd == "go":
                self._handle_go(parts[1:])

            elif cmd == "quit":
                break

    def _handle_position(self, args: list[str]) -> None:
        if not args:
            return

        if args[0] == "startpos":
            self.board = cshogi.Board()
            moves_idx = 2 if len(args) > 1 and args[1] == "moves" else -1
        elif args[0] == "sfen":
            # sfen形式の局面読み込み
            sfen_parts = []
            i = 1
            while i < len(args) and args[i] != "moves":
                sfen_parts.append(args[i])
                i += 1
            self.board = cshogi.Board(" ".join(sfen_parts))
            moves_idx = i + 1 if i < len(args) else -1
        else:
            return

        # 指し手を適用
        if moves_idx > 0 and moves_idx < len(args):
            for usi in args[moves_idx:]:
                move = self.board.move_from_usi(usi)
                self.board.push(move)

    def _handle_go(self, args: list[str]) -> None:
        # 簡易的に固定深さ探索
        depth = 5
        result = self.engine.search(self.board, depth)
        
        if result.move is not None:
            move_usi = cshogi.move_to_usi(result.move)
            self.log_debug(f"Selected: {move_usi}, Score: {result.score}, Nodes: {result.nodes}")
            print(f"bestmove {move_usi}")
        else:
            self.log_debug(f"ERROR: No move returned")
            print("bestmove resign")
        
        sys.stdout.flush()


if __name__ == "__main__":
    engine = USIEngine()
    engine.run()
