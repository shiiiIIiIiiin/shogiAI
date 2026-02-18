#include "cshogi.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <climits>
#include <chrono>

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
// ShogiEngine (engine.py:ShogiEngine の移植 + 反復深化・時間制御)
// ============================================================
class ShogiEngine {
public:
    int nodes;
    int mateSearchDepth;

    ShogiEngine() : nodes(0), mateSearchDepth(2), stopSearch(false) {}

    struct SearchResult {
        int move;  // Move value (0 = none)
        int score;
        int nodes;
        int depth;
    };

    // 反復深化探索 (時間制限付き)
    SearchResult searchWithTimeLimit(__Board& board, int timeLimitMs) {
        nodes = 0;
        stopSearch = false;
        searchStartTime = std::chrono::steady_clock::now();
        searchTimeLimitMs = timeLimitMs;

        // 詰み探索
        int mateMove = findMate(board, mateSearchDepth);
        if (mateMove != 0) {
            return {mateMove, 100000000, nodes, 1};
        }

        int bestMove = 0;
        int bestScore = 0;
        int completedDepth = 0;

        // 反復深化: depth 1 から順に深くする
        for (int depth = 1; depth <= 30; ++depth) {
            stopSearch = false;
            int move = 0;
            int score = negamax(board, depth, -1000000000, 1000000000, move);

            if (stopSearch) {
                // 時間切れで中断 → 前の深さの結果を使う
                break;
            }

            // この深さの探索が完了した
            if (move != 0) {
                bestMove = move;
                bestScore = score;
                completedDepth = depth;
            }

            std::cout << "info depth " << depth
                      << " score cp " << score
                      << " nodes " << nodes << std::endl;

            // 残り時間チェック: 次の深さを探索する余裕があるか
            auto elapsed = std::chrono::steady_clock::now() - searchStartTime;
            int elapsedMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            // 経過時間が制限の60%を超えたら次の深さには行かない
            if (elapsedMs > timeLimitMs * 60 / 100) {
                break;
            }
        }

        // フォールバック
        if (bestMove == 0) {
            MoveList<LegalAll> ml(board.pos);
            if (ml.size() > 0) {
                bestMove = ml.move().value();
            }
        }
        else if (!board.moveIsLegal(bestMove)) {
            MoveList<LegalAll> ml(board.pos);
            if (ml.size() > 0) {
                bestMove = ml.move().value();
            } else {
                bestMove = 0;
            }
        }

        return {bestMove, bestScore, nodes, completedDepth};
    }

private:
    bool stopSearch;
    std::chrono::steady_clock::time_point searchStartTime;
    int searchTimeLimitMs;

    // 時間切れチェック (1024ノードごとに確認)
    bool isTimeUp() {
        if ((nodes & 1023) != 0) return false;
        auto elapsed = std::chrono::steady_clock::now() - searchStartTime;
        int elapsedMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        return elapsedMs >= searchTimeLimitMs;
    }

    // 評価関数
    int evaluate(__Board& board) {
        const Position& pos = board.pos;
        int material = 0;

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

        const Hand blackHand = pos.hand(Black);
        const Hand whiteHand = pos.hand(White);
        for (HandPiece hp = HPawn; hp < HandPieceNum; ++hp) {
            int bCount = handCount(blackHand, hp);
            int wCount = handCount(whiteHand, hp);
            int value = pieceTypeValue(handPieceToPT(hp));
            material += bCount * value;
            material -= wCount * value;
        }

        if (pos.turn() == White) {
            material = -material;
        }
        return material;
    }

    // 手の並び替え: 取る手 → 王手 → その他
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

