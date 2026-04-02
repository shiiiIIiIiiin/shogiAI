#include "cshogi.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <climits>
#include <chrono>
#include <cmath>

// ============================================================
// 駒の評価値テーブル (engine.py:_piece_value の移植)
// ============================================================
static int pieceTypeValue(PieceType pt)
{
    switch (pt)
    {
    case Pawn:
        return 100; // 歩
    case Lance:
        return 300; // 香車
    case Knight:
        return 300; // 桂馬
    case Silver:
        return 400; // 銀
    case Bishop:
        return 700; // 角
    case Rook:
        return 800; // 飛車
    case Gold:
        return 500; // 金
    case King:
        return 100000; // 玉
    case ProPawn:
        return 410; // と金（成り歩）
    case ProLance:
        return 450; // 成香（成り香）
    case ProKnight:
        return 450; // 成桂（成り桂）
    case ProSilver:
        return 480; // 成銀（成り銀）
    case Horse:
        return 900; // 馬（成り角）
    case Dragon:
        return 900; // 龍（成り飛車）
    default:
        return 0;
    }
}

// HandPiece -> PieceType 変換 (持ち駒評価用)
static PieceType handPieceToPT(HandPiece hp)
{
    switch (hp)
    {
    case HPawn:
        return Pawn;
    case HLance:
        return Lance;
    case HKnight:
        return Knight;
    case HSilver:
        return Silver;
    case HGold:
        return Gold;
    case HBishop:
        return Bishop;
    case HRook:
        return Rook;
    default:
        return Occupied;
    }
}

// 持ち駒の枚数を取得するヘルパー
static int handCount(const Hand &h, HandPiece hp)
{
    switch (hp)
    {
    case HPawn:
        return h.numOf<HPawn>();
    case HLance:
        return h.numOf<HLance>();
    case HKnight:
        return h.numOf<HKnight>();
    case HSilver:
        return h.numOf<HSilver>();
    case HGold:
        return h.numOf<HGold>();
    case HBishop:
        return h.numOf<HBishop>();
    case HRook:
        return h.numOf<HRook>();
    default:
        return 0;
    }
}

// ============================================================
// 駒の位置評価 (Piece-Square Table)
// ============================================================
// rank: 0=一段(敵陣奥), 8=九段(自陣奥) ※先手視点
// 後手の場合は rank を反転して使う
static int positionalBonus(PieceType pt, int file, int rank)
{
    int bonus = 0;
    switch (pt)
    {
    case Pawn:
        // 前進ボーナス + 中央筋ボーナス
        bonus = (8 - rank) * 2;
        if (file >= 2 && file <= 6)
            bonus += 3;
        break;
    case Lance:
        bonus = std::max(0, (7 - rank)) * 2;
        break;
    case Knight:
        bonus = std::max(0, (6 - rank)) * 3;
        if (file >= 2 && file <= 6)
            bonus += 5;
        break;
    case Silver:
        bonus = std::max(0, (7 - rank)) * 3;
        if (file >= 2 && file <= 6)
            bonus += 5;
        break;
    case Gold:
        bonus = std::max(0, (7 - rank)) * 2;
        break;
    case Bishop:
    {
        // 中央寄りボーナス
        int cd = abs(file - 4) + abs(rank - 4);
        bonus = std::max(0, 15 - cd * 2);
        break;
    }
    case Rook:
        // 飛車活用: 前進で大きなボーナス
        bonus = std::max(0, (7 - rank)) * 7;
        break;
    case King:
        // 囲い位置ボーナス（控えめ: 最大12点）
        if (rank >= 7)
            bonus += 7;
        else if (rank == 6)
            bonus += 3;
        else
            bonus -= (6 - rank) * 2;
        // 端寄りボーナス
        if (file == 1 || file == 7)
            bonus += 5;
        else if (file == 0 || file == 2 || file == 6 || file == 8)
            bonus += 3;
        else
            bonus -= 3;
        break;
    case ProPawn:
    case ProLance:
    case ProKnight:
    case ProSilver:
        bonus = std::max(0, (6 - rank)) * 3;
        if (file >= 2 && file <= 6)
            bonus += 5;
        break;
    case Horse:
    {
        int cd = abs(file - 4) + abs(rank - 4);
        bonus = std::max(0, 12 - cd * 2);
        if (rank < 6)
            bonus += (6 - rank) * 2;
        break;
    }
    case Dragon:
        bonus = std::max(0, (6 - rank)) * 6;
        break;
    default:
        break;
    }
    return bonus;
}

