/* Copyright (C) 2026 The Project_Nebulora Developers

This program is free software: you can redistribute it and/or modify it under the terms of the 
GNU General Public License as published by the Free Software Foundation, either version 3 
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. 
If not, see https://www.gnu.org/licenses/.
*/

#ifndef BOARDCLASS_HPP
#define BOARDCLASS_HPP

#include<array>
#include<string>
#include<iostream>
#include<sstream>
#include<bit>
#include<immintrin.h>
#include<format> //for hash debugging

#include "globals.hpp"

//precomputed tables
extern std::array<uint64_t, 64>                   seenByKing;
extern std::array<uint64_t, 64>                   seenByKnight;
extern std::array<std::array<uint64_t, 512>, 64>  seenByD12;
extern std::array<std::array<uint64_t, 4096>, 64> seenByHV;
extern std::array<uint64_t, 64>                   attackHV;
extern std::array<uint64_t, 64>                   attackD12;
extern std::array<uint64_t, 4096>                 fromToTable;
extern std::array<uint64_t, 858>                  zobristHashes;


//forward declarations
class transpositionTableClass;





class alignas(64) boardClass {

    public:
        //rearranging hot variables and alligning/padding varialbes to cachelines can improve performance.
        //still left to do.

        //board representation
        std::array<uint64_t, 12> bitboard;
        std::array<uint64_t, 3> occupied;
        uint64_t positionHash;
        std::array<uint8_t, 64> pieceAt;
        uint64_t* repetitionListPtr;
        uint16_t moveClockPly;
        bool whiteToMove;
        uint8_t castlingFlags;
        uint8_t enpassantFiles;
        uint8_t fiftyMoveClockPly;

        //repetition table start is a cold point of this class. The currentPtr is kept in the hot section.
        //it is alligned to allow for SIMD optimization later on.
        alignas(64) std::array<uint64_t, 4096> repetitionHashList;


        //setup position functions
        boardClass(){
            setupStartingPosition();
        }

        void setupStartingPosition(){
            const std::string FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
            setupFEN(FEN);
        }

        void setupFEN(const std::string& FEN){
            using namespace constants;

            setBoardEmpty();

            //convert the string to the individual fields
            std::stringstream ss(FEN);
            std::string position, toMove, castling, enpassant;
            uint64_t halfMoveCounter, moveCounter;
            ss >> position >> toMove >> castling >> enpassant >> halfMoveCounter >> moveCounter;


            { //set all the pieces on the board
                const std::string symbols = "PRNBQKprnbqk/12345678";
                int fileIndex = 0;
                int rankIndex = 7;
                for (int i = 0; i < position.length(); i++) {
    
                    int type = symbols.find(position[i], 0);
   
                    if (type < 12) { //is it a pieceType
                        uint8_t squareIndex = 8 * rankIndex + fileIndex;
                        uint64_t squareBitboard  = 1ULL << squareIndex;
                        uint8_t pieceColor = type < 6 ? white : black;
                        
                        bitboard[type] |= squareBitboard;
                        occupied[pieceColor] |= squareBitboard;
                        occupied[combined] |= squareBitboard;
                        pieceAt[squareIndex] = type;
                        positionHash ^= zobristHashes[squareIndex * 13 + type];

                        fileIndex++;
                    } else     
                    
                    if (type > 12) { //skip empty squares
                        
                        uint8_t emptyNSquares = type - 12;
                        fileIndex += emptyNSquares;
                    } else    
                    { //skip to the new line
                        fileIndex = 0;
                        rankIndex--;
                    }    
                }   
                

            }    
            
            { //set the side to move
                whiteToMove = toMove == "w";
                positionHash ^= !whiteToMove * zobristHashes[hash::whiteToMoveHash];
            }    
            
            { //set all the castling rights
                if (castling.find('K') != std::string::npos) {castlingFlags |= whiteKingCastle;  }
                if (castling.find('Q') != std::string::npos) {castlingFlags |= whiteQueenCastle; }
                if (castling.find('k') != std::string::npos) {castlingFlags |= blackKingCastle;  }
                if (castling.find('q') != std::string::npos) {castlingFlags |= blackQueenCastle; }
                positionHash ^= zobristHashes[hash::CastlingHash + castlingFlags];
            }   

            { //set up the enpassant rights
                const std::string files = "abcdefgh";
                int file;
                if ((file = files.find(enpassant[0], 0)) != std::string::npos) {
                    enpassantFiles |= 1ULL << file;
                    positionHash ^= zobristHashes[hash::enPassantFileHash + file];
                }    
            }    
            
            { //setup the boardcounters
                fiftyMoveClockPly = halfMoveCounter;
                moveClockPly = 2 * (moveCounter - 1) + !whiteToMove;
            }
            
            {//setup the currenthasPtr
                repetitionListPtr = &repetitionHashList[0];
                *(repetitionListPtr++) = positionHash;
            }
        }    
        

        // I/O functions
        void visualizeBoard() {
            using namespace constants;

            const std::string pieceTypes = "PRNBQKprnbqk ";
            const std::string line  = "+---+---+---+---+---+---+---+---+\n";

            //loop over all the ranks
            for (int64_t rank = 7; rank >=0; rank--) {

                std::cout << line << "|";

                //loop over all the files
                for (uint64_t file = 0; file < 8; file++) {

                    std::cout << " " << pieceTypes[pieceAt[file + 8 * rank]] << " |";
                }

                std::cout << '\n';
            }
            std::cout << line;

            //std::cout << "hash: " << std::format("{:x}", positionHash) << std::endl;
            std::cout << "FEN: " << getFen() << '\n';
        }

        std::string getFen(){

            std::string fen;
            const std::string symbolOf = "PRNBQKprnbqk1";
            int fenLength = 0;

            for (int rank = 7; rank >= 0; rank--) {

                if (rank != 7) {fen += '/'; fenLength++;}

                for (int file = 0; file < 8; file++) {

                    int squareIndex = 8 * rank + file;
                    uint8_t pieceType = pieceAt[squareIndex];

                    if ((file > 0) && (pieceType == constants::emptySquare) && ((fen[fenLength-1] - '0') < 9)) {
                        int value = fen[fenLength-1] - '0';
                        fen[fenLength-1] = char('0' + value + 1);
                    } else {
                        fen += symbolOf[pieceType];
                        fenLength++;
                    }
                }
            }

            if (whiteToMove) {fen += " w ";}
            else {fen += " b ";}

            if ((castlingFlags & constants::whiteKingCastle)  != 0) {fen += 'K'; }
            if ((castlingFlags & constants::whiteQueenCastle) != 0) {fen += 'Q'; }
            if ((castlingFlags & constants::blackKingCastle)  != 0) {fen += 'k'; }
            if ((castlingFlags & constants::blackQueenCastle) != 0) {fen += 'q'; }
            if (castlingFlags == 0) {fen += '-';}

            //enpassant
            const std::string files = "abcdefgh";
            if (enpassantFiles != 0) { //check if the enpassant files are not empty
                int fileIndex = std::countr_zero(enpassantFiles);
                fen += " " + std::string(1, files[fileIndex]) + (whiteToMove ? "6" : "3");
            } else {
                fen += " -";
            }

            //counters
            fen += " " + std::to_string(fiftyMoveClockPly);
            fen += " " + std::to_string(moveClockPly / 2 + 1);

            return fen;
        }

        uint16_t coordinatesToMove(const std::string& move){
            const std::string promotions = " rnbq";

            uint16_t startingFile = move[0] - 'a';
            uint16_t startingRank = move[1] - '1';
            uint16_t targetFile   = move[2] - 'a';
            uint16_t targetRank   = move[3] - '1';
            uint16_t promotion    = 0;

            //promotions have a 5th element and are converted to the highest 4 bits.
            if (size(move) == 5) {promotion = promotions.find(move[4]);}

            return promotion << 12 | targetRank << 9 | targetFile << 6 | startingRank << 3 | startingFile;
        }
        