    // Negamax探索 (時間切れ対応)
    int negamax(__Board& board, int depth, int alpha, int beta, int& outBestMove) {
        ++nodes;
        outBestMove = 0;

        if (stopSearch) return 0;
        if (isTimeUp()) { stopSearch = true; return 0; }

        if (depth == 0) {
            return quiescence(board, 0, alpha, beta);
        }

        if (board.is_game_over()) {
            return -999999 + (30 - depth);
        }

        int bestScore = -1000000000;

        auto moves = generateOrderedMoves(board);
        for (int move : moves) {
            board.push(move);
            int dummy = 0;
            int score = -negamax(board, depth - 1, -beta, -alpha, dummy);
            board.pop();

            if (stopSearch) return 0;

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

    // 静止探索 (時間切れ対応)
    int quiescence(__Board& board, int depth, int alpha, int beta) {
        ++nodes;

        if (stopSearch) return 0;
        if (isTimeUp()) { stopSearch = true; return 0; }

        if (board.is_game_over()) {
            return -999999;
        }

        int standPat = evaluate(board);
        if (standPat >= beta) return beta;
        if (standPat > alpha) alpha = standPat;

        int bestScore = standPat;

        if (depth < 3) {
            std::vector<std::pair<int, int>> captures;

            auto moves = generateOrderedMoves(board);
            for (int move : moves) {
                Square to = (Square)((move >> 0) & 0x7f);
                Piece captured = board.pos.piece(to);
                if (captured == Empty) continue;

                PieceType pt = pieceToPieceType(captured);
                int value = pieceTypeValue(pt);
                captures.push_back({value, move});
            }

            std::sort(captures.begin(), captures.end(),
                [](const auto& a, const auto& b) { return a.first > b.first; });

            for (auto& [val, move] : captures) {
                if (!board.moveIsLegal(move)) continue;

                board.push(move);
                int score = -quiescence(board, depth + 1, -beta, -alpha);
                board.pop();

                if (stopSearch) return 0;

                if (score > bestScore) bestScore = score;
                if (score > alpha) alpha = score;
                if (alpha >= beta) break;
            }
        }

        return bestScore;
    }

    // 詰み探索
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
// USI プロトコルハンドラ (時間制御対応)
// ============================================================
class USIHandler {
public:
    USIHandler() {}

    void run() {
        std::string line;
        while (std::getline(std::cin, line)) {
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
                handleGo(iss);
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
        std::string rest;
        std::getline(iss, rest);
        if (!rest.empty() && rest[0] == ' ') {
            rest = rest.substr(1);
        }
        try {
            board.set_position(rest);
        } catch (...) {
            board = __Board();
        }
    }

    void handleGo(std::istringstream& iss) {
        // go コマンドのパラメータを解析
        int btime = 0, wtime = 0, byoyomi = 0, binc = 0, winc = 0;
        bool hasTime = false;

        std::string token;
        while (iss >> token) {
            if (token == "btime") { iss >> btime; hasTime = true; }
            else if (token == "wtime") { iss >> wtime; hasTime = true; }
            else if (token == "byoyomi") { iss >> byoyomi; hasTime = true; }
            else if (token == "binc") { iss >> binc; hasTime = true; }
            else if (token == "winc") { iss >> winc; hasTime = true; }
        }

        int timeLimitMs;
        if (hasTime) {
            int myTime = (board.pos.turn() == Black) ? btime : wtime;
            int myInc = (board.pos.turn() == Black) ? binc : winc;

            if (byoyomi > 0) {
                // 秒読み: 残り時間の1/50 + 秒読みの80%
                timeLimitMs = myTime / 50 + byoyomi * 80 / 100;
                if (timeLimitMs > 5000) timeLimitMs = 5000;
            } else if (myInc > 0) {
                // フィッシャー (floodgate-600-10F 等):
                // 加算時間を基本に、残り時間の一部を上乗せ
                timeLimitMs = myInc + myTime / 60;
                int maxTime = std::min(myTime / 3, myInc * 3);
                if (timeLimitMs > maxTime) timeLimitMs = maxTime;
                // 残り時間に余裕があれば、少なくとも加算時間分は考える
                if (myTime > myInc * 2 && timeLimitMs < myInc) {
                    timeLimitMs = myInc;
                }
            } else {
                // 切れ負け: 残り時間の1/30
                timeLimitMs = myTime / 30;
                if (timeLimitMs > 5000) timeLimitMs = 5000;
            }

            if (timeLimitMs < 200) timeLimitMs = 200;

            // 残り時間が少ない場合はさらに短く
            if (myTime < 10000) {
                timeLimitMs = std::min(timeLimitMs, myTime / 5);
                if (timeLimitMs < 100) timeLimitMs = 100;
            }
        } else {
            timeLimitMs = 2000;
        }

        auto result = engine.searchWithTimeLimit(board, timeLimitMs);

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
    initTable();

    USIHandler handler;
    handler.run();

    return 0;
}