// 玉の安全度: 隣接8マスの味方駒 (飛車・龍除く)
static int kingSafety(const Position &pos, Square kingSq, Color c)
{
    int safety = 0;
    int kf = (int)makeFile(kingSq);
    int kr = (int)makeRank(kingSq);
    for (int df = -1; df <= 1; ++df)
    {
        for (int dr = -1; dr <= 1; ++dr)
        {
            if (df == 0 && dr == 0)
                continue;
            int nf = kf + df;
            int nr = kr + dr;
            if (nf < 0 || nf > 8 || nr < 0 || nr > 8)
                continue;
            Square nsq = makeSquare((File)nf, (Rank)nr);
            Piece p = pos.piece(nsq);
            if (p != Empty && pieceToColor(p) == c)
            {
                PieceType pt = pieceToPieceType(p);
                int bonus = 0;
                switch (pt)
                {
                case Gold:
                    bonus = 40; // 金
                    break;
                case Silver:
                    bonus = 30; // 銀
                    break;
                case Knight:
                    bonus = 20; // 桂馬
                    break;
                case Lance:
                    bonus = 20; // 香車
                    break;
                case Bishop:
                    bonus = 15; // 角
                    break;
                case ProSilver:
                    bonus = 30; // 成銀
                    break;
                case ProKnight:
                    bonus = 20; // 成桂
                    break;
                case ProLance:
                    bonus = 20; // 成香
                    break;
                case ProPawn:
                    bonus = 10; // と金
                    break;
                case Horse:
                case Dragon:
                case Rook:
                case Pawn:
                    bonus = 0; // 馬・龍・飛車・歩は0点
                    break;
                default:
                    bonus = 10; // その他
                    break;
                }
                if (bonus > 0)
                    safety += bonus;
            }
        }
    }
    return safety;
}

// ============================================================
// ShogiEngine (engine.py:ShogiEngine の移植 + 反復深化・時間制御)
// ============================================================
class ShogiEngine
{
public:
    int nodes;
    int mateSearchDepth;

    ShogiEngine() : nodes(0), mateSearchDepth(2), stopSearch(false) {}

    struct SearchResult
    {
        int move; // Move value (0 = none)
        int score;
        int nodes;
        int depth;
    };

