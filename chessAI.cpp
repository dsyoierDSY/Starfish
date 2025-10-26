//Starfish --- chess engine developed by dsyoier
//Version 2.5 (Bookworm)
//FEN Book Edition - Final Corrected Version
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <cctype>   // for ::tolower
#include <limits>   // for numeric_limits
#include <fstream>  // for file I/O
#include <sstream>  // for string streams
#include <cstdint>  // for uint64_t

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#define Sleep(x) usleep((x)*1000)
#endif

using namespace std;

// --- 核心定义与数据结构 (无变化) ---
const int WHITE = 0;
const int BLACK = 1;
const int NONE = 2;
const int EMPTY_PIECE = 0;
const int PAWN = 1, KNIGHT = 2, BISHOP = 3, ROOK = 4, QUEEN = 5, KING = 6;
const int PIECE_TYPE_MASK = 0b0111;
const int COLOR_MASK = 0b1000;
const int W_PAWN = PAWN, W_KNIGHT = KNIGHT, W_BISHOP = BISHOP, W_ROOK = ROOK, W_QUEEN = QUEEN, W_KING = KING;
const int B_PAWN = PAWN | COLOR_MASK, B_KNIGHT = KNIGHT | COLOR_MASK, B_BISHOP = BISHOP | COLOR_MASK, B_ROOK = ROOK | COLOR_MASK, B_QUEEN = QUEEN | COLOR_MASK, B_KING = KING | COLOR_MASK;
struct Pos {
    int x, y;
    Pos(int xx = 0, int yy = 0) : x(xx), y(yy) {}
    bool ok() const { return x >= 1 && x <= 8 && y >= 1 && y <= 8; }
    bool operator==(const Pos& other) const { return x == other.x && y == other.y; }
};
struct Move {
    Pos from, to;
    int promotion = EMPTY_PIECE;
    bool operator==(const Move& other) const {
        return from == other.from && to == other.to && promotion == other.promotion;
    }
};

// --- 全局游戏状态 ---
int board[10][10];
int currentPlayer;
Pos enPassantTarget;
int castlingRights;
int Round;
int halfmoveClock;
const int WK_CASTLE = 1, WQ_CASTLE = 2, BK_CASTLE = 4, BQ_CASTLE = 8;

struct UndoInfo {
    int capturedPiece;
    Pos enPassantTarget;
    int castlingRights;
    int halfmoveClock;
};

