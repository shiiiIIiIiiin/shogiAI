from __future__ import annotations

import os
import sys
import subprocess
import webbrowser
import threading
from typing import Dict, Optional

from flask import Flask, jsonify, render_template, request
import cshogi

# PyInstaller対応: sys.path を先に設定
if getattr(sys, 'frozen', False):
    BASE_DIR = sys._MEIPASS
    TEMPLATE_DIR = os.path.join(BASE_DIR, "web", "templates")
else:
    WEB_DIR = os.path.dirname(os.path.abspath(__file__))
    ROOT_DIR = os.path.dirname(WEB_DIR)
    TEMPLATE_DIR = os.path.join(WEB_DIR, "templates")


class CppEngine:
    """C++ ShogiAlgo.exe とUSIプロトコルで通信するラッパー"""

    def __init__(self, exe_path: str):
        self.exe_path = exe_path
        self.process: Optional[subprocess.Popen] = None
        self._start()

    def _start(self):
        self.process = subprocess.Popen(
            [self.exe_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        self._send("usi")
        self._wait_for("usiok")
        self._send("isready")
        self._wait_for("readyok")

    def _send(self, cmd: str):
        self.process.stdin.write(cmd + "\n")
        self.process.stdin.flush()

    def _wait_for(self, expected: str) -> str:
        while True:
            line = self.process.stdout.readline().strip()
            if line == expected:
                return line

    def search(self, board: cshogi.Board, time_ms: int = 5000) -> Optional[str]:
        """盤面を送ってbestmoveをUSI文字列で返す"""
        if self.process is None or self.process.poll() is not None:
            self._start()

        sfen = board.sfen() if hasattr(board, "sfen") else board.to_sfen()
        self._send(f"position sfen {sfen}")
        self._send(f"go byoyomi {time_ms}")

        while True:
            line = self.process.stdout.readline().strip()
            if line.startswith("bestmove"):
                parts = line.split()
                if len(parts) >= 2 and parts[1] != "resign" and parts[1] != "win":
                    return parts[1]
                return None

    def close(self):
        if self.process and self.process.poll() is None:
            self._send("quit")
            self.process.wait(timeout=3)


def find_cpp_engine() -> str:
    """C++エンジンのパスを探す"""
    if getattr(sys, 'frozen', False):
        base = sys._MEIPASS
    else:
        base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    candidates = [
        os.path.join(base, "cpp", "build", "Release", "ShogiAlgo.exe"),
        os.path.join(base, "cpp", "build", "ShogiAlgo.exe"),
    ]
    for path in candidates:
        if os.path.isfile(path):
            return path
    raise FileNotFoundError(
        f"ShogiAlgo.exe が見つかりません。先にC++版をビルドしてください。\n"
        f"検索パス: {candidates}"
    )


app = Flask(__name__, template_folder=TEMPLATE_DIR)
engine = CppEngine(find_cpp_engine())

PIECE_JP = {
    1: "歩",
    2: "香",
    3: "桂",
    4: "銀",
    5: "角",
    6: "飛",
    7: "金",
    8: "玉",
    9: "と",
    10: "杏",
    11: "圭",
    12: "全",
    13: "馬",
    14: "龍",
}


def board_to_sfen(board: cshogi.Board) -> str:
    if hasattr(board, "sfen"):
        return board.sfen()
    if hasattr(board, "to_sfen"):
        return board.to_sfen()
    raise AttributeError("Board has no sfen method")


def render_hands(board: cshogi.Board) -> str:
    black_hand, white_hand = board.pieces_in_hand
    hand_pieces = ["歩", "香", "桂", "銀", "金", "角", "飛"]

    def fmt_hand(counts):
        parts = []
        for name, count in zip(hand_pieces, counts):
            if count > 0:
                parts.append(f"{name}×{count}")
        return " ".join(parts) if parts else "なし"

    return (
        "<div class='hands hands-layout'>"
        "<div class='hand player'>"
        "<div class='hand-title'>🙂 プレイヤー（先手）</div>"
        f"<div class='hand-body'>{fmt_hand(black_hand)}</div>"
        "</div>"
        "<div class='hand ai'>"
        "<div class='hand-title'>🤖 AI（後手）</div>"
        f"<div class='hand-body'>{fmt_hand(white_hand)}</div>"
        "</div>"
        "</div>"
    )


def render_board_html(board: cshogi.Board) -> str:
    # 盤面: ranks a-i, files 9-1 (将棋の見た目に合わせる)
    ranks = ["a", "b", "c", "d", "e", "f", "g", "h", "i"]
    files = [9, 8, 7, 6, 5, 4, 3, 2, 1]
    square_names = getattr(cshogi, "SQUARE_NAMES", None)

    rows = []
    for r_index, rank in enumerate(ranks):
        cells = []
        for file_num in files:
            # cshogiのsqは file(1-9)×rank(a-i) の順
            sq = (file_num - 1) * 9 + r_index
            sq_name = square_names[sq] if square_names else f"{file_num}{rank}"
            piece = board.piece(sq)
            if piece == 0:
                piece_str = ""
                cls = "empty"
            else:
                piece_type = board.piece_type(sq)
                jp = PIECE_JP.get(piece_type, "?")
                is_white = piece >= 16
                piece_str = jp
                cls = "white" if is_white else "black"
            cells.append(
                f"<td class='{cls}' data-sq='{sq}' data-name='{sq_name}'>"
                + piece_str
                + "</td>"
            )
        rows.append("<tr>" + "".join(cells) + "</tr>")

    board_html = (
        "<table class='board'>"
        + "".join(rows)
        + "</table>"
    )
    return board_html


def get_game_result(board: cshogi.Board) -> str:
    """ゲーム結果を判定"""
    if not board.is_game_over():
        return ""
    
    # 王手の判定で勝敗を判定
    # is_checkmate() -> 現在のプレイヤーが詰まされている
    if board.is_checkmate():
        # 前のターンのプレイヤーが勝ち
        winner = "AI（後手）" if board.turn == cshogi.BLACK else "プレイヤー（先手）"
        return f"終局！ {winner} の勝ち"
    
    # 千日手や同一局面繰り返し
    if board.is_draw():
        return "終局：引き分け（反復）"
    
    # その他の終了
    return "終局"


def json_state(board: cshogi.Board, message: str = "") -> Dict:
    black_hand, white_hand = board.pieces_in_hand
    
    # 終局時はメッセージに結果を入れる
    if board.is_game_over() and not message:
        message = get_game_result(board)
    
    return {
        "sfen": board_to_sfen(board),
        "board_html": render_board_html(board),
        "turn": "先手" if board.turn == cshogi.BLACK else "後手",
        "game_over": board.is_game_over(),
        "message": message,
        "hands": {
            "black": list(black_hand),
            "white": list(white_hand),
        },
    }


@app.get("/")
def index():
    board = cshogi.Board()
    return render_template(
        "index.html",
        sfen=board_to_sfen(board),
        board_html=render_board_html(board),
        turn="先手",
    )


@app.post("/api/play")
def play():
    data = request.get_json(force=True)
    sfen = data.get("sfen")
    move_usi = (data.get("move") or "").strip()

    board = cshogi.Board(sfen) if sfen else cshogi.Board()

    if move_usi:
        try:
            move = board.move_from_usi(move_usi)
        except Exception:
            return jsonify(json_state(board, "不正なUSI形式です")), 400

        if not board.is_legal(move):
            return jsonify(json_state(board, "その手は合法ではありません")), 400
        board.push(move)

    if board.is_game_over():
        return jsonify(json_state(board, "終局です"))

    # AIの手 (C++エンジン)
    move_usi = engine.search(board)
    if move_usi is None:
        return jsonify(json_state(board, "AIが手を返せませんでした")), 500
    try:
        move = board.move_from_usi(move_usi)
    except Exception:
        return jsonify(json_state(board, "AIが不正な手を返しました")), 500
    if not board.is_legal(move):
        return jsonify(json_state(board, "AIが不正な手を返しました")), 500
    board.push(move)

    return jsonify(json_state(board, ""))


@app.post("/api/player_move")
def player_move():
    data = request.get_json(force=True)
    sfen = data.get("sfen")
    move_usi = (data.get("move") or "").strip()

    board = cshogi.Board(sfen) if sfen else cshogi.Board()

    if not move_usi:
        return jsonify(json_state(board, "手が指定されていません")), 400

    try:
        move = board.move_from_usi(move_usi)
    except Exception:
        return jsonify(json_state(board, "不正なUSI形式です")), 400

    if not board.is_legal(move):
        return jsonify(json_state(board, "その手は合法ではありません")), 400

    board.push(move)
    return jsonify(json_state(board, ""))


@app.post("/api/ai")
def ai_move():
    data = request.get_json(force=True)
    sfen = data.get("sfen")

    board = cshogi.Board(sfen) if sfen else cshogi.Board()
    if board.is_game_over():
        return jsonify(json_state(board, "終局です"))

    # AIの手 (C++エンジン)
    move_usi = engine.search(board)
    if move_usi is None:
        return jsonify(json_state(board, "AIが手を返せませんでした")), 500
    try:
        move = board.move_from_usi(move_usi)
    except Exception:
        return jsonify(json_state(board, "AIが不正な手を返しました")), 500
    if not board.is_legal(move):
        return jsonify(json_state(board, "AIが不正な手を返しました")), 500
    board.push(move)

    return jsonify(json_state(board, ""))


@app.post("/api/promote_check")
def promote_check():
    data = request.get_json(force=True)
    sfen = data.get("sfen")
    from_sq = (data.get("from") or "").strip()
    to_sq = (data.get("to") or "").strip()

    if not from_sq or not to_sq:
        return jsonify({"can_promote": False, "legal": False}), 400

    if "*" in from_sq or "*" in to_sq:
        return jsonify({"can_promote": False, "legal": False}), 400

    board = cshogi.Board(sfen) if sfen else cshogi.Board()

    # square name -> index
    try:
        square_names = getattr(cshogi, "SQUARE_NAMES")
        from_idx = square_names.index(from_sq)
        to_idx = square_names.index(to_sq)
    except Exception:
        return jsonify({"can_promote": False, "legal": False}), 400

    piece = board.piece(from_idx)
    if piece == 0:
        return jsonify({"can_promote": False, "legal": False})

    piece_type = board.piece_type(from_idx)
    promotable_types = {1, 2, 3, 4, 5, 6}
    promotable = piece_type in promotable_types

    def in_promotion_zone(square_idx: int) -> bool:
        rank = square_idx % 9  # 0=a ... 8=i
        if board.turn == cshogi.BLACK:
            return rank <= 2  # a,b,c
        return rank >= 6  # g,h,i

    move_plain = f"{from_sq}{to_sq}"
    move_promote = f"{from_sq}{to_sq}+"

    legal_plain = False
    legal_promote = False

    try:
        move = board.move_from_usi(move_plain)
        legal_plain = board.is_legal(move)
    except Exception:
        legal_plain = False

    try:
        move = board.move_from_usi(move_promote)
        legal_promote = board.is_legal(move)
    except Exception:
        legal_promote = False

    eligible = promotable and (in_promotion_zone(from_idx) or in_promotion_zone(to_idx))

    return jsonify(
        {
            "legal": legal_plain or legal_promote,
            "can_promote": legal_plain and legal_promote and eligible,
            "must_promote": (not legal_plain) and legal_promote,
        }
    )


@app.post("/api/reset")
def reset():
    board = cshogi.Board()
    return jsonify(json_state(board, "リセットしました"))


if __name__ == "__main__":
    PORT = 5000
    
    def open_browser():
        import time
        time.sleep(2)  # サーバー起動を待つ
        try:
            webbrowser.open(f"http://localhost:{PORT}")
        except Exception as e:
            print(f"ブラウザ起動エラー: {e}")
            print(f"ブラウザで http://localhost:{PORT} を開いてください")
    
    # ブラウザを自動で開く
    threading.Thread(target=open_browser, daemon=True).start()
    
    print(f"\n Flask サーバーを起動中... http://localhost:{PORT}")
    app.run(host="127.0.0.1", port=PORT, debug=False)