        std::string moveToCoordinates(const uint16_t move){
            static constexpr char files[]      = "abcdefgh";
            static constexpr char ranks[]      = "12345678";
            static constexpr char promotions[] = " rnbq";
            constexpr uint16_t lowestThreeBits = uint16_t(0b111);


            //calculate the rank and file
            uint16_t startFileIndex  = move         & lowestThreeBits;
            uint16_t startRankIndex  = (move >> 3)  & lowestThreeBits;
            uint16_t targetFileIndex = (move >> 6)  & lowestThreeBits;
            uint16_t targetRankIndex = (move >> 9)  & lowestThreeBits;
            uint16_t promotion       = (move >> 12) & lowestThreeBits;

            //assemble the move
            std::string returnMove; //strings are not initialized as random
            returnMove.reserve(5);
            returnMove += files[startFileIndex];
            returnMove += ranks[startRankIndex];
            returnMove += files[targetFileIndex];
            returnMove += ranks[targetRankIndex];
            if (promotion != 0) {returnMove += promotions[promotion];}
            

            return returnMove;
        }


        //adjust the board
        /*declaration of make move to prevent circulair dependency*/
        template<bool prefetchTT, bool addToHistoryStack>
        uint64_t makeMove(const uint16_t& move, searchEnvStruct* envPtr = nullptr, searchNodeStruct* nodePtr = nullptr);

        void unMakeMove(uint64_t unMakeInfo, const uint16_t move) {
            using namespace constants;

            //undo any possible enpassantFileHash
            positionHash ^= zobristHashes[hash::enPassantFileHash + std::countr_zero(uint32_t(enpassantFiles) | 0x100U)];

            uint8_t startingCastlingFlags = castlingFlags;

            //unpack the unMakeInfo variable
            castlingFlags           = (unMakeInfo      ) & 0b1111U;
            enpassantFiles          = (unMakeInfo >> 4 ) & 0b11111111U;
            fiftyMoveClockPly       = (unMakeInfo >> 12) & 0b1111111U;
            uint8_t targetPieceType = (unMakeInfo >> 19) & 0b1111U;
            uint64_t startingIndex  = (move            ) & 0b111111U;
            uint64_t targetIndex    = (move        >> 6) & 0b111111U;
            uint8_t promotion       = (move       >> 12) & 0b111;
            uint8_t startPieceType  = pieceAt[targetIndex] - promotion;
            whiteToMove             = !whiteToMove;
            positionHash ^= zobristHashes[hash::whiteToMoveHash];
            moveClockPly--;

            //reHash any possible enpassantFileHash
            positionHash ^= zobristHashes[hash::enPassantFileHash + std::countr_zero(uint32_t(enpassantFiles) | 0x100U)];

            uint64_t startingBitBoard = 1ULL << startingIndex;
            uint64_t targetBitBoard = 1ULL << targetIndex;

            //remove from targetsquare
            bitboard[startPieceType + promotion]  ^= targetBitBoard;
            occupied[whiteToMove ? white : black] ^= targetBitBoard;
            occupied[combined]                    ^= targetBitBoard;
            pieceAt[targetIndex]                   = emptySquare;
            positionHash                          ^= zobristHashes[targetIndex * 13 + startPieceType + promotion];

            //add to startingIndex
            bitboard[startPieceType]              ^= startingBitBoard;
            occupied[whiteToMove ? white : black] ^= startingBitBoard;
            occupied[combined]                    ^= startingBitBoard;
            pieceAt[startingIndex]                 = startPieceType;
            positionHash                          ^= zobristHashes[startingIndex * 13 + startPieceType];
                
            //add back captured piece
            if (targetPieceType != emptySquare) {
                bitboard[targetPieceType]             ^= targetBitBoard;
                occupied[whiteToMove ? black : white] ^= targetBitBoard;
                occupied[combined]                    ^= targetBitBoard;
                pieceAt[targetIndex]                   = targetPieceType;
                positionHash                          ^= zobristHashes[targetIndex * 13 + targetPieceType];
            }



            //enpassant to the left
            if ( ((startingIndex + 7) == targetIndex) &&(startPieceType == whitePawn) && (targetPieceType == emptySquare)) [[unlikely]] {
                enpassantCapture<true, false, true>(startingIndex);
            } else

            //enpassant to the right
            if ( ((startingIndex + 9) == targetIndex) && (startPieceType == whitePawn) && (targetPieceType == emptySquare)) [[unlikely]] {
                enpassantCapture<true, true, true>(startingIndex);
            } else

            //enpassant to the left
            if ( ((startingIndex - 9) == targetIndex) && (startPieceType == blackPawn) && (targetPieceType == emptySquare)) [[unlikely]] {
                enpassantCapture<false, false, true>(startingIndex);
            } else
            
            //enpassant to the right
            if ( ((startingIndex - 7) == targetIndex) && (startPieceType == blackPawn) && (targetPieceType == emptySquare)) [[unlikely]] {
                enpassantCapture<false, true, true>(startingIndex);
            } else
            
            //castling queen side
            if ( (startPieceType == whiteKing) && (startingIndex - 2) == targetIndex) [[unlikely]] {
                castledRook<true, false, true>();
            } else

            //castling king side
            if ( (startPieceType == whiteKing) && (startingIndex + 2) == targetIndex) [[unlikely]] {
                castledRook<true, true, true>();
            } else


            //castling queen side
            if ( (startPieceType == blackKing) && (startingIndex - 2) == targetIndex) [[unlikely]] {
                castledRook<false, false, true>();
            } else

            //castling king side
            if ( (startPieceType == blackKing) && (startingIndex + 2) == targetIndex) [[unlikely]] {
                castledRook<false, true, true>();
            }





            uint8_t changedCastlingFlags = startingCastlingFlags ^ castlingFlags;
            positionHash ^= zobristHashes[hash::CastlingHash + changedCastlingFlags];

            //move back the pointer of the repetitionHashList
            repetitionListPtr--;


            //incremental eval still left to do
        }

        uint64_t makeNullMove(){
            using namespace constants;

            //add the current hash to the repetitionHashList and advance the pointer
            //*(repetitionListPtr++) = positionHash;  //REPETITIONS UNDER NULL MOVES?!?!

            //undo any possible enpassantFileHash
            positionHash ^= zobristHashes[hash::enPassantFileHash + std::countr_zero(uint32_t(enpassantFiles) | 0x100U)];

            positionHash ^= zobristHashes[hash::whiteToMoveHash];

            //assemble the info needed to unmake this move
            uint64_t unmakeInfo = (castlingFlags)
                                | (enpassantFiles << 4)
                                | (fiftyMoveClockPly << 12)
                                | (0ULL << 19);

            //update the board
            fiftyMoveClockPly = fiftyMoveClockPly + 1;
            moveClockPly += 1;
            enpassantFiles = 0;
            
            whiteToMove = !whiteToMove;

            //add the hash for castling changes and current enpassant state
            positionHash ^= zobristHashes[hash::enPassantFileHash + std::countr_zero(uint32_t(enpassantFiles) | 0x100U)];

            return unmakeInfo;
        }

        void unMakeNullMove(uint64_t unMakeInfo) {
            using namespace constants;

            //undo any possible enpassantFileHash
            positionHash ^= zobristHashes[hash::enPassantFileHash + std::countr_zero(uint32_t(enpassantFiles) | 0x100U)];

            //unpack the unMakeInfo variable
            castlingFlags           = (unMakeInfo      ) & 0b1111U;
            enpassantFiles          = (unMakeInfo >> 4 ) & 0b11111111U;
            fiftyMoveClockPly       = (unMakeInfo >> 12) & 0b1111111U;
            uint8_t targetPieceType = (unMakeInfo >> 19) & 0b1111U;
            whiteToMove             = !whiteToMove;
            positionHash ^= zobristHashes[hash::whiteToMoveHash];
            moveClockPly--;

            //reHash any possible enpassantFileHash
            positionHash ^= zobristHashes[hash::enPassantFileHash + std::countr_zero(uint32_t(enpassantFiles) | 0x100U)];
        }


        
        //remaining functions
        bool inCheck(){
            if (whiteToMove) {
                return inCheckTemplate<true>();
            } else {
                return inCheckTemplate<false>();
            }
        }

        //repetition handling functions
        bool isOneRepetition() {

            /*the fifty move counter gives the amount of ply since the last capture or pawn move. As a result any position
            before can no longer be repeated. However the fiftyMoveClock is not reset when castling rights change eventough this is
            irreversible. As a result this does some repetitive work but it is good enough for now.*/

            /*in a "from fen" position the full history of the position is not known. As a result we can only look back till this position
            and not to the starting position.*/
            const uint16_t NMovesBack = std::min<uint16_t>( fiftyMoveClockPly, repetitionListPtr - &repetitionHashList[0] ) / 2;
            const uint64_t currentHash = positionHash;

            uint64_t* localHashListPtr = repetitionListPtr;
            for (int i = 0; i < NMovesBack; i++) {

                /*a repetition can only happen an even amount of plies ago. As a result I can advance the pointer two slots instead
                of one*/
                localHashListPtr -= 2;

                if (currentHash == *(localHashListPtr)) [[unlikely]] { //is repetition?
                    return true;
                }
            }

            return false;
        }

        bool isTwoRepetitions() {

            /*the fifty move counter gives the amount of ply since the last capture or pawn move. As a result any position
            before can no longer be repeated. However the fiftyMoveClock is not reset when castling rights change eventough this is
            irreversible. As a result this does some repetitive work but it is good enough for now.*/

            /*in a "from fen" position the full history of the position is not known. As a result we can only look back till this position
            and not to the starting position.*/
            const uint16_t NMovesBack = std::min<uint16_t>( fiftyMoveClockPly, repetitionListPtr - &repetitionHashList[0] ) / 2;
            const uint64_t currentHash = positionHash;

            uint64_t* localHashListPtr = repetitionListPtr;
            for (int i = 0; i < NMovesBack; i++) {

                /*a repetition can only happen an even amount of plies ago. As a result I can advance the pointer two slots instead
                of one*/
                localHashListPtr -= 2;

                if (currentHash == *(localHashListPtr)) [[unlikely]] { //is repetition?

                    //check for a second repetition
                    for ( ; i < NMovesBack; i++) {

                        localHashListPtr -= 2;

                        if (currentHash == *(localHashListPtr)) [[unlikely]] { //is repetition?

                            return true;
                        }
                    }
                }
            }

            return false;
        }



        private:    


        //helper functions

        void setBoardEmpty() {
            bitboard.fill(0);
            pieceAt.fill(12);
            occupied.fill(0);
            positionHash = 0ULL;
            moveClockPly = 0;
            whiteToMove = false;
            castlingFlags = 0;
            enpassantFiles = 0;
            fiftyMoveClockPly = 0;
        }    
        
        template<bool whiteMoved, bool capturedToTheRight, bool undoMove>
        void enpassantCapture(const uint16_t& startingIndex){
            using namespace constants;

            uint16_t capturedIndex = startingIndex + (capturedToTheRight ? 1 : -1);
            uint64_t capturedSquareBitboard = 1ULL << capturedIndex;
            constexpr uint8_t opponentPawnType = whiteMoved ? blackPawn : whitePawn;



            //update the boardstate
            bitboard[opponentPawnType]           ^= capturedSquareBitboard;
            pieceAt[capturedIndex]                = !undoMove ? emptySquare : opponentPawnType;
            occupied[whiteMoved ? black : white] ^= capturedSquareBitboard;
            occupied[combined]                   ^= capturedSquareBitboard;
            positionHash                         ^= zobristHashes[capturedIndex * 13 + opponentPawnType];
        }

        template<bool whiteSide, bool kingSide, bool undoMove>
        void castledRook() {
            using namespace constants;

            constexpr uint8_t startingIndexRook = (whiteSide ? 0 : 56) + (kingSide ? 7 : 0);
            constexpr uint64_t startingBitboardRook = 1ULL << startingIndexRook;
            constexpr uint8_t targetIndexRook = (whiteSide ? 0 : 56) + (kingSide ? 5 : 3);
            constexpr uint64_t targetBitboardRook = 1ULL << targetIndexRook;
            constexpr uint8_t rookType = whiteSide ? whiteRook : blackRook;
            constexpr uint8_t colorOfTheRook = whiteSide ? white : black;

            //flip the startingsquare of the rook
            bitboard[rookType]        ^= startingBitboardRook;
            occupied[colorOfTheRook]  ^= startingBitboardRook;
            occupied[combined]        ^= startingBitboardRook;
            pieceAt[startingIndexRook] = !undoMove ? emptySquare : rookType;
            positionHash              ^= zobristHashes[startingIndexRook * 13 + rookType];


            //flip the targetsquare of the rook
            bitboard[rookType]       ^= targetBitboardRook;
            occupied[colorOfTheRook] ^= targetBitboardRook;
            occupied[combined]       ^= targetBitboardRook;
            pieceAt[targetIndexRook]  = !undoMove ? rookType : emptySquare;
            positionHash              ^= zobristHashes[targetIndexRook * 13 + rookType];

        }

        template<bool whiteSide>
        bool inCheckTemplate(){
            using namespace constants;

            int friendlyKing   = whiteSide ? whiteKing : blackKing;
            int opponentKnight = whiteSide ? blackKnight : whiteKnight;
            int opponentRook   = whiteSide ? blackRook : whiteRook;
            int opponentQueen  = whiteSide ? blackQueen : whiteQueen;
            int opponentBishop = whiteSide ? blackBishop : whiteBishop;
            int opponentPawn   = whiteSide ? blackPawn : whitePawn;

            
            int kingIndex = std::countr_zero(bitboard[friendlyKing]);

            //add knights
            uint64_t checkingPieces = seenByKnight[kingIndex] & bitboard[opponentKnight];
            if (checkingPieces != 0) [[unlikely]] {return true;}

            //add D12 pieces
            checkingPieces = seenByD12[kingIndex][_pext_u64(occupied[combined], attackD12[kingIndex])] & (bitboard[opponentBishop] | bitboard[opponentQueen]);
            if (checkingPieces != 0) [[unlikely]] {return true;}

            //add HV pieces
            checkingPieces = seenByHV[kingIndex][_pext_u64(occupied[combined], attackHV[kingIndex])] & (bitboard[opponentRook] | bitboard[opponentQueen]); //black rook and bishop
            if (checkingPieces != 0) [[unlikely]] {return true;}

            //add pawns
            //attack from the left
            checkingPieces |= bitShift<whiteSide ? -7 : 9>(bitboard[opponentPawn] & ~hFile) & bitboard[friendlyKing];

            //attack from the right
            checkingPieces |= bitShift<whiteSide ? -9 : 7>(bitboard[opponentPawn] & ~aFile) & bitboard[friendlyKing];

            if (checkingPieces != 0) [[unlikely]] {return true;}

            return false;
        }

        template<int shiftvalue>
        uint64_t bitShift(uint64_t unShifted) { //also initialized in moveGeneratorClass. Might be cleaner to make it a lambda.
            
            if constexpr (shiftvalue >= 0) {
                return unShifted << shiftvalue;
            } else {
                return unShifted >> (-shiftvalue);;
            }
        }


};        







#endif