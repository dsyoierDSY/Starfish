// chessAI.cpp
//Starfish --- chess engine developed by dsyoier
//Version 2.8.1 (Strategist Pro) - PGN/FEN/SAN Aware, Legacy Compiler Compatible
//FEN Book Edition - Final Corrected Version with Min-Depth/Max-Time Search Logic
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <cctype>
#include <limits>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#define Sleep(x) usleep((x)*1000)
#endif

using namespace std;

// --- 核心定义与数据结构 ---
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

// --- 局面历史记录，用于三次重复检测 ---
map<string, int> positionHistory;


// --- 棋子静态数据 ---
int pieceValue[] = {0, 100, 375, 397, 613, 1220, 20000};
map<int, char> pieceChar = {
    {W_PAWN, 'P'}, {W_KNIGHT, 'N'}, {W_BISHOP, 'B'}, {W_ROOK, 'R'}, {W_QUEEN, 'Q'}, {W_KING, 'K'},
    {B_PAWN, 'p'}, {B_KNIGHT, 'n'}, {B_BISHOP, 'b'}, {B_ROOK, 'r'}, {B_QUEEN, 'q'}, {B_KING, 'k'},
    {EMPTY_PIECE, '.'}
};
map<char, int> charToPiece = {
    {'P', W_PAWN}, {'N', W_KNIGHT}, {'B', W_BISHOP}, {'R', W_ROOK}, {'Q', W_QUEEN}, {'K', W_KING},
    {'p', B_PAWN}, {'n', B_KNIGHT}, {'b', B_BISHOP}, {'r', B_ROOK}, {'q', B_QUEEN}, {'k', B_KING}
};
Pos knight_deltas[] = {{1,2}, {1,-2}, {-1,2}, {-1,-2}, {2,1}, {2,-1}, {-2,1}, {-2,-1}};
Pos bishop_deltas[] = {{1,1}, {1,-1}, {-1,1}, {-1,-1}};
Pos rook_deltas[]   = {{1,0}, {-1,0}, {0,1}, {0,-1}};
Pos king_deltas[]   = {{1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {1,-1}, {-1,1}, {-1,-1}};

// --- 棋子位置表 (Piece-Square Tables) ---
const int pawn_pst_mg[64] = {0,0,0,0,0,0,0,0,50,50,50,50,50,50,50,50,10,10,20,30,30,20,10,10,5,5,10,25,25,10,5,5,0,0,0,20,20,0,0,0,5,-5,-10,0,0,-10,-5,5,5,10,10,-20,-20,10,10,5,0,0,0,0,0,0,0,0};
const int pawn_pst_eg[64] = {0,0,0,0,0,0,0,0,80,80,80,80,80,80,80,80,50,50,50,50,50,50,50,50,30,30,30,30,30,30,30,30,20,20,20,20,20,20,20,20,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,0,0,0,0,0,0,0,0};
const int knight_pst_mg[64] = {-250,-40,-30,-30,-30,-30,-40,-250,-40,-20,0,0,0,0,-20,-40,-30,0,10,15,15,10,0,-30,-30,5,15,20,20,15,5,-30,-30,0,15,20,20,15,0,-30,-30,5,10,15,15,10,5,-30,-40,-20,0,5,5,0,-20,-40,-250,-40,-30,-30,-30,-30,-40,-250};
const int knight_pst_eg[64] = {-50,-30,-20,-20,-20,-20,-30,-50,-30,-10,0,5,5,0,-10,-30,-20,0,5,10,10,5,0,-20,-20,5,10,15,15,10,5,-20,-20,5,10,15,15,10,5,-20,-20,0,5,10,10,5,0,-20,-30,-10,0,5,5,0,-10,-30,-50,-30,-20,-20,-20,-20,-30,-50};
const int bishop_pst_mg[64] = {-20,-10,-10,-10,-10,-10,-10,-20,-10,0,0,0,0,0,0,-10,-10,0,5,10,10,5,0,-10,-10,5,5,10,10,5,5,-10,-10,0,10,10,10,10,0,-10,-10,10,10,10,10,10,10,-10,-10,5,0,0,0,0,5,-10,-20,-10,-10,-10,-10,-10,-10,-20};
const int bishop_pst_eg[64] = {-20,-10,-10,-10,-10,-10,-10,-20,-10,0,5,0,0,5,0,-10,-10,5,5,5,5,5,5,-10,-10,0,5,5,5,5,0,-10,-10,5,5,5,5,5,5,-10,-10,0,5,0,0,5,0,-10,-10,0,0,0,0,0,0,-10,-20,-10,-10,-10,-10,-10,-10,-20};
const int rook_pst_mg[64] = {0,0,0,0,0,0,0,0,5,10,10,10,10,10,10,5,-5,0,0,0,0,0,0,-5,-5,0,0,0,0,0,0,-5,-5,0,0,0,0,0,0,-5,-5,0,0,0,0,0,0,-5,-5,0,0,0,0,0,0,-5,0,0,0,5,5,0,0,0};
const int rook_pst_eg[64] = {10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,10,10,10,10,10,10,10,10};
const int queen_pst_mg[64] = {-20,-10,-10,-5,-5,-10,-10,-20,-10,0,0,0,0,0,0,-10,-10,0,5,5,5,5,0,-10,-5,0,5,5,5,5,0,-5,0,0,5,5,5,5,0,-5,-10,5,5,5,5,5,0,-10,-10,0,5,0,0,0,0,-10,-20,-10,-10,-5,-5,-10,-10,-20};
const int queen_pst_eg[64] = {-20,-10,-10,-5,-5,-10,-10,-20,-10,0,0,0,0,0,0,-10,-10,0,5,5,5,5,0,-10,-5,0,5,5,5,5,0,-5,0,0,5,5,5,5,0,-5,-10,5,5,5,5,5,0,-10,-10,0,5,0,0,0,0,-10,-20,-10,-10,-5,-5,-10,-10,-20};
const int king_pst_mg[64] = {-30,-40,-40,-50,-50,-40,-40,-30,-30,-40,-40,-50,-50,-40,-40,-30,-30,-40,-40,-50,-50,-40,-40,-30,-30,-40,-40,-50,-50,-40,-40,-30,-20,-30,-30,-40,-40,-30,-30,-20,-10,-20,-20,-20,-20,-20,-20,-10,20,20,0,0,0,0,20,20,20,30,10,0,0,10,30,20};
const int king_pst_eg[64] = {-50,-40,-30,-20,-20,-30,-40,-50,-30,-20,-10,0,0,-10,-20,-30,-30,-10,20,30,30,20,-10,-30,-30,-10,30,40,40,30,-10,-30,-30,-10,30,40,40,30,-10,-30,-30,-10,20,30,30,20,-10,-30,-30,-30,0,0,0,0,-30,-30,-50,-30,-30,-30,-30,-30,-30,-50};

// --- FEN 开局库 ---
map<string, string> openingBook;
inline int PieceColorOf(int piece); // 前向声明
void ResetPositionHistory(); // 前向声明

// --- 生成用于三同检测的局面唯一键 ---
string GeneratePositionKey() {
    stringstream key;
    for (int y = 8; y >= 1; --y) {
        int empty_count = 0;
        for (int x = 1; x <= 8; ++x) {
            int piece = board[x][y];
            if (piece == EMPTY_PIECE) { empty_count++; } else {
                if (empty_count > 0) { key << empty_count; empty_count = 0; }
                key << pieceChar[piece];
            }
        }
        if (empty_count > 0) { key << empty_count; }
        if (y > 1) { key << '/'; }
    }
    key << (currentPlayer == WHITE ? " w " : " b ");
    string castle_str;
    if (castlingRights & WK_CASTLE) castle_str += 'K';
    if (castlingRights & WQ_CASTLE) castle_str += 'Q';
    if (castlingRights & BK_CASTLE) castle_str += 'k';
    if (castlingRights & BQ_CASTLE) castle_str += 'q';
    key << (castle_str.empty() ? "-" : castle_str) << " ";
    if (enPassantTarget.ok()) { key << (char)('a' + enPassantTarget.x - 1) << (char)('0' + enPassantTarget.y); } else { key << "-"; }
    return key.str();
}

string GenerateFEN() {
    string key = GeneratePositionKey();
    stringstream fen;
    fen << key << " " << halfmoveClock << " " << Round;
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


// --- 辅助函数 ---
inline int PieceColorOf(int piece) {
    if (piece == EMPTY_PIECE) return NONE;
    return ( (piece & COLOR_MASK) ? BLACK : WHITE );
}

void ResetPositionHistory() {
    positionHistory.clear();
    positionHistory[GeneratePositionKey()]++;
}
// --- 核心功能函数 ---
void GenerateMoves(vector<Move>& moves, bool capturesOnly = false);
void SetBoard() {
    memset(board, 0, sizeof(board));
    board[1][1]=W_ROOK; board[2][1]=W_KNIGHT; board[3][1]=W_BISHOP; board[4][1]=W_QUEEN; board[5][1]=W_KING; board[6][1]=W_BISHOP; board[7][1]=W_KNIGHT; board[8][1]=W_ROOK;
    for (int i=1; i<=8; i++) board[i][2]=W_PAWN;
    board[1][8]=B_ROOK; board[2][8]=B_KNIGHT; board[3][8]=B_BISHOP; board[4][8]=B_QUEEN; board[5][8]=B_KING; board[6][8]=B_BISHOP; board[7][8]=B_KNIGHT; board[8][8]=B_ROOK;
    for (int i=1; i<=8; i++) board[i][7]=B_PAWN;
    currentPlayer = WHITE; castlingRights = WK_CASTLE | WQ_CASTLE | BK_CASTLE | BQ_CASTLE; enPassantTarget = Pos(0,0); Round = 1;
    halfmoveClock = 0;
    ResetPositionHistory();
}
void LoadFEN(const string& fen) {
    memset(board, 0, sizeof(board));
    stringstream ss(fen);
    string piece_placement, active_color, castling, en_passant, halfmove, fullmove;
    ss >> piece_placement >> active_color >> castling >> en_passant >> halfmove >> fullmove;

    int x = 1, y = 8;
    for (char c : piece_placement) {
        if (c == '/') { y--; x = 1; }
        else if (isdigit(c)) { x += (c - '0'); }
        else { board[x++][y] = charToPiece[c]; }
    }
    currentPlayer = (active_color == "w") ? WHITE : BLACK;
    castlingRights = 0;
    for (char c : castling) {
        if (c == 'K') castlingRights |= WK_CASTLE;
        else if (c == 'Q') castlingRights |= WQ_CASTLE;
        else if (c == 'k') castlingRights |= BK_CASTLE;
        else if (c == 'q') castlingRights |= BQ_CASTLE;
    }
    if (en_passant == "-") { enPassantTarget = Pos(0, 0); }
    else { enPassantTarget = Pos(en_passant[0] - 'a' + 1, en_passant[1] - '0'); }
    try {
        halfmoveClock = stoi(halfmove);
        Round = stoi(fullmove);
    } catch (const std::invalid_argument& ia) {
        halfmoveClock = 0;
        Round = 1;
    }
    ResetPositionHistory();
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
    
    positionHistory[GeneratePositionKey()]++;
    
    return undo;
}
void UnmakeMove(const Move& move, const UndoInfo& undo) {
    positionHistory[GeneratePositionKey()]--;

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

// --- 评估函数部分 ---
const double DOUBLED_PAWN_PENALTY = -15.0;
const double ISOLATED_PAWN_PENALTY = -10.0;
const double PASSED_PAWN_BONUS[] = {0, 10, 20, 30, 50, 80, 120, 200}; 
const double BISHOP_PAIR_BONUS = 40.0;
const double ROOK_ON_SEMI_OPEN_FILE_BONUS = 15.0;
const double ROOK_ON_OPEN_FILE_BONUS = 30.0;
const double PAWN_SHIELD_PENALTY = -10.0; 

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

double EvaluatePositional() {
    double score = 0;
    int white_pawns_on_file[9] = {0};
    int black_pawns_on_file[9] = {0};
    int white_bishops = 0, black_bishops = 0;
    Pos white_king_pos, black_king_pos;
    for (int x = 1; x <= 8; ++x) {
        for (int y = 1; y <= 8; ++y) {
            int piece = board[x][y];
            if (piece == EMPTY_PIECE) continue;
            int piece_type = piece & PIECE_TYPE_MASK;
            int color = PieceColorOf(piece);
            if (piece_type == PAWN) {
                if (color == WHITE) white_pawns_on_file[x]++;
                else black_pawns_on_file[x]++;
            } else if (piece_type == BISHOP) {
                if (color == WHITE) white_bishops++;
                else black_bishops++;
            } else if (piece_type == KING) {
                if (color == WHITE) white_king_pos = {x, y};
                else black_king_pos = {x, y};
            }
        }
    }
    for (int i = 1; i <= 8; ++i) {
        if (white_pawns_on_file[i] > 1) score += (white_pawns_on_file[i] - 1) * DOUBLED_PAWN_PENALTY;
        if (black_pawns_on_file[i] > 1) score -= (black_pawns_on_file[i] - 1) * DOUBLED_PAWN_PENALTY;
    }
    for (int x = 1; x <= 8; ++x) {
        for (int y = 1; y <= 8; ++y) {
            int piece = board[x][y];
            if (piece == EMPTY_PIECE) continue;
            int piece_type = piece & PIECE_TYPE_MASK;
            int color = PieceColorOf(piece);
            if (piece_type == PAWN) {
                int left_file_idx = max(1, x - 1); int right_file_idx = min(8, x + 1);
                bool isolated = false;
                if (color == WHITE) { if (white_pawns_on_file[left_file_idx] == 0 && white_pawns_on_file[right_file_idx] == 0) isolated = true; } 
                else { if (black_pawns_on_file[left_file_idx] == 0 && black_pawns_on_file[right_file_idx] == 0) isolated = true; }
                if (isolated) { score += (color == WHITE) ? ISOLATED_PAWN_PENALTY : -ISOLATED_PAWN_PENALTY; }
                bool is_passed = true;
                int forward_dir = (color == WHITE) ? 1 : -1;
                for (int dx = -1; dx <= 1; ++dx) {
                    int check_x = x + dx; if (check_x < 1 || check_x > 8) continue;
                    for (int check_y = y + forward_dir; check_y >= 1 && check_y <= 8; check_y += forward_dir) {
                        int blocking_piece = board[check_x][check_y];
                        if (blocking_piece != EMPTY_PIECE && PieceColorOf(blocking_piece) != color && (blocking_piece & PIECE_TYPE_MASK) == PAWN) {
                            is_passed = false; break;
                        }
                    }
                    if (!is_passed) break;
                }
                if (is_passed) { int rank_from_home = (color == WHITE) ? y : (9 - y); double bonus = PASSED_PAWN_BONUS[rank_from_home-1]; score += (color == WHITE) ? bonus : -bonus; }
            } else if (piece_type == ROOK) {
                if (color == WHITE) {
                    if (white_pawns_on_file[x] == 0) { if (black_pawns_on_file[x] == 0) score += ROOK_ON_OPEN_FILE_BONUS; else score += ROOK_ON_SEMI_OPEN_FILE_BONUS; }
                } else { if (black_pawns_on_file[x] == 0) { if (white_pawns_on_file[x] == 0) score -= ROOK_ON_OPEN_FILE_BONUS; else score -= ROOK_ON_SEMI_OPEN_FILE_BONUS; } }
            }
        }
    }
    if (white_bishops >= 2) score += BISHOP_PAIR_BONUS;
    if (black_bishops >= 2) score -= BISHOP_PAIR_BONUS;
    if (white_king_pos.y <= 2) {
        if (white_king_pos.x >= 6) { 
            if (board[6][2] != W_PAWN) score += PAWN_SHIELD_PENALTY * 2; else if (board[6][3] == W_PAWN) score += PAWN_SHIELD_PENALTY;
            if (board[7][2] != W_PAWN) score += PAWN_SHIELD_PENALTY * 2; else if (board[7][3] == W_PAWN) score += PAWN_SHIELD_PENALTY;
            if (board[8][2] != W_PAWN) score += PAWN_SHIELD_PENALTY * 2; else if (board[8][3] == W_PAWN) score += PAWN_SHIELD_PENALTY;
        } else if (white_king_pos.x <= 4) {
            if (board[1][2] != W_PAWN) score += PAWN_SHIELD_PENALTY * 2; else if (board[1][3] == W_PAWN) score += PAWN_SHIELD_PENALTY;
            if (board[2][2] != W_PAWN) score += PAWN_SHIELD_PENALTY * 2; else if (board[2][3] == W_PAWN) score += PAWN_SHIELD_PENALTY;
            if (board[3][2] != W_PAWN) score += PAWN_SHIELD_PENALTY * 2; else if (board[3][3] == W_PAWN) score += PAWN_SHIELD_PENALTY;
        }
    }
    if (black_king_pos.y >= 7) {
        if (black_king_pos.x >= 6) {
            if (board[6][7] != B_PAWN) score -= PAWN_SHIELD_PENALTY * 2; else if (board[6][6] == B_PAWN) score -= PAWN_SHIELD_PENALTY;
            if (board[7][7] != B_PAWN) score -= PAWN_SHIELD_PENALTY * 2; else if (board[7][6] == B_PAWN) score -= PAWN_SHIELD_PENALTY;
            if (board[8][7] != B_PAWN) score -= PAWN_SHIELD_PENALTY * 2; else if (board[8][6] == B_PAWN) score -= PAWN_SHIELD_PENALTY;
        } else if (black_king_pos.x <= 4) {
            if (board[1][7] != B_PAWN) score -= PAWN_SHIELD_PENALTY * 2; else if (board[1][6] == B_PAWN) score -= PAWN_SHIELD_PENALTY;
            if (board[2][7] != B_PAWN) score -= PAWN_SHIELD_PENALTY * 2; else if (board[2][6] == B_PAWN) score -= PAWN_SHIELD_PENALTY;
            if (board[3][7] != B_PAWN) score -= PAWN_SHIELD_PENALTY * 2; else if (board[3][6] == B_PAWN) score -= PAWN_SHIELD_PENALTY;
        }
    }
    return score;
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
    score += EvaluatePositional();
    return score;
}

// --- AI 搜索 ---
Move searchBestMove;
long long computedNodes;
chrono::time_point<chrono::high_resolution_clock> searchStartTime;
int searchTimeLimitMs;
double QuiescenceSearch(double alpha, double beta);
bool time_is_up = false;
int search_min_depth;
int iterative_deepening_current_depth;

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
    if (time_is_up) { return 0; }
    if (iterative_deepening_current_depth > search_min_depth) {
        auto now = chrono::high_resolution_clock::now();
        if (chrono::duration_cast<chrono::milliseconds>(now - searchStartTime).count() > searchTimeLimitMs) {
            time_is_up = true;
            return 0;
        }
    }
    if (positionHistory[GeneratePositionKey()] >= 2) { return 0.0; }
    if (depth == 0) { return QuiescenceSearch(alpha, beta); }
    vector<Move> moves; GenerateMoves(moves, false);
    vector<Move> legal_moves;
    Pos kingPos;
    for(int x=1; x<=8; ++x) for(int y=1; y<=8; ++y) { if(board[x][y] == (KING | (currentPlayer * COLOR_MASK))) { kingPos = {x,y}; break; } }
    for (const auto& m : moves) {
        Pos kingPos_after = kingPos;
        if ((board[m.from.x][m.from.y] & PIECE_TYPE_MASK) == KING) kingPos_after = m.to;
        UndoInfo undo = MakeMove(m);
        if (!IsSquareAttacked(kingPos_after, currentPlayer)) { legal_moves.push_back(m); }
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
        if (time_is_up) { return 0; }
        if (score > bestScore) { bestScore = score; }
        if (bestScore > alpha) { alpha = bestScore; }
        if (alpha >= beta) { break; }
    }
    return bestScore;
}
double QuiescenceSearch(double alpha, double beta) {
    if (time_is_up) { return 0; }
    if (iterative_deepening_current_depth > search_min_depth) {
        auto now = chrono::high_resolution_clock::now();
        if (chrono::duration_cast<chrono::milliseconds>(now - searchStartTime).count() > searchTimeLimitMs) {
            time_is_up = true;
            return 0;
        }
    }
    double stand_pat = (currentPlayer == WHITE ? Evaluate() : -Evaluate());
    if (stand_pat >= beta) { return beta; }
    if (alpha < stand_pat) { alpha = stand_pat; }
    vector<Move> capture_moves; GenerateMoves(capture_moves, true);
    vector<Move> legal_captures;
    Pos kingPos;
    for(int x=1; x<=8; ++x) for(int y=1; y<=8; ++y) { if(board[x][y] == (KING | (currentPlayer * COLOR_MASK))) { kingPos = {x,y}; break; } }
     for (const auto& m : capture_moves) {
        Pos kingPos_after = kingPos;
        if ((board[m.from.x][m.from.y] & PIECE_TYPE_MASK) == KING) kingPos_after = m.to;
        UndoInfo undo = MakeMove(m);
        if (!IsSquareAttacked(kingPos_after, currentPlayer)) { legal_captures.push_back(m); }
        UnmakeMove(m, undo);
    }
    sort(legal_captures.begin(), legal_captures.end(), [&](const Move& a, const Move& b) { return scoreMove(a) > scoreMove(b); });
    for (const auto& m : legal_captures) {
        UndoInfo undo = MakeMove(m);
        computedNodes++;
        double score = -QuiescenceSearch(-beta, -alpha);
        UnmakeMove(m, undo);
        if (time_is_up) { return 0; }
        if (score >= beta) { return beta; }
        if (score > alpha) { alpha = score; }
    }
    return alpha;
}
void SearchBestMove(int min_depth) {
    searchStartTime = chrono::high_resolution_clock::now();
    computedNodes = 0;
    time_is_up = false;
    search_min_depth = min_depth;
    double bestScoreSoFar = -numeric_limits<double>::infinity();
    const int MAX_DEPTH = 64;
    vector<Move> moves; GenerateMoves(moves, false);
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
    for (int current_depth = 2; current_depth <= MAX_DEPTH; current_depth += 2) {
        iterative_deepening_current_depth = current_depth;
        sort(legal_moves.begin(), legal_moves.end(), [&](const Move& a, const Move& b) { return scoreMove(a) > scoreMove(b); });
        Move bestMoveThisIteration = legal_moves[0];
        double bestScoreThisIteration = -numeric_limits<double>::infinity();
        double alpha = -numeric_limits<double>::infinity();
        double beta = numeric_limits<double>::infinity();
        for (const auto& m : legal_moves) {
            UndoInfo undo = MakeMove(m);
            double score = -AlphaBetaSearch(current_depth - 1, -beta, -alpha);
            UnmakeMove(m, undo);
            if (time_is_up) { break; }
            if (score > bestScoreThisIteration) {
                bestScoreThisIteration = score;
                bestMoveThisIteration = m;
                if (score > alpha) { alpha = score; }
            }
        }
        if (time_is_up) {
            cout << "info string Time up during depth " << current_depth << ". Using results from depth " << current_depth - 2 << "." << endl;
            break; 
        }
        bestScoreSoFar = bestScoreThisIteration;
        searchBestMove = bestMoveThisIteration;
        auto end_time = chrono::high_resolution_clock::now();
        chrono::duration<double> diff = end_time - searchStartTime;
        cout << "info depth " << current_depth << " score cp " << static_cast<int>(currentPlayer == WHITE ? bestScoreSoFar : -bestScoreSoFar)
             << " nodes " << computedNodes << " time " << static_cast<int>(diff.count() * 1000) << "ms" << endl;
        if (current_depth >= min_depth) {
            auto now = chrono::high_resolution_clock::now();
            if (chrono::duration_cast<chrono::milliseconds>(now - searchStartTime).count() > searchTimeLimitMs) {
                cout << "info string Not enough time to start next depth." << endl;
                break;
            }
        }
    }
}
// --- UI 辅助函数 ---
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
    cout << "  +------------------------+" << endl;
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
    cout << "  +------------------------+" << endl;
    cout << "   a  b  c  d  e  f  g  h" << endl;
    cout << "FEN: " << GenerateFEN() << endl << endl;
}
Move ParseAlgebraicMove(const string& san_str, const vector<Move>& legal_moves) {
    string s = san_str;
    if (s == "O-O" || s == "0-0") {
        int rank = (currentPlayer == WHITE) ? 1 : 8;
        for (const auto& m : legal_moves) {
            if ((board[m.from.x][m.from.y] & PIECE_TYPE_MASK) == KING &&
                m.from.x == 5 && m.from.y == rank && m.to.x == 7 && m.to.y == rank) {
                return m;
            }
        }
    }
    if (s == "O-O-O" || s == "0-0-0") {
        int rank = (currentPlayer == WHITE) ? 1 : 8;
        for (const auto& m : legal_moves) {
            if ((board[m.from.x][m.from.y] & PIECE_TYPE_MASK) == KING &&
                m.from.x == 5 && m.from.y == rank && m.to.x == 3 && m.to.y == rank) {
                return m;
            }
        }
    }
    
    s.erase(remove(s.begin(), s.end(), '+'), s.end());
    s.erase(remove(s.begin(), s.end(), '#'), s.end());
    s.erase(remove(s.begin(), s.end(), 'x'), s.end());
    
    int promotion_piece = EMPTY_PIECE;
    size_t promotion_pos = s.find('=');
    if (promotion_pos != string::npos) {
        char prom_char = toupper(s[promotion_pos + 1]);
        if (prom_char == 'Q') promotion_piece = QUEEN;
        else if (prom_char == 'R') promotion_piece = ROOK;
        else if (prom_char == 'B') promotion_piece = BISHOP;
        else if (prom_char == 'N') promotion_piece = KNIGHT;
        promotion_piece |= (currentPlayer * COLOR_MASK);
        s = s.substr(0, promotion_pos);
    }
    
    Pos to_pos;
    to_pos.y = s.back() - '0';
    s.pop_back();
    to_pos.x = s.back() - 'a' + 1;
    s.pop_back();

    int piece_type = PAWN;
    if (!s.empty() && isupper(s[0])) {
        if (s[0] == 'N') piece_type = KNIGHT;
        else if (s[0] == 'B') piece_type = BISHOP;
        else if (s[0] == 'R') piece_type = ROOK;
        else if (s[0] == 'Q') piece_type = QUEEN;
        else if (s[0] == 'K') piece_type = KING;
        s = s.substr(1);
    }
    
    int from_file = -1, from_rank = -1;
    if (!s.empty()) {
        if (s.length() == 1) {
            if (s[0] >= 'a' && s[0] <= 'h') from_file = s[0] - 'a' + 1;
            else if (s[0] >= '1' && s[0] <= '8') from_rank = s[0] - '0';
        } else if (s.length() == 2) {
            from_file = s[0] - 'a' + 1;
            from_rank = s[1] - '0';
        }
    }
    
    vector<Move> candidates;
    for (const auto& m : legal_moves) {
        if (m.to == to_pos && (board[m.from.x][m.from.y] & PIECE_TYPE_MASK) == piece_type) {
            bool file_match = (from_file == -1 || m.from.x == from_file);
            bool rank_match = (from_rank == -1 || m.from.y == from_rank);
            bool promotion_match = (promotion_piece == EMPTY_PIECE) ? (m.promotion == EMPTY_PIECE) : (m.promotion == promotion_piece);

            if (file_match && rank_match && promotion_match) {
                candidates.push_back(m);
            }
        }
    }
    
    if (candidates.size() == 1) {
        return candidates[0];
    }
    
    return Move{Pos(0,0), Pos(0,0)};
}
bool LoadPGN(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cout << "Error: Could not open PGN file '" << filename << "'" << endl;
        return false;
    }

    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();
    file.close();

    string initial_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    smatch match;
    // --- FIX START --- Replaced raw string literals with standard strings for compatibility ---
    regex fen_regex("\\[FEN\\s+\"([^\"]+)\"\\]");
    if (regex_search(content, match, fen_regex)) {
        initial_fen = match[1].str();
    }
    LoadFEN(initial_fen);
    
    size_t movetext_start = content.rfind(']');
    if (movetext_start == string::npos) movetext_start = 0;
    else movetext_start++;
    
    string movetext = content.substr(movetext_start);
    movetext = regex_replace(movetext, regex("\\{.*?\\}"), " "); // Remove comments
    movetext = regex_replace(movetext, regex("\\d+\\."), " ");   // Remove move numbers
    // --- FIX END ---
    
    stringstream move_stream(movetext);
    string san_move;
    while (move_stream >> san_move) {
        if (san_move == "1-0" || san_move == "0-1" || san_move == "1/2-1/2" || san_move == "*") break;

        vector<Move> legal_moves;
        vector<Move> all_moves; GenerateMoves(all_moves, false);
        Pos kingPos;
        for(int x=1; x<=8; ++x) for(int y=1; y<=8; ++y) if(board[x][y] == (KING | (currentPlayer * COLOR_MASK))) { kingPos = {x,y}; break; }
        for (const auto& m : all_moves) {
            Pos king_after = kingPos; if((board[m.from.x][m.from.y] & PIECE_TYPE_MASK) == KING) king_after = m.to;
            UndoInfo undo = MakeMove(m);
            if (!IsSquareAttacked(king_after, currentPlayer)) legal_moves.push_back(m);
            UnmakeMove(m, undo);
        }
        
        Move parsed_move = ParseAlgebraicMove(san_move, legal_moves);
        if (parsed_move.from.ok()) {
            MakeMove(parsed_move);
        } else {
            cout << "Error parsing move '" << san_move << "' in PGN file. Stopping." << endl;
            PrintBoard();
            return false;
        }
    }
    cout << "PGN file loaded successfully." << endl;
    return true;
}

// --- 主函数和UI ---
int main() {
    #ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) { DWORD dwMode = 0; if (GetConsoleMode(hOut, &dwMode)) { dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING; SetConsoleMode(hOut, dwMode); }}
    #endif
    cout << "------------------- StarFish Ver 2.8.1 (Strategist Pro) ------------------" << endl;
    cout << "Engine with PGN/FEN/SAN support, FEN Book, Iterative Deepening, and Draw Detection." << endl;
    cout << "Nov, 2025 Build. Developed by dsyoier, upgraded by AI." << endl;
    LoadOpeningBook();

    cout << "\nSelect Game Mode:" << endl;
    cout << "1. Start a new game" << endl;
    cout << "2. Load position from FEN string" << endl;
    cout << "3. Load game from PGN file" << endl;
    cout << "Enter your choice (1-3): ";
    
    int mode_choice;
    cin >> mode_choice;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    switch (mode_choice) {
        case 2: {
            cout << "Enter FEN string: ";
            string fen;
            getline(cin, fen);
            LoadFEN(fen);
            break;
        }
        case 3: {
            cout << "Enter PGN file name (e.g., game.pgn): ";
            string filename;
            getline(cin, filename);
            if (!LoadPGN(filename)) return 1;
            break;
        }
        default:
            SetBoard();
            break;
    }


    int enginePlayerColor = -1;
    cout << "\nWhich color should the engine play as? (w for white, b for black): ";
    char choice;
    while (cin >> choice) {
        choice = tolower(choice);
        if (choice == 'w') { enginePlayerColor = WHITE; break; } 
        else if (choice == 'b') { enginePlayerColor = BLACK; break; } 
        else { cout << "Invalid input. Please enter 'w' or 'b': "; }
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    const int MIN_SEARCH_DEPTH = 6;
    const int MAX_SEARCH_TIME_MS = 8000;

    while (true) {
        PrintBoard();
        
        if (positionHistory[GeneratePositionKey()] >= 3) { cout << "Draw by three-fold repetition!" << endl; break; }
        if (halfmoveClock >= 100) { cout << "Draw by 50-move rule!" << endl; break; }

        vector<Move> all_moves; GenerateMoves(all_moves, false);
        vector<Move> legal_moves;
        Pos kingPos_before_move;
        for(int x=1; x<=8; ++x) for(int y=1; y<=8; ++y) if(board[x][y] == (KING | (currentPlayer * COLOR_MASK))) { kingPos_before_move = {x,y}; break; }
        for (const auto& m : all_moves) {
            Pos kingPos_after_move = kingPos_before_move;
            if ((board[m.from.x][m.from.y] & PIECE_TYPE_MASK) == KING) kingPos_after_move = m.to;
            UndoInfo undo = MakeMove(m);
            if (!IsSquareAttacked(kingPos_after_move, currentPlayer)) { legal_moves.push_back(m); }
            UnmakeMove(m, undo);
        }
        if (legal_moves.empty()) {
            if (IsSquareAttacked(kingPos_before_move, 1 - currentPlayer)) { cout << "Checkmate! " << ((currentPlayer == WHITE) ? "Black" : "White") << " Player Won." << endl;
            } else { cout << "Stalemate! It's a draw." << endl; }
            break;
        }

        if (currentPlayer == enginePlayerColor) {
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
                    MakeMove(book_move); book_move_found = true;
                }
            }
            if (!book_move_found) {
                string engineColorStr = (enginePlayerColor == WHITE ? "White" : "Black");
                cout << "StarFish (" << engineColorStr << ") is thinking (min depth " << MIN_SEARCH_DEPTH
                     << ", max time " << MAX_SEARCH_TIME_MS / 1000.0 << "s)..." << endl;
                searchTimeLimitMs = MAX_SEARCH_TIME_MS;
                SearchBestMove(MIN_SEARCH_DEPTH);
                
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
             string playerColorStr = (currentPlayer == WHITE ? "White" : "Black");
             cout << "Your turn (" << playerColorStr << ")." << endl;
             cout << "Enter your move in algebraic notation (e.g., e4, Nf3, O-O): ";
             string san_input;
             while (getline(cin, san_input)) {
                if (san_input.empty()) continue;
                Move human_move = ParseAlgebraicMove(san_input, legal_moves);
                if (human_move.from.ok()) {
                    MakeMove(human_move);
                    break;
                } else {
                    cout << "Invalid or illegal move '" << san_input << "'. Try again: ";
                }
            }
            if (cin.eof()) break;
        }
    }
    return 0;
}