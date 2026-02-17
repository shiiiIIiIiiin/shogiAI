#include "cshogi.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <climits>

// ============================================================
// 駒の評価値テーブル (engine.py:_piece_value の移植)
// ============================================================
static int pieceTypeValue(PieceType pt) {
    switch (pt) {
        case Pawn:      return 100;
        case Lance:     return 300;
        case Knight:    return 300;
        case Silver:    return 400;
        case Bishop:    return 700;
        case Rook:      return 800;
        case Gold:      return 500;
        case King:      return 100000;
        case ProPawn:   return 500;
        case ProLance:  return 500;
        case ProKnight: return 500;
        case ProSilver: return 600;
        case Horse:     return 900;
        case Dragon:    return 900;
        default:        return 0;
    }
}

// HandPiece -> PieceType 変換 (持ち駒評価用)
static PieceType handPieceToPT(HandPiece hp) {
    switch (hp) {
        case HPawn:   return Pawn;
        case HLance:  return Lance;
        case HKnight: return Knight;
        case HSilver: return Silver;
        case HGold:   return Gold;
        case HBishop: return Bishop;
        case HRook:   return Rook;
        default:      return Occupied;
    }
}

// 持ち駒の枚数を取得するヘルパー
static int handCount(const Hand& h, HandPiece hp) {
    switch (hp) {
        case HPawn:   return h.numOf<HPawn>();
        case HLance:  return h.numOf<HLance>();
        case HKnight: return h.numOf<HKnight>();
        case HSilver: return h.numOf<HSilver>();
        case HGold:   return h.numOf<HGold>();
        case HBishop: return h.numOf<HBishop>();
        case HRook:   return h.numOf<HRook>();
        default:      return 0;
    }
}

// ============================================================
// ShogiEngine (engine.py:ShogiEngine の移植)
// ============================================================
class ShogiEngine {
public:
    int nodes;
    int mateSearchDepth;

    ShogiEngine() : nodes(0), mateSearchDepth(2) {}

    struct SearchResult {
        int move;  // Move value (0 = none)
        int score;
        int nodes;
        int depth;
    };

    SearchResult search(__Board& board, int depth) {
        nodes = 0;

        // 詰み探索
        int mateMove = findMate(board, mateSearchDepth);
        if (mateMove != 0) {
            return {mateMove, 100000000, nodes, depth};
        }

        int bestMove = 0;
        int score = negamax(board, depth, -1000000000, 1000000000, bestMove);

        // フォールバック: 手が返らなかった場合
        if (bestMove == 0) {
            MoveList<LegalAll> ml(board.pos);
            if (ml.size() > 0) {
                bestMove = ml.move().value();
            }
        }
        // 合法性チェック
        else if (!board.moveIsLegal(bestMove)) {
            MoveList<LegalAll> ml(board.pos);
            if (ml.size() > 0) {
                bestMove = ml.move().value();
            } else {
                bestMove = 0;
            }
        }

        return {bestMove, score, nodes, depth};
    }

private:
    // 評価関数 (engine.py:evaluate の移植)
    int evaluate(__Board& board) {
        const Position& pos = board.pos;
        int material = 0;

        // 盤上の駒を評価
        for (Square sq = SQ11; sq < SquareNum; ++sq) {
            Piece p = pos.piece(sq);
            if (p == Empty) continue;

            PieceType pt = pieceToPieceType(p);
            int value = pieceTypeValue(pt);

            if (pieceToColor(p) == Black) {
                material += value;
            } else {
                material -= value;
            }
        }

        // 持ち駒を評価
        const Hand blackHand = pos.hand(Black);
        const Hand whiteHand = pos.hand(White);
        for (HandPiece hp = HPawn; hp < HandPieceNum; ++hp) {
            int bCount = handCount(blackHand, hp);
            int wCount = handCount(whiteHand, hp);
            int value = pieceTypeValue(handPieceToPT(hp));
            material += bCount * value;
            material -= wCount * value;
        }

        // 手番視点で返す
        if (pos.turn() == White) {
            material = -material;
        }
        return material;
    }

    // 手の並び替え (engine.py:_generate_moves の移植)
    // 取る手 → 王手 → その他の順
    std::vector<int> generateOrderedMoves(__Board& board) {
        std::vector<int> captures;
        std::vector<int> checks;
        std::vector<int> quiet;

        for (MoveList<LegalAll> ml(board.pos); !ml.end(); ++ml) {
            int move = ml.move().value();
            Square to = (Square)((move >> 0) & 0x7f);
            Piece captured = board.pos.piece(to);

            if (captured != Empty) {
                captures.push_back(move);
            } else {
                board.push(move);
                bool isCheck = board.pos.inCheck();
                board.pop();
                if (isCheck) {
                    checks.push_back(move);
                } else {
                    quiet.push_back(move);
                }
            }
        }

        std::vector<int> result;
        result.reserve(captures.size() + checks.size() + quiet.size());
        result.insert(result.end(), captures.begin(), captures.end());
        result.insert(result.end(), checks.begin(), checks.end());
        result.insert(result.end(), quiet.begin(), quiet.end());
        return result;
    }

    // Negamax探索 (engine.py:_negamax の移植)
    int negamax(__Board& board, int depth, int alpha, int beta, int& outBestMove) {
        ++nodes;
        outBestMove = 0;

        if (depth == 0) {
            return quiescence(board, 0, alpha, beta);
        }

        if (board.is_game_over()) {
            return evaluate(board);
        }

        int bestScore = -1000000000;

        auto moves = generateOrderedMoves(board);
        for (int move : moves) {
            board.push(move);
            int dummy = 0;
            int score = -negamax(board, depth - 1, -beta, -alpha, dummy);
            board.pop();

            if (score > bestScore) {
                bestScore = score;
                outBestMove = move;
            }
            if (score > alpha) {
                alpha = score;
            }
            if (alpha >= beta) {
                break;
            }
        }

        return bestScore;
    }

    // 静止探索 (engine.py:_quiescence の移植)
    int quiescence(__Board& board, int depth, int alpha, int beta) {
        ++nodes;

        if (board.is_game_over()) {
            return evaluate(board);
        }

        int standPat = evaluate(board);
        if (standPat >= beta) return beta;
        if (standPat > alpha) alpha = standPat;

        int bestScore = standPat;

        if (depth < 3) {
            // 取る手を集めて駒の価値でソート (大きい駒から)
            std::vector<std::pair<int, int>> captures; // (value, move)

            auto moves = generateOrderedMoves(board);
            for (int move : moves) {
                Square to = (Square)((move >> 0) & 0x7f);
                Piece captured = board.pos.piece(to);
                if (captured == Empty) continue;

                PieceType pt = pieceToPieceType(captured);
                int value = pieceTypeValue(pt);
                captures.push_back({value, move});
            }

            // 大きい駒から順にソート
            std::sort(captures.begin(), captures.end(),
                [](const auto& a, const auto& b) { return a.first > b.first; });

            for (auto& [val, move] : captures) {
                if (!board.moveIsLegal(move)) continue;

                board.push(move);
                int score = -quiescence(board, depth + 1, -beta, -alpha);
                board.pop();

                if (score > bestScore) bestScore = score;
                if (score > alpha) alpha = score;
                if (alpha >= beta) break;
            }
        }

        return bestScore;
    }

    // 詰み探索 (engine.py:_find_mate の移植)
    int findMate(__Board& board, int depth) {
        if (depth <= 0) return 0;

        for (MoveList<LegalAll> ml(board.pos); !ml.end(); ++ml) {
            int move = ml.move().value();
            board.push(move);
            if (isForcedMate(board, depth - 1, false)) {
                board.pop();
                return move;
            }
            board.pop();
        }
        return 0;
    }

    // 詰み判定 (engine.py:_is_forced_mate の移植)
    bool isForcedMate(__Board& board, int depth, bool attacker) {
        if (depth <= 0) return false;

        MoveList<LegalAll> ml(board.pos);
        if (ml.size() == 0) {
            return board.pos.inCheck();
        }

        if (attacker) {
            for (MoveList<LegalAll> ml2(board.pos); !ml2.end(); ++ml2) {
                int move = ml2.move().value();
                board.push(move);
                if (isForcedMate(board, depth - 1, false)) {
                    board.pop();
                    return true;
                }
                board.pop();
            }
            return false;
        }

        // 受ける側: 全ての手で詰みが避けられない場合のみ詰み
        for (MoveList<LegalAll> ml2(board.pos); !ml2.end(); ++ml2) {
            int move = ml2.move().value();
            board.push(move);
            if (!isForcedMate(board, depth - 1, true)) {
                board.pop();
                return false;
            }
            board.pop();
        }
        return true;
    }
};

// ============================================================
// USI プロトコルハンドラ (usi.py:USIEngine の移植)
// ============================================================
class USIHandler {
public:
    USIHandler() {}

    void run() {
        std::string line;
        while (std::getline(std::cin, line)) {
            // 末尾の改行や空白を除去
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
                line.pop_back();
            }
            if (line.empty()) continue;

            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;

            if (cmd == "usi") {
                std::cout << "id name ShogiAlgo-cpp" << std::endl;
                std::cout << "id author kimura" << std::endl;
                std::cout << "usiok" << std::endl;
            }
            else if (cmd == "isready") {
                std::cout << "readyok" << std::endl;
            }
            else if (cmd == "usinewgame") {
                board = __Board();
            }
            else if (cmd == "position") {
                handlePosition(iss);
            }
            else if (cmd == "go") {
                handleGo();
            }
            else if (cmd == "quit") {
                break;
            }
        }
    }

private:
    __Board board;
    ShogiEngine engine;

    void handlePosition(std::istringstream& iss) {
        // "position" の後の文字列全体を set_position に渡す
        std::string rest;
        std::getline(iss, rest);
        // 先頭の空白を除去
        if (!rest.empty() && rest[0] == ' ') {
            rest = rest.substr(1);
        }
        try {
            board.set_position(rest);
        } catch (...) {
            // パース失敗時はボードを初期化
            board = __Board();
        }
    }

    void handleGo() {
        int depth = 5;
        auto result = engine.search(board, depth);

        if (result.move != 0) {
            std::string moveUsi = Move(result.move).toUSI();
            std::cout << "info depth " << result.depth
                      << " score cp " << result.score
                      << " nodes " << result.nodes << std::endl;
            std::cout << "bestmove " << moveUsi << std::endl;
        } else {
            std::cout << "bestmove resign" << std::endl;
        }
    }
};

// ============================================================
// main
// ============================================================
int main() {
    // cshogi の初期化 (ルックアップテーブル等)
    initTable();

    USIHandler handler;
    handler.run();

    return 0;
}
