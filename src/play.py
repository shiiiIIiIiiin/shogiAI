from __future__ import annotations

import cshogi

from engine import ShogiEngine


def format_board_with_alpha_ranks(board: cshogi.Board) -> str:
    piece_map = {
        'FU': '歩', 'KY': '香', 'KE': '桂', 'GI': '銀', 'KI': '金',
        'KA': '角', 'HI': '飛', 'OU': '玉',
        'TO': 'と', 'NY': '杏', 'NK': '圭', 'NG': '全',
        'UM': '馬', 'RY': '龍'
    }
    
    lines = str(board).splitlines()
    rank_labels = ["a", "b", "c", "d", "e", "f", "g", "h", "i"]
    formatted = []
    for line in lines:
        # Replace piece symbols with Japanese
        for eng, jpn in piece_map.items():
            line = line.replace(eng, jpn)
        
        if line.startswith("P") and len(line) > 2 and line[1].isdigit():
            idx = int(line[1]) - 1
            if 0 <= idx < 9:
                formatted.append(f"{rank_labels[idx]}{line[2:]}")
                continue
        formatted.append(line)
    return "\n".join(formatted)


def main() -> None:
    board = cshogi.Board()
    engine = ShogiEngine()

    while not board.is_game_over():
        # engine 手番
        result = engine.search(board, depth=3)
        if result.move is None:
            break
        board.push(result.move)
        print(f"engine move: {cshogi.move_to_usi(result.move)}")
        print(format_board_with_alpha_ranks(board))

        if board.is_game_over():
            break

        # 人間の手番: USI 形式で入力
        while True:
            usi = input("your move (usi): ").strip()
            if usi == "":
                return
            try:
                move = board.move_from_usi(usi)
            except Exception:
                print("invalid usi format")
                continue

            if not board.is_legal(move):
                print("illegal move")
                continue

            board.push(move)
            print(format_board_with_alpha_ranks(board))
            break

    print("game over")


if __name__ == "__main__":
    main()