    // 反復深化探索 (時間制限付き)
    SearchResult searchWithTimeLimit(__Board &board, int timeLimitMs)
    {
        nodes = 0;
        stopSearch = false;
        searchStartTime = std::chrono::steady_clock::now();
        searchTimeLimitMs = timeLimitMs;

        // 詰み探索
        int mateMove = findMate(board, mateSearchDepth);
        if (mateMove != 0)
        {
            return {mateMove, 100000000, nodes, 1};
        }

        int bestMove = 0;
        int bestScore = 0;
        int completedDepth = 0;

        // 反復深化: depth 1 から順に深くする
        for (int depth = 1; depth <= 30; ++depth)
        {
            stopSearch = false;
            int move = 0;
            int score = negamax(board, depth, -1000000000, 1000000000, move);

            if (stopSearch)
            {
                // 時間切れで中断 → 前の深さの結果を使う
                break;
            }

            // この深さの探索が完了した
            if (move != 0)
            {
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
            if (elapsedMs > timeLimitMs * 60 / 100)
            {
                break;
            }
        }

        // フォールバック
        if (bestMove == 0)
        {
            MoveList<LegalAll> ml(board.pos);
            if (ml.size() > 0)
            {
                bestMove = ml.move().value();
            }
        }
        else if (!board.moveIsLegal(bestMove))
        {
            MoveList<LegalAll> ml(board.pos);
            if (ml.size() > 0)
            {
                bestMove = ml.move().value();
            }
            else
            {
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
    bool isTimeUp()
    {
        if ((nodes & 1023) != 0)
            return false;
        auto elapsed = std::chrono::steady_clock::now() - searchStartTime;
        int elapsedMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        return elapsedMs >= searchTimeLimitMs;
    }

    // 評価関数 (駒割り + PST + 玉の安全度)
    int evaluate(__Board &board)
    {
        const Position &pos = board.pos;
        int material = 0;
        int positional = 0;
        Square blackKingSq = SQ59; // デフォルト
        Square whiteKingSq = SQ51;

        for (Square sq = SQ11; sq < SquareNum; ++sq)
        {
            Piece p = pos.piece(sq);
            if (p == Empty)
                continue;

            PieceType pt = pieceToPieceType(p);
            Color c = pieceToColor(p);
            int value = pieceTypeValue(pt);

            // 玉の位置を記録
            if (pt == King)
            {
                if (c == Black)
                    blackKingSq = sq;
                else
                    whiteKingSq = sq;
            }

            // 駒割り
            if (c == Black)
            {
                material += value;
            }
            else
            {
                material -= value;
            }

            // PST: 位置ボーナス
            int file = (int)makeFile(sq);
            int rank = (int)makeRank(sq);
            // 後手は rank を反転 (8 - rank)
            int pstRank = (c == Black) ? rank : (8 - rank);
            int pstBonus = positionalBonus(pt, file, pstRank);
            if (c == Black)
            {
                positional += pstBonus;
            }
            else
            {
                positional -= pstBonus;
            }
        }

        // 持ち駒評価
        const Hand blackHand = pos.hand(Black);
        const Hand whiteHand = pos.hand(White);
        for (HandPiece hp = HPawn; hp < HandPieceNum; ++hp)
        {
            int bCount = handCount(blackHand, hp);
            int wCount = handCount(whiteHand, hp);
            int value = pieceTypeValue(handPieceToPT(hp));
            material += bCount * value;
            material -= wCount * value;
        }

        // 玉の安全度
        int safety = kingSafety(pos, blackKingSq, Black) - kingSafety(pos, whiteKingSq, White);

        int score = material + positional + safety;

        if (pos.turn() == White)
        {
            score = -score;
        }
        return score;
    }

    // 手の並び替え: 取る手 → 王手 → その他
    std::vector<int> generateOrderedMoves(__Board &board)
    {
        std::vector<int> captures;
        std::vector<int> checks;
        std::vector<int> quiet;

        for (MoveList<LegalAll> ml(board.pos); !ml.end(); ++ml)
        {
            int move = ml.move().value();
            Square to = (Square)((move >> 0) & 0x7f);
            Piece captured = board.pos.piece(to);

            if (captured != Empty)
            {
                captures.push_back(move);
            }
            else
            {
                board.push(move);
                bool isCheck = board.pos.inCheck();
                board.pop();
                if (isCheck)
                {
                    checks.push_back(move);
                }
                else
                {
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
    int negamax(__Board &board, int depth, int alpha, int beta, int &outBestMove)
    {
        ++nodes;
        outBestMove = 0;

        if (stopSearch)
            return 0;
        if (isTimeUp())
        {
            stopSearch = true;
            return 0;
        }

        if (depth == 0)
        {
            return quiescence(board, 0, alpha, beta);
        }

        if (board.is_game_over())
        {
            return -999999 + (30 - depth);
        }

        int bestScore = -1000000000;

        auto moves = generateOrderedMoves(board);
        for (int move : moves)
        {
            board.push(move);
            int dummy = 0;
            int score = -negamax(board, depth - 1, -beta, -alpha, dummy);
            board.pop();

            if (stopSearch)
                return 0;

            if (score > bestScore)
            {
                bestScore = score;
                outBestMove = move;
            }
            if (score > alpha)
            {
                alpha = score;
            }
            if (alpha >= beta)
            {
                break;
            }
        }

        return bestScore;
    }

    // 静止探索 (時間切れ対応)
    int quiescence(__Board &board, int depth, int alpha, int beta)
    {
        ++nodes;

        if (stopSearch)
            return 0;
        if (isTimeUp())
        {
            stopSearch = true;
            return 0;
        }

        if (board.is_game_over())
        {
            return -999999;
        }

        int standPat = evaluate(board);
        if (standPat >= beta)
            return beta;
        if (standPat > alpha)
            alpha = standPat;

        int bestScore = standPat;

        if (depth < 3)
        {
            std::vector<std::pair<int, int>> captures;

            auto moves = generateOrderedMoves(board);
            for (int move : moves)
            {
                Square to = (Square)((move >> 0) & 0x7f);
                Piece captured = board.pos.piece(to);
                if (captured == Empty)
                    continue;

                PieceType pt = pieceToPieceType(captured);
                int value = pieceTypeValue(pt);
                captures.push_back({value, move});
            }

            std::sort(captures.begin(), captures.end(),
                      [](const auto &a, const auto &b)
                      { return a.first > b.first; });

            for (auto &[val, move] : captures)
            {
                if (!board.moveIsLegal(move))
                    continue;

                board.push(move);
                int score = -quiescence(board, depth + 1, -beta, -alpha);
                board.pop();

                if (stopSearch)
                    return 0;

                if (score > bestScore)
                    bestScore = score;
                if (score > alpha)
                    alpha = score;
                if (alpha >= beta)
                    break;
            }
        }

        return bestScore;
    }

    // 詰み探索
    int findMate(__Board &board, int depth)
    {
        if (depth <= 0)
            return 0;

        for (MoveList<LegalAll> ml(board.pos); !ml.end(); ++ml)
        {
            int move = ml.move().value();
            board.push(move);
            if (isForcedMate(board, depth - 1, false))
            {
                board.pop();
                return move;
            }
            board.pop();
        }
        return 0;
    }

    bool isForcedMate(__Board &board, int depth, bool attacker)
    {
        if (depth <= 0)
            return false;

        MoveList<LegalAll> ml(board.pos);
        if (ml.size() == 0)
        {
            return board.pos.inCheck();
        }

        if (attacker)
        {
            for (MoveList<LegalAll> ml2(board.pos); !ml2.end(); ++ml2)
            {
                int move = ml2.move().value();
                board.push(move);
                if (isForcedMate(board, depth - 1, false))
                {
                    board.pop();
                    return true;
                }
                board.pop();
            }
            return false;
        }

        for (MoveList<LegalAll> ml2(board.pos); !ml2.end(); ++ml2)
        {
            int move = ml2.move().value();
            board.push(move);
            if (!isForcedMate(board, depth - 1, true))
            {
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
class USIHandler
{
public:
    USIHandler() : ply(0) {}

    void run()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
            {
                line.pop_back();
            }
            if (line.empty())
                continue;

            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;

            if (cmd == "usi")
            {
                std::cout << "id name ShogiAlgo-cpp" << std::endl;
                std::cout << "id author kimura" << std::endl;
                std::cout << "usiok" << std::endl;
            }
            else if (cmd == "isready")
            {
                std::cout << "readyok" << std::endl;
            }
            else if (cmd == "usinewgame")
            {
                board = __Board();
            }
            else if (cmd == "position")
            {
                handlePosition(iss);
            }
            else if (cmd == "go")
            {
                handleGo(iss);
            }
            else if (cmd == "quit")
            {
                break;
            }
        }
    }

private:
    __Board board;
    ShogiEngine engine;
    int ply; // 現在の手数（定跡判定用）

    void handlePosition(std::istringstream &iss)
    {
        std::string rest;
        std::getline(iss, rest);
        if (!rest.empty() && rest[0] == ' ')
        {
            rest = rest.substr(1);
        }

        // 手数をカウント（"moves" 以降のトークン数）
        ply = 0;
        size_t movesPos = rest.find("moves");
        if (movesPos != std::string::npos)
        {
            std::istringstream ms(rest.substr(movesPos + 5));
            std::string m;
            while (ms >> m)
                ply++;
        }

        try
        {
            board.set_position(rest);
        }
        catch (...)
        {
            board = __Board();
            ply = 0;
        }
    }

    void handleGo(std::istringstream &iss)
    {
        // go コマンドのパラメータを解析
        int btime = 0, wtime = 0, byoyomi = 0, binc = 0, winc = 0;
        bool hasTime = false;

        std::string token;
        while (iss >> token)
        {
            if (token == "btime")
            {
                iss >> btime;
                hasTime = true;
            }
            else if (token == "wtime")
            {
                iss >> wtime;
                hasTime = true;
            }
            else if (token == "byoyomi")
            {
                iss >> byoyomi;
                hasTime = true;
            }
            else if (token == "binc")
            {
                iss >> binc;
                hasTime = true;
            }
            else if (token == "winc")
            {
                iss >> winc;
                hasTime = true;
            }
        }

        int timeLimitMs;
        if (hasTime)
        {
            int myTime = (board.pos.turn() == Black) ? btime : wtime;
            int myInc = (board.pos.turn() == Black) ? binc : winc;

            if (byoyomi > 0)
            {
                // 秒読み: 残り時間の1/50 + 秒読みの80%
                timeLimitMs = myTime / 50 + byoyomi * 80 / 100;
                if (timeLimitMs > 5000)
                    timeLimitMs = 5000;
            }
            else if (myInc > 0)
            {
                // フィッシャー (floodgate-600-10F 等):
                // 加算時間を基本に、残り時間の一部を上乗せ
                timeLimitMs = myInc + myTime / 60;
                int maxTime = std::min(myTime / 3, myInc * 3);
                if (timeLimitMs > maxTime)
                    timeLimitMs = maxTime;
                // 残り時間に余裕があれば、少なくとも加算時間分は考える
                if (myTime > myInc * 2 && timeLimitMs < myInc)
                {
                    timeLimitMs = myInc;
                }
            }
            else
            {
                // 切れ負け: 残り時間の1/30
                timeLimitMs = myTime / 30;
                if (timeLimitMs > 5000)
                    timeLimitMs = 5000;
            }

            if (timeLimitMs < 200)
                timeLimitMs = 200;

            // 序盤は時間を節約して中終盤に回す
            if (ply < 30)
            {
                // ply 6(定跡直後)で30%, ply 30で100%に線形補間
                int pct = 30 + (ply * 70 / 30);
                if (pct > 100)
                    pct = 100;
                timeLimitMs = timeLimitMs * pct / 100;
                if (timeLimitMs < 200)
                    timeLimitMs = 200;
            }

            // 残り時間が少ない場合はさらに短く（byoyomiのみの場合は除外）
            if (myTime > 0 && myTime < 10000)
            {
                timeLimitMs = std::min(timeLimitMs, myTime / 5);
                if (timeLimitMs < 100)
                    timeLimitMs = 100;
            }
        }
        else
        {
            timeLimitMs = 2000;
        }

        auto result = engine.searchWithTimeLimit(board, timeLimitMs);

        if (result.move != 0)
        {
            std::string moveUsi = Move(result.move).toUSI();
            std::cout << "info depth " << result.depth
                      << " score cp " << result.score
                      << " nodes " << result.nodes << std::endl;
            std::cout << "bestmove " << moveUsi << std::endl;
        }
        else
        {
            std::cout << "bestmove resign" << std::endl;
        }
    }
};

// ============================================================
// main
// ============================================================
int main()
{
    initTable();

    USIHandler handler;
    handler.run();

    return 0;
}