// --- 棋子静态数据 (无变化) ---
int pieceValue[] = {0, 100, 375, 397, 613, 1220, 20000};
map<int, char> pieceChar = {
    {W_PAWN, 'P'}, {W_KNIGHT, 'N'}, {W_BISHOP, 'B'}, {W_ROOK, 'R'}, {W_QUEEN, 'Q'}, {W_KING, 'K'},
    {B_PAWN, 'p'}, {B_KNIGHT, 'n'}, {B_BISHOP, 'b'}, {B_ROOK, 'r'}, {B_QUEEN, 'q'}, {B_KING, 'k'},
    {EMPTY_PIECE, '.'}
};
Pos knight_deltas[] = {{1,2}, {1,-2}, {-1,2}, {-1,-2}, {2,1}, {2,-1}, {-2,1}, {-2,-1}};
Pos bishop_deltas[] = {{1,1}, {1,-1}, {-1,1}, {-1,-1}};
Pos rook_deltas[]   = {{1,0}, {-1,0}, {0,1}, {0,-1}};
Pos king_deltas[]   = {{1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {1,-1}, {-1,1}, {-1,-1}};

// --- 棋子位置表 (Piece-Square Tables) (无变化) ---
const int pawn_pst_mg[64] = {0,0,0,0,0,0,0,0,50,50,50,50,50,50,50,50,10,10,20,30,30,20,10,10,5,5,10,25,25,10,5,5,0,0,0,20,20,0,0,0,5,-5,-10,0,0,-10,-5,5,5,10,10,-20,-20,10,10,5,0,0,0,0,0,0,0,0};
const int pawn_pst_eg[64] = {0,0,0,0,0,0,0,0,80,80,80,80,80,80,80,80,50,50,50,50,50,50,50,50,30,30,30,30,30,30,30,30,20,20,20,20,20,20,20,20,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,0,0,0,0,0,0,0,0};
const int knight_pst_mg[64] = {-50,-40,-30,-30,-30,-30,-40,-50,-40,-20,0,0,0,0,-20,-40,-30,0,10,15,15,10,0,-30,-30,5,15,20,20,15,5,-30,-30,0,15,20,20,15,0,-30,-30,5,10,15,15,10,5,-30,-40,-20,0,5,5,0,-20,-40,-50,-40,-30,-30,-30,-30,-40,-50};
const int knight_pst_eg[64] = {-50,-30,-20,-20,-20,-20,-30,-50,-30,-10,0,5,5,0,-10,-30,-20,0,5,10,10,5,0,-20,-20,5,10,15,15,10,5,-20,-20,5,10,15,15,10,5,-20,-20,0,5,10,10,5,0,-20,-30,-10,0,5,5,0,-10,-30,-50,-30,-20,-20,-20,-20,-30,-50};
const int bishop_pst_mg[64] = {-20,-10,-10,-10,-10,-10,-10,-20,-10,0,0,0,0,0,0,-10,-10,0,5,10,10,5,0,-10,-10,5,5,10,10,5,5,-10,-10,0,10,10,10,10,0,-10,-10,10,10,10,10,10,10,-10,-10,5,0,0,0,0,5,-10,-20,-10,-10,-10,-10,-10,-10,-20};
const int bishop_pst_eg[64] = {-20,-10,-10,-10,-10,-10,-10,-20,-10,0,5,0,0,5,0,-10,-10,5,5,5,5,5,5,-10,-10,0,5,5,5,5,0,-10,-10,5,5,5,5,5,5,-10,-10,0,5,0,0,5,0,-10,-10,0,0,0,0,0,0,-10,-20,-10,-10,-10,-10,-10,-10,-20};
const int rook_pst_mg[64] = {0,0,0,0,0,0,0,0,5,10,10,10,10,10,10,5,-5,0,0,0,0,0,0,-5,-5,0,0,0,0,0,0,-5,-5,0,0,0,0,0,0,-5,-5,0,0,0,0,0,0,-5,-5,0,0,0,0,0,0,-5,0,0,0,5,5,0,0,0};
const int rook_pst_eg[64] = {10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,10,10,10,10,10,10,10,10};
const int queen_pst_mg[64] = {-20,-10,-10,-5,-5,-10,-10,-20,-10,0,0,0,0,0,0,-10,-10,0,5,5,5,5,0,-10,-5,0,5,5,5,5,0,-5,0,0,5,5,5,5,0,-5,-10,5,5,5,5,5,0,-10,-10,0,5,0,0,0,0,-10,-20,-10,-10,-5,-5,-10,-10,-20};
const int queen_pst_eg[64] = {-20,-10,-10,-5,-5,-10,-10,-20,-10,0,0,0,0,0,0,-10,-10,0,5,5,5,5,0,-10,-5,0,5,5,5,5,0,-5,0,0,5,5,5,5,0,-5,-10,5,5,5,5,5,0,-10,-10,0,5,0,0,0,0,-10,-20,-10,-10,-5,-5,-10,-10,-20};
const int king_pst_mg[64] = {-30,-40,-40,-50,-50,-40,-40,-30,-30,-40,-40,-50,-50,-40,-40,-30,-30,-40,-40,-50,-50,-40,-40,-30,-30,-40,-40,-50,-50,-40,-40,-30,-20,-30,-30,-40,-40,-30,-30,-20,-10,-20,-20,-20,-20,-20,-20,-10,20,20,0,0,0,0,20,20,20,30,10,0,0,10,30,20};
const int king_pst_eg[64] = {-50,-40,-30,-20,-20,-30,-40,-50,-30,-20,-10,0,0,-10,-20,-30,-30,-10,20,30,30,20,-10,-30,-30,-10,30,40,40,30,-10,-30,-30,-10,30,40,40,30,-10,-30,-30,-10,20,30,30,20,-10,-30,-30,-30,0,0,0,0,-30,-30,-50,-30,-30,-30,-30,-30,-30,-50};
inline int PieceColorOf(int piece);


// --- FEN 开局库 (无变化) ---
map<string, string> openingBook;
string GenerateFEN() {
    stringstream fen;
    for (int y = 8; y >= 1; --y) {
        int empty_count = 0;
        for (int x = 1; x <= 8; ++x) {
            int piece = board[x][y];
            if (piece == EMPTY_PIECE) { empty_count++; } else {
                if (empty_count > 0) { fen << empty_count; empty_count = 0; }
                fen << pieceChar[piece];
            }
        }
        if (empty_count > 0) { fen << empty_count; }
        if (y > 1) { fen << '/'; }
    }
    fen << (currentPlayer == WHITE ? " w " : " b ");
    string castle_str;
    if (castlingRights & WK_CASTLE) castle_str += 'K';
    if (castlingRights & WQ_CASTLE) castle_str += 'Q';
    if (castlingRights & BK_CASTLE) castle_str += 'k';
    if (castlingRights & BQ_CASTLE) castle_str += 'q';
    fen << (castle_str.empty() ? "-" : castle_str) << " ";
    if (enPassantTarget.ok()) { fen << (char)('a' + enPassantTarget.x - 1) << (char)('0' + enPassantTarget.y); } else { fen << "-"; }
    fen << " " << halfmoveClock << " " << Round;
    return fen.str();
}
void LoadOpeningBook() {
    ifstream bookFile("opening_book.txt");
    if (!bookFile.is_open()) { cout << "info string Opening book file 'opening_book.txt' not found." << endl; return; }
    string line;
    int count = 0;
    while (getline(bookFile, line)) {
        size_t first_space = line.find(' ');
        if (first_space != string::npos) {
            size_t last_space = line.rfind(' ');
            if(last_space != string::npos && last_space > first_space) {
                string fen = line.substr(0, last_space);
                string uci_move = line.substr(last_space + 1);
                openingBook[fen] = uci_move;
                count++;
            }
        }
    }
    cout << "info string Loaded " << count << " positions from FEN opening book." << endl;
    bookFile.close();
}


// --- 辅助函数 (无变化) ---
inline int PieceColorOf(int piece) {
    if (piece == EMPTY_PIECE) return NONE;
    return ( (piece & COLOR_MASK) ? BLACK : WHITE );
}

// --- 核心功能函数 (无变化) ---
void GenerateMoves(vector<Move>& moves, bool capturesOnly = false);
void SetBoard() {
    memset(board, 0, sizeof(board));
    board[1][1]=W_ROOK; board[2][1]=W_KNIGHT; board[3][1]=W_BISHOP; board[4][1]=W_QUEEN; board[5][1]=W_KING; board[6][1]=W_BISHOP; board[7][1]=W_KNIGHT; board[8][1]=W_ROOK;
    for (int i=1; i<=8; i++) board[i][2]=W_PAWN;
    board[1][8]=B_ROOK; board[2][8]=B_KNIGHT; board[3][8]=B_BISHOP; board[4][8]=B_QUEEN; board[5][8]=B_KING; board[6][8]=B_BISHOP; board[7][8]=B_KNIGHT; board[8][8]=B_ROOK;
    for (int i=1; i<=8; i++) board[i][7]=B_PAWN;
    currentPlayer = WHITE; castlingRights = WK_CASTLE | WQ_CASTLE | BK_CASTLE | BQ_CASTLE; enPassantTarget = Pos(0,0); Round = 1;
    halfmoveClock = 0;
}
bool IsSquareAttacked(Pos p, int attackerCamp) {
    int pawnDir = (attackerCamp == WHITE) ? 1 : -1;
    Pos a1 = {p.x - 1, p.y - pawnDir};
    if (a1.ok() && board[a1.x][a1.y] == (PAWN | (attackerCamp * COLOR_MASK))) return true;
    Pos a2 = {p.x + 1, p.y - pawnDir};
    if (a2.ok() && board[a2.x][a2.y] == (PAWN | (attackerCamp * COLOR_MASK))) return true;
    for (const auto& d : knight_deltas) {
        Pos target = {p.x + d.x, p.y + d.y};
        if (target.ok() && board[target.x][target.y] == (KNIGHT | (attackerCamp * COLOR_MASK))) return true;
    }
    for (const auto& d : king_deltas) {
        Pos target = {p.x + d.x, p.y + d.y};
        if (target.ok() && board[target.x][target.y] == (KING | (attackerCamp * COLOR_MASK))) return true;
    }
    for (const auto& d : rook_deltas) {
        for (int i = 1; i < 8; i++) {
            Pos target = {p.x + d.x * i, p.y + d.y * i};
            if (!target.ok()) break;
            int piece = board[target.x][target.y];
            if (piece != EMPTY_PIECE) {
                if (PieceColorOf(piece) == attackerCamp) {
                    int t = piece & PIECE_TYPE_MASK;
                    if (t == ROOK || t == QUEEN) return true;
                }
                break;
            }
        }
    }
    for (const auto& d : bishop_deltas) {
        for (int i = 1; i < 8; i++) {
            Pos target = {p.x + d.x * i, p.y + d.y * i};
            if (!target.ok()) break;
            int piece = board[target.x][target.y];
            if (piece != EMPTY_PIECE) {
                if (PieceColorOf(piece) == attackerCamp) {
                    int t = piece & PIECE_TYPE_MASK;
                    if (t == BISHOP || t == QUEEN) return true;
                }
                break;
            }
        }
    }
    return false;
}
void GenerateMoves(vector<Move>& moves, bool capturesOnly) {
    moves.clear();
    int pawnDir = (currentPlayer == WHITE) ? 1 : -1;
    int startRank = (currentPlayer == WHITE) ? 2 : 7;
    int promotionRank = (currentPlayer == WHITE) ? 8 : 1;
    for (int x = 1; x <= 8; x++) {
        for (int y = 1; y <= 8; y++) {
            int piece = board[x][y];
            if (piece == EMPTY_PIECE || PieceColorOf(piece) != currentPlayer) continue;
            int pieceColor = PieceColorOf(piece);
            int pieceType = piece & PIECE_TYPE_MASK;
            Pos from = {x, y};
            switch (pieceType) {
                case PAWN: {
                    Pos to1 = {x, y + pawnDir};
                    if (!capturesOnly && to1.ok() && board[to1.x][to1.y] == EMPTY_PIECE) {
                        if (to1.y == promotionRank) {
                            moves.push_back({from, to1, QUEEN | (pieceColor * COLOR_MASK)}); moves.push_back({from, to1, ROOK | (pieceColor * COLOR_MASK)}); moves.push_back({from, to1, BISHOP | (pieceColor * COLOR_MASK)}); moves.push_back({from, to1, KNIGHT | (pieceColor * COLOR_MASK)});
                        } else { moves.push_back({from, to1}); }
                    }
                    if (!capturesOnly && y == startRank && board[x][y + pawnDir] == EMPTY_PIECE && board[x][y + 2 * pawnDir] == EMPTY_PIECE) {
                        moves.push_back({from, {x, y + 2 * pawnDir}});
                    }
                    Pos to_eat[] = {{x - 1, y + pawnDir}, {x + 1, y + pawnDir}};
                    for (const auto& to_e : to_eat) {
                        if (to_e.ok()) {
                            int targetPiece = board[to_e.x][to_e.y];
                            if (targetPiece != EMPTY_PIECE && PieceColorOf(targetPiece) != currentPlayer) {
                                if (to_e.y == promotionRank) {
                                    moves.push_back({from, to_e, QUEEN | (pieceColor * COLOR_MASK)}); moves.push_back({from, to_e, ROOK | (pieceColor * COLOR_MASK)}); moves.push_back({from, to_e, BISHOP | (pieceColor * COLOR_MASK)}); moves.push_back({from, to_e, KNIGHT | (pieceColor * COLOR_MASK)});
                                } else { moves.push_back({from, to_e}); }
                            }
                        }
                    }
                    if (enPassantTarget.ok() && ((abs(x - enPassantTarget.x) == 1) && (y + pawnDir == enPassantTarget.y))) {
                         moves.push_back({from, enPassantTarget});
                    }
                    break;
                }
                case KNIGHT: case KING: {
                    Pos* deltas = (pieceType == KNIGHT) ? knight_deltas : king_deltas;
                    for (int i = 0; i < 8; i++) {
                        Pos to = {x + deltas[i].x, y + deltas[i].y};
                        if (to.ok()) {
                            int targetPiece = board[to.x][to.y];
                            if (targetPiece == EMPTY_PIECE) {
                                if (!capturesOnly) moves.push_back({from, to});
                            } else if (PieceColorOf(targetPiece) != currentPlayer) {
                                moves.push_back({from, to});
                            }
                        }
                    }
                    if (!capturesOnly && pieceType == KING) {
                        int rank = (currentPlayer == WHITE) ? 1 : 8;
                        if (!IsSquareAttacked({x,y}, 1 - currentPlayer)) {
                            if ((castlingRights & (currentPlayer == WHITE ? WK_CASTLE : BK_CASTLE)) && board[x + 1][rank] == EMPTY_PIECE && board[x + 2][rank] == EMPTY_PIECE && !IsSquareAttacked({x + 1, rank}, 1 - currentPlayer) && !IsSquareAttacked({x + 2, rank}, 1 - currentPlayer)) {
                                moves.push_back({from, {x + 2, rank}});
                            }
                            if ((castlingRights & (currentPlayer == WHITE ? WQ_CASTLE : BQ_CASTLE)) && board[x - 1][rank] == EMPTY_PIECE && board[x - 2][rank] == EMPTY_PIECE && board[x - 3][rank] == EMPTY_PIECE && !IsSquareAttacked({x - 1, rank}, 1 - currentPlayer) && !IsSquareAttacked({x - 2, rank}, 1 - currentPlayer)) {
                                moves.push_back({from, {x - 2, rank}});
                            }
                        }
                    }
                    break;
                }
                case BISHOP: case ROOK: case QUEEN: {
                    Pos* deltas; int end_idx = 0;
                    if (pieceType == BISHOP) { deltas = bishop_deltas; end_idx = 4; } else if (pieceType == ROOK) { deltas = rook_deltas; end_idx = 4; } else { deltas = king_deltas; end_idx = 8; }
                    for (int i = 0; i < end_idx; i++) {
                        for (int j = 1; j < 8; j++) {
                            Pos to = {x + deltas[i].x * j, y + deltas[i].y * j};
                            if (!to.ok()) break;
                            int targetPiece = board[to.x][to.y];
                            if (targetPiece == EMPTY_PIECE) {
                                if (!capturesOnly) moves.push_back({from, to});
                            } else {
                                if (PieceColorOf(targetPiece) != currentPlayer) { moves.push_back({from, to}); }
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
}
UndoInfo MakeMove(const Move& move) {
    UndoInfo undo;
    undo.capturedPiece = board[move.to.x][move.to.y];
    undo.enPassantTarget = enPassantTarget;
    undo.castlingRights = castlingRights;
    undo.halfmoveClock = halfmoveClock;
    int piece = board[move.from.x][move.from.y];
    int pieceType = piece & PIECE_TYPE_MASK;
    int pieceColor = PieceColorOf(piece);
    if (pieceType == PAWN || undo.capturedPiece != EMPTY_PIECE) { halfmoveClock = 0; } else { halfmoveClock++; }
    board[move.to.x][move.to.y] = piece;
    board[move.from.x][move.from.y] = EMPTY_PIECE;
    enPassantTarget = Pos(0, 0);
    if (pieceType == PAWN) {
        if (abs(move.to.y - move.from.y) == 2) {
            enPassantTarget = Pos(move.from.x, move.from.y + ((pieceColor == WHITE) ? 1 : -1));
        } else if (move.to == undo.enPassantTarget && undo.enPassantTarget.ok()) {
            int capturedPawnY = move.to.y + ((pieceColor == WHITE) ? -1 : 1);
            undo.capturedPiece = board[move.to.x][capturedPawnY];
            board[move.to.x][capturedPawnY] = EMPTY_PIECE;
        } else if (move.promotion != EMPTY_PIECE) {
            board[move.to.x][move.to.y] = move.promotion;
        }
    } else if (pieceType == KING) {
        if (abs(move.to.x - move.from.x) == 2) {
            int rank = (pieceColor == WHITE) ? 1 : 8;
            if (move.to.x == 7) { board[6][rank] = board[8][rank]; board[8][rank] = EMPTY_PIECE;
            } else { board[4][rank] = board[1][rank]; board[1][rank] = EMPTY_PIECE; }
        }
    }
    if (piece == W_KING) castlingRights &= ~(WK_CASTLE | WQ_CASTLE);
    else if (piece == B_KING) castlingRights &= ~(BK_CASTLE | BQ_CASTLE);
    else if (piece == W_ROOK && move.from.x == 1 && move.from.y == 1) castlingRights &= ~WQ_CASTLE;
    else if (piece == W_ROOK && move.from.x == 8 && move.from.y == 1) castlingRights &= ~WK_CASTLE;
    else if (piece == B_ROOK && move.from.x == 1 && move.from.y == 8) castlingRights &= ~BQ_CASTLE;
    else if (piece == B_ROOK && move.from.x == 8 && move.from.y == 8) castlingRights &= ~BK_CASTLE;
    if (undo.capturedPiece == W_ROOK && move.to.x == 1 && move.to.y == 1) castlingRights &= ~WQ_CASTLE;
    if (undo.capturedPiece == W_ROOK && move.to.x == 8 && move.to.y == 1) castlingRights &= ~WK_CASTLE;
    if (undo.capturedPiece == B_ROOK && move.to.x == 1 && move.to.y == 8) castlingRights &= ~BQ_CASTLE;
    if (undo.capturedPiece == B_ROOK && move.to.x == 8 && move.to.y == 8) castlingRights &= ~BK_CASTLE;
    currentPlayer = 1 - currentPlayer;
    if (currentPlayer == WHITE) { Round++; }
    return undo;
}
void UnmakeMove(const Move& move, const UndoInfo& undo) {
    currentPlayer = 1 - currentPlayer;
    int movedPiece;
    if (move.promotion != EMPTY_PIECE) { movedPiece = PAWN | (currentPlayer * COLOR_MASK); } else { movedPiece = board[move.to.x][move.to.y]; }
    board[move.from.x][move.from.y] = movedPiece;
    board[move.to.x][move.to.y] = undo.capturedPiece;
    int movedPieceType = movedPiece & PIECE_TYPE_MASK;
    if (movedPieceType == PAWN && move.to == undo.enPassantTarget && undo.enPassantTarget.ok()) {
        board[move.to.x][move.to.y] = EMPTY_PIECE;
        int capturedPawnY = move.to.y + ((currentPlayer == WHITE) ? -1 : 1);
        board[move.to.x][capturedPawnY] = undo.capturedPiece;
    } else if (movedPieceType == KING && abs(move.to.x - move.from.x) == 2) {
        int rank = (currentPlayer == WHITE) ? 1 : 8;
        if (move.to.x == 7) { board[8][rank] = board[6][rank]; board[6][rank] = EMPTY_PIECE;
        } else { board[1][rank] = board[4][rank]; board[4][rank] = EMPTY_PIECE; }
    }
    castlingRights = undo.castlingRights;
    enPassantTarget = undo.enPassantTarget;
    halfmoveClock = undo.halfmoveClock;
    if (currentPlayer == BLACK) { Round--; }
}

// --- 评估函数 (无变化) ---
double GetPSTValue(int piece, int x, int y) {
    int piece_type = piece & PIECE_TYPE_MASK;
    int color = PieceColorOf(piece);
    int square_idx = (y - 1) * 8 + (x - 1);
    if (color == BLACK) { square_idx = 63 - square_idx; }
    int total_pieces = 0;
    for (int i = 1; i <= 8; ++i) for (int j = 1; j <= 8; ++j) if(board[i][j]!=EMPTY_PIECE) total_pieces++;
    double phase = min(1.0, (total_pieces - 8) / 24.0);
    const int* mg_table = nullptr; const int* eg_table = nullptr;
    switch (piece_type) {
        case PAWN:   mg_table = pawn_pst_mg;   eg_table = pawn_pst_eg;   break;
        case KNIGHT: mg_table = knight_pst_mg; eg_table = knight_pst_eg; break;
        case BISHOP: mg_table = bishop_pst_mg; eg_table = bishop_pst_eg; break;
        case ROOK:   mg_table = rook_pst_mg;   eg_table = rook_pst_eg;   break;
        case QUEEN:  mg_table = queen_pst_mg;  eg_table = queen_pst_eg;  break;
        case KING:   mg_table = king_pst_mg;   eg_table = king_pst_eg;   break;
        default: return 0;
    }
    double mg_score = mg_table[square_idx]; double eg_score = eg_table[square_idx];
    return (mg_score * phase) + (eg_score * (1.0 - phase));
}
double Evaluate() {
    double score = 0;
    for (int x = 1; x <= 8; x++) {
        for (int y = 1; y <= 8; y++) {
            int piece = board[x][y];
            if (piece == EMPTY_PIECE) continue;
            int pieceColor = PieceColorOf(piece);
            int pieceType = piece & PIECE_TYPE_MASK;
            double current_score = pieceValue[pieceType] + GetPSTValue(piece, x, y);
            score += (pieceColor == WHITE) ? current_score : -current_score;
        }
    }
    return score;
}

// --- AI 搜索 (无变化) ---
Move searchBestMove;
long long computedNodes;
chrono::time_point<chrono::high_resolution_clock> searchStartTime;
int searchTimeLimitMs;
double QuiescenceSearch(double alpha, double beta);
int scoreMove(const Move& move) {
    int score = 0;
    int capturedPieceType = board[move.to.x][move.to.y] & PIECE_TYPE_MASK;
    if (capturedPieceType != EMPTY_PIECE) {
        int movingPieceType = board[move.from.x][move.from.y] & PIECE_TYPE_MASK;
        score = 10000 + pieceValue[capturedPieceType] - pieceValue[movingPieceType];
    }
    if (move.promotion != EMPTY_PIECE) { score += pieceValue[move.promotion & PIECE_TYPE_MASK]; }
    return score;
}
double AlphaBetaSearch(int depth, double alpha, double beta) {
    auto now = chrono::high_resolution_clock::now();
    if (chrono::duration_cast<chrono::milliseconds>(now - searchStartTime).count() > searchTimeLimitMs) { return 0; }
    if (depth == 0) { return QuiescenceSearch(alpha, beta); }

    vector<Move> moves;
    GenerateMoves(moves, false);
    vector<Move> legal_moves;
    Pos kingPos;
    for(int x=1; x<=8; ++x) for(int y=1; y<=8; ++y) {
        if(board[x][y] == (KING | (currentPlayer * COLOR_MASK))) { kingPos = {x,y}; break; }
    }
    for (const auto& m : moves) {
        Pos kingPos_after = kingPos;
        if ((board[m.from.x][m.from.y] & PIECE_TYPE_MASK) == KING) kingPos_after = m.to;
        UndoInfo undo = MakeMove(m);
        if (!IsSquareAttacked(kingPos_after, currentPlayer)) { 
            legal_moves.push_back(m);
        }
        UnmakeMove(m, undo);
    }

    if (legal_moves.empty()) {
        if (IsSquareAttacked(kingPos, 1 - currentPlayer)) { return -1e9 - depth; } 
        else { return 0; }
    }
    sort(legal_moves.begin(), legal_moves.end(), [&](const Move& a, const Move& b) { return scoreMove(a) > scoreMove(b); });
    double bestScore = -numeric_limits<double>::infinity();
    for (const auto& m : legal_moves) {
        UndoInfo undo = MakeMove(m);
        computedNodes++;
        double score = -AlphaBetaSearch(depth - 1, -beta, -alpha);
        UnmakeMove(m, undo);
        if (chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - searchStartTime).count() > searchTimeLimitMs) { return 0; }
        if (score > bestScore) { bestScore = score; }
        if (bestScore > alpha) { alpha = bestScore; }
        if (alpha >= beta) { break; }
    }
    return bestScore;
}
double QuiescenceSearch(double alpha, double beta) {
    auto now = chrono::high_resolution_clock::now();
    if (chrono::duration_cast<chrono::milliseconds>(now - searchStartTime).count() > searchTimeLimitMs) { return 0; }

    double stand_pat = (currentPlayer == WHITE ? Evaluate() : -Evaluate());
    if (stand_pat >= beta) { return beta; }
    if (alpha < stand_pat) { alpha = stand_pat; }

    vector<Move> capture_moves;
    GenerateMoves(capture_moves, true);
    vector<Move> legal_captures;
    Pos kingPos;
    for(int x=1; x<=8; ++x) for(int y=1; y<=8; ++y) {
        if(board[x][y] == (KING | (currentPlayer * COLOR_MASK))) { kingPos = {x,y}; break; }
    }
     for (const auto& m : capture_moves) {
        Pos kingPos_after = kingPos;
        if ((board[m.from.x][m.from.y] & PIECE_TYPE_MASK) == KING) kingPos_after = m.to;
        UndoInfo undo = MakeMove(m);
        if (!IsSquareAttacked(kingPos_after, currentPlayer)) { 
            legal_captures.push_back(m);
        }
        UnmakeMove(m, undo);
    }
    sort(legal_captures.begin(), legal_captures.end(), [&](const Move& a, const Move& b) { return scoreMove(a) > scoreMove(b); });
    for (const auto& m : legal_captures) {
        UndoInfo undo = MakeMove(m);
        computedNodes++;
        double score = -QuiescenceSearch(-beta, -alpha);
        UnmakeMove(m, undo);
        if (chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - searchStartTime).count() > searchTimeLimitMs) { return 0; }
        if (score >= beta) { return beta; }
        if (score > alpha) { alpha = score; }
    }
    return alpha;
}

// --- 修改部分 ---
void SearchBestMove() {
    searchStartTime = chrono::high_resolution_clock::now();
    computedNodes = 0;
    double bestScoreSoFar = -numeric_limits<double>::infinity();
    const int MAX_DEPTH = 64;
    vector<Move> moves;
    GenerateMoves(moves, false);
    vector<Move> legal_moves;
    Pos kingPos;
    for(int x=1; x<=8; ++x) for(int y=1; y<=8; ++y) if(board[x][y] == (KING | (currentPlayer * COLOR_MASK))) { kingPos = {x,y}; break; }
    for (const auto& m : moves) {
        Pos kingPos_after = kingPos;
        if ((board[m.from.x][m.from.y] & PIECE_TYPE_MASK) == KING) kingPos_after = m.to;
        UndoInfo undo = MakeMove(m);
        if (!IsSquareAttacked(kingPos_after, currentPlayer)) legal_moves.push_back(m);
        UnmakeMove(m, undo);
    }
    if (legal_moves.empty()) return;
    searchBestMove = legal_moves[0];
    
    // --- 修改：将迭代加深循环改为只搜索偶数层深度 ---
    // 原代码: for (int current_depth = 1; current_depth <= MAX_DEPTH; ++current_depth)
    for (int current_depth = 2; current_depth <= MAX_DEPTH; current_depth += 2) {
        double alpha = -numeric_limits<double>::infinity();
        double beta = numeric_limits<double>::infinity();
        sort(legal_moves.begin(), legal_moves.end(), [&](const Move& a, const Move& b) { return scoreMove(a) > scoreMove(b); });
        Move bestMoveThisIteration = legal_moves[0];
        double bestScoreThisIteration = -numeric_limits<double>::infinity();
        for (const auto& m : legal_moves) {
            UndoInfo undo = MakeMove(m);
            double score = -AlphaBetaSearch(current_depth - 1, -beta, -alpha);
            UnmakeMove(m, undo);
            auto now = chrono::high_resolution_clock::now();
            if (chrono::duration_cast<chrono::milliseconds>(now - searchStartTime).count() > searchTimeLimitMs) { goto search_end; }
            if (score > bestScoreThisIteration) {
                bestScoreThisIteration = score;
                bestMoveThisIteration = m;
                if (score > alpha) { alpha = score; }
            }
        }
        bestScoreSoFar = bestScoreThisIteration;
        searchBestMove = bestMoveThisIteration;
        auto end_time = chrono::high_resolution_clock::now();
        chrono::duration<double> diff = end_time - searchStartTime;
        cout << "info depth " << current_depth << " score cp " << static_cast<int>(currentPlayer == WHITE ? bestScoreSoFar : -bestScoreSoFar)
             << " nodes " << computedNodes << " time " << static_cast<int>(diff.count() * 1000) << "ms" << endl;
    }
search_end:
    return;
}
// --- 修改结束 ---

// --- 主函数和UI (无变化) ---
Move parse_uci_move(const string& uci_str) {
    Move move;
    move.from.x = uci_str[0] - 'a' + 1; move.from.y = uci_str[1] - '0';
    move.to.x = uci_str[2] - 'a' + 1; move.to.y = uci_str[3] - '0';
    if (uci_str.length() == 5) {
        char prom_char = tolower(uci_str[4]); int prom_piece_type = EMPTY_PIECE;
        if (prom_char == 'q') prom_piece_type = QUEEN; else if (prom_char == 'r') prom_piece_type = ROOK;
        else if (prom_char == 'b') prom_piece_type = BISHOP; else if (prom_char == 'n') prom_piece_type = KNIGHT;
        move.promotion = prom_piece_type | (currentPlayer * COLOR_MASK);
    }
    return move;
}
void PrintBoard() {
    cout << "\n   a  b  c  d  e  f  g  h" << endl;
    cout << "  -------------------------" << endl;
    for (int j = 8; j >= 1; j--) {
        cout << j << " |";
        for (int i = 1; i <= 8; i++) {
            char symbol = pieceChar[board[i][j]]; int piece = board[i][j];
            if (piece != EMPTY_PIECE) {
                int color = PieceColorOf(piece);
                if (color == WHITE) cout << " " << "\033[37m" << symbol << "\033[0m" << " ";
                else cout << " " << "\033[31m" << symbol << "\033[0m" << " ";
            } else { cout << " " << '.' << " "; }
        }
        cout << "| " << j << endl;
    }
    cout << "  -------------------------" << endl;
    cout << "   a  b  c  d  e  f  g  h\n" << endl;
}
int main() {
    #ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) { DWORD dwMode = 0; if (GetConsoleMode(hOut, &dwMode)) { dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING; SetConsoleMode(hOut, dwMode); }}
    #endif
    cout << "-------------------- StarFish Ver 2.5 (Bookworm) [Final Corrected] --------------------" << endl;
    cout << "Engine with FEN Opening Book, Iterative Deepening, and Quiescence Search." << endl;
    cout << "Oct, 2025 Build. Developed by dsyoier, upgraded by AI." << endl;
    LoadOpeningBook();
    SetBoard();
    const int THINKING_TIME_MS = 8000;
    while (true) {
        PrintBoard();
        vector<Move> all_moves; GenerateMoves(all_moves, false);
        vector<Move> legal_moves;
        Pos kingPos_before_move;
        for(int x=1; x<=8; ++x) for(int y=1; y<=8; ++y) {
            if(board[x][y] == (KING | (currentPlayer * COLOR_MASK))) { kingPos_before_move = {x,y}; break; }
        }
        for (const auto& m : all_moves) {
            Pos kingPos_after_move = kingPos_before_move;
            if ((board[m.from.x][m.from.y] & PIECE_TYPE_MASK) == KING) kingPos_after_move = m.to;
            UndoInfo undo = MakeMove(m);
            if (!IsSquareAttacked(kingPos_after_move, currentPlayer)) {
                legal_moves.push_back(m);
            }
            UnmakeMove(m, undo);
        }
        if (legal_moves.empty()) {
            if (IsSquareAttacked(kingPos_before_move, 1 - currentPlayer)) {
                cout << "Checkmate! " << ((currentPlayer == WHITE) ? "Black" : "White") << " Player Won." << endl;
            } else { cout << "Stalemate! It's a draw." << endl; }
            break;
        }
        if (currentPlayer == WHITE) {
            bool book_move_found = false;
            string current_fen = GenerateFEN();
            if (openingBook.count(current_fen)) {
                string uci_move_str = openingBook[current_fen];
                Move book_move = parse_uci_move(uci_move_str);
                bool is_legal = false;
                for (const auto& legal_m : legal_moves) { if (legal_m == book_move) { is_legal = true; break; } }
                if (is_legal) {
                    cout << "----------------------------------" << endl;
                    cout << "StarFish plays from its opening book (FEN: " << current_fen << ")" << endl;
                    cout << "Move: " << uci_move_str << endl;
                    cout << "----------------------------------" << endl;
                    MakeMove(book_move);
                    book_move_found = true;
                }
            }
            if (!book_move_found) {
                cout << "StarFish (White) is thinking for up to " << THINKING_TIME_MS / 1000.0 << " seconds..." << endl;
                searchTimeLimitMs = THINKING_TIME_MS;
                SearchBestMove();
                auto end_time = chrono::high_resolution_clock::now();
                chrono::duration<double> diff = end_time - searchStartTime;
                cout << "----------------------------------" << endl;
                cout << "AI has made its move." << endl;
                cout << "Move: From (" << (char)('a' + searchBestMove.from.x - 1) << searchBestMove.from.y << ") to (" << (char)('a' + searchBestMove.to.x - 1) << searchBestMove.to.y << ")" << endl;
                cout << "Total Nodes Computed: " << computedNodes << endl;
                cout << "Total Time Taken: " << diff.count() << " seconds" << endl;
                cout << "----------------------------------" << endl;
                MakeMove(searchBestMove);
            }
        } else {
             cout << "Your turn (Black)." << endl;
             cout << "Enter your move (e.g., e7 e5): ";
             string from_str, to_str;
             while (cin >> from_str >> to_str) {
                if (from_str.length() != 2 || to_str.length() != 2) { cout << "Invalid format. Try again: "; continue; }
                int from_x = tolower(from_str[0])-'a'+1, from_y = from_str[1]-'0', to_x = tolower(to_str[0])-'a'+1, to_y = to_str[1]-'0';
                bool move_found = false;
                for(const auto& m : legal_moves) {
                    if(m.from.x == from_x && m.from.y == from_y && m.to.x == to_x && m.to.y == to_y) {
                         if ((board[m.from.x][m.from.y] & PIECE_TYPE_MASK) == PAWN && (m.to.y == 1 || m.to.y == 8)) {
                             Move promotion_move = m; promotion_move.promotion = (currentPlayer == BLACK ? B_QUEEN : W_QUEEN); MakeMove(promotion_move);
                         } else { MakeMove(m); }
                         move_found = true; break;
                    }
                }
                if(move_found) { break; } else { cout << "Invalid or illegal move. Try again: "; }
            }
            if (cin.eof()) break;
        }
    }
    return 0;
}
