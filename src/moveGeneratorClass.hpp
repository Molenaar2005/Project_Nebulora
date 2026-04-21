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

#ifndef MOVEGENERATORCLASS_HPP
#define MOVEGENERATORCLASS_HPP

#include<array>
#include<immintrin.h>

#include "boardClass.hpp"




//precomputed tables
extern std::array<uint64_t, 64>                   seenByKing;
extern std::array<uint64_t, 64>                   seenByKnight;
extern std::array<std::array<uint64_t, 512>, 64>  seenByD12;
extern std::array<std::array<uint64_t, 4096>, 64> seenByHV;
extern std::array<uint64_t, 64>                   attackHV;
extern std::array<uint64_t, 64>                   attackD12;
extern std::array<uint64_t, 4096>                 fromToTable;





class moveGeneratorClass {
    
    public:


    
    alignas(64) std::array<moveStruct, 20'000> moveStack;


        void legalMoves(boardClass& board, moveStruct*& firstEmptyMovePtr, searchNodeStruct* returnNodePtr = nullptr) {
            using namespace constants;
    
            //moveGenerator is a template function that allows unnecessary functions to be switched of based on gamestate.
            //so if for example white doesn't have queen side castling rights then the function shouldn't check if it can based on the boardposition.
    
            //NOTE: For simplicity and early development onlyCaptures does not include enpassant captures.

            uint8_t kingCastlingAllowed  = (board.whiteToMove ? (whiteKingCastle  & board.castlingFlags) : (blackKingCastle  & board.castlingFlags)) != 0;
            uint8_t queenCastlingAllowed = (board.whiteToMove ? (whiteQueenCastle & board.castlingFlags) : (blackQueenCastle & board.castlingFlags)) != 0;
            uint8_t enpassantAllowed     = board.enpassantFiles != 0;
    
    
            uint8_t templateType = (enpassantAllowed << 3) | (queenCastlingAllowed << 2) | (kingCastlingAllowed << 1) | static_cast<uint8_t>(board.whiteToMove);
    
            switch (templateType) {
                case 0b00000: return moveGenerator<0b00000>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b00001: return moveGenerator<0b00001>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b00010: return moveGenerator<0b00010>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b00011: return moveGenerator<0b00011>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b00100: return moveGenerator<0b00100>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b00101: return moveGenerator<0b00101>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b00110: return moveGenerator<0b00110>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b00111: return moveGenerator<0b00111>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b01000: return moveGenerator<0b01000>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b01001: return moveGenerator<0b01001>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b01010: return moveGenerator<0b01010>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b01011: return moveGenerator<0b01011>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b01100: return moveGenerator<0b01100>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b01101: return moveGenerator<0b01101>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b01110: return moveGenerator<0b01110>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b01111: return moveGenerator<0b01111>(board, firstEmptyMovePtr, returnNodePtr);
                default: std::cerr << "incorrect legalMoves template" << std::endl;
            }
        }
        
        void quiescenceMoves(boardClass& board, moveStruct*& firstEmptyMovePtr, searchNodeStruct* returnNodePtr = nullptr) {
            using namespace constants;
        
            //moveGenerator is a template function that allows unnecessary functions to be switched of based on gamestate.
            
            //quiescence only produces captures and promotions.


            uint8_t enpassantAllowed     = board.enpassantFiles != 0;
            uint8_t templateType = (0b10000) | (enpassantAllowed << 3) | static_cast<uint8_t>(board.whiteToMove);
        
            switch (templateType) {

                case 0b10000: return moveGenerator<0b10000>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b10001: return moveGenerator<0b10001>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b11000: return moveGenerator<0b11000>(board, firstEmptyMovePtr, returnNodePtr);
                case 0b11001: return moveGenerator<0b11001>(board, firstEmptyMovePtr, returnNodePtr);
                default: std::cerr << "incorrect quiescenceMoves template" << std::endl;
                }
        }
        
        uint64_t perftRoot(boardClass& board, uint8_t depth) {
            if (depth == 0) [[unlikely]] {std::cout << "\nTotal leafNodes: " << 1ULL << "\n\n"; return 0; }

            moveStruct* const basePtr = &moveStack[0];
            moveStruct* firstEmptyPtr = basePtr;
            legalMoves(board, firstEmptyPtr);

            uint64_t totalLeafs = 0;

            while ((firstEmptyPtr) > basePtr) {
                firstEmptyPtr--;

                moveStruct currMove = *firstEmptyPtr;
                
                uint64_t unMakeInfo = board.makeMove<false, false>(currMove.move);
                uint64_t leafs = perftNode(board, depth - 1, firstEmptyPtr);
                board.unMakeMove(unMakeInfo, currMove.move);

                totalLeafs += leafs;

                std::cout << board.moveToCoordinates(currMove.move) << ": " << leafs << '\n';
            }

            std::cout << "\nTotal leafNodes: " << totalLeafs << "\n\n";
            return totalLeafs;
        }

        uint64_t perftNode(boardClass& board, uint8_t depth, moveStruct* baseMovePtr) {
            //leaf node return only reached at 2 ply from the root where bulk-counting has not yet kicked in.
            if (depth == 0) [[unlikely]] { return 1; }

            moveStruct* firstEmptyPtr = baseMovePtr;
            legalMoves(board, firstEmptyPtr);

            if (depth == 1) { //bulk counting
                return firstEmptyPtr - baseMovePtr;
            }


            uint64_t leafs = 0;

            while ((firstEmptyPtr) > baseMovePtr) {
                firstEmptyPtr--;

                moveStruct currMove = *firstEmptyPtr;
                
                uint64_t unMakeInfo = board.makeMove<false, false>(currMove.move);
                leafs += perftNode(board, depth - 1, firstEmptyPtr);
                board.unMakeMove(unMakeInfo, currMove.move);
            }

            return leafs;
        }


    private:

        template<bool whiteToMove>
        struct alignas(64) moveGenContextStruct {
            const boardClass& board;
            const uint64_t pinMaskD12;
            const uint64_t pinMaskHV;
            const uint64_t checkMask;
            const uint64_t seenByOpponent;
            const uint64_t friendlyBitboard;
            const uint64_t opponentBitboard;
        
            const uint8_t friendlyKingIndex;
            const uint8_t checksN;
            moveStruct* firstEmptyMovePtr;
        
            moveGenContextStruct(const boardClass& boardRef, moveStruct* baseMoveStackPtr) :
                board(boardRef),

                /* define a bitboard with valid squares that resolve an ongoing check (if any).
                knights and pawns get themselves added and sliding pieces get themselves and the squares in between added.
                this handles a single check perfectly except enpassant captures which get special logic. */
                checkMask(generateCheckMask<whiteToMove>(boardRef)),
                
                /* define Horizontal-vertical and diagonal12 bitboards. This bitboard has all rays from the king to the checking piece
                if the pinned piece is removed. So if a piece is not part of these bitboards then it can move freely. If it is part
                of one of these bitboards then the targetsquare also has to be within this bitboard.
                source: https://www.codeproject.com/Articles/5313417/Worlds-Fastest-Bitboard-Chess-Movegenerator */
                pinMaskD12(generatePinMaskD12<whiteToMove>(boardRef)),
                pinMaskHV(generatePinMaskHV<whiteToMove>(boardRef)),

                //more usefull bitboards
                seenByOpponent(generateSeenByOpponent<whiteToMove>(boardRef)),
                friendlyBitboard(boardRef.occupied[whiteToMove ? constants::white : constants::black]),
                opponentBitboard(boardRef.occupied[whiteToMove ? constants::black : constants::white]),

                //remaining variables
                friendlyKingIndex(std::countr_zero(boardRef.bitboard[whiteToMove ? constants::whiteKing : constants::blackKing])),
                firstEmptyMovePtr(baseMoveStackPtr),
                checksN((checkMask == 0) + (checkMask != ~0ULL)) {
            }
        };

        template<uint8_t templateType>
        static void moveGenerator(const boardClass& boardRef, moveStruct*& baseMoveStackPtr, searchNodeStruct* returnNodePtr) {
            using namespace constants;


            //unpack the templateType
            constexpr bool whiteToMove      = (templateType & 0b00001L) != 0;
            constexpr uint8_t castleRights  = (templateType & 0b00110L) >> 1;
            constexpr bool enpassantAllowed = (templateType & 0b01000L) != 0;
            constexpr bool inQuiescence     = (templateType & 0b10000U) != 0;

            //all the relevant moveGenerator variables are packed in a struct to reduce the amount of references that need to be passed.
            //moveGenContextStruct moveGenContext = moveGenContextStruct::moveGenContextStruct<whiteToMove>(boardRef, baseMoveStackIndex);
            moveGenContextStruct<whiteToMove> moveGenContext(boardRef, baseMoveStackPtr);
            
            //if there is a double check then only kingmoves may be valid
            bool onlyKingMoves = moveGenContext.checksN >= 2;
            if (!onlyKingMoves) {

                constexpr bool notPinned = false;
                constexpr bool pinned = true;

                pawnMoves<whiteToMove, enpassantAllowed, inQuiescence>(moveGenContext);

                knightMoves<whiteToMove, inQuiescence>(moveGenContext);

                slidingPiecesD12<whiteToMove, notPinned, inQuiescence>(moveGenContext);
                slidingPiecesD12<whiteToMove, pinned, inQuiescence>(moveGenContext);

                slidingPiecesHV<whiteToMove, notPinned, inQuiescence>(moveGenContext);
                slidingPiecesHV<whiteToMove, pinned, inQuiescence>(moveGenContext);

            }

            
            kingMoves<whiteToMove, inQuiescence, castleRights>(moveGenContext);

            //write back the required data
            baseMoveStackPtr = moveGenContext.firstEmptyMovePtr;

            if (returnNodePtr != nullptr) { //is a ptr for metadata given? (yes for search, no for perft)
                returnNodePtr->seenByOpponent = moveGenContext.seenByOpponent;
            }
        }

        template<bool whiteToMove>
        static uint64_t generateCheckMask(const boardClass& board){
            using namespace constants;


            const uint64_t opponentPawnBitboard   = board.bitboard[whiteToMove ? blackPawn   : whitePawn  ];
            const uint64_t opponentRookBitboard   = board.bitboard[whiteToMove ? blackRook   : whiteRook  ];
            const uint64_t opponentKnightBitboard = board.bitboard[whiteToMove ? blackKnight : whiteKnight];
            const uint64_t opponentBishopBitboard = board.bitboard[whiteToMove ? blackBishop : whiteBishop];
            const uint64_t opponentQueenBitboard  = board.bitboard[whiteToMove ? blackQueen  : whiteQueen ];
            const uint64_t friendlyKingBitboard   = board.bitboard[whiteToMove ? whiteKing   : blackKing  ];
            const uint64_t friendlyKingIndex      = std::countr_zero(friendlyKingBitboard);

            /* example knight. To find if a knight gives a check I can treat the king as a knight and see if the targetsquares
            contain an enemy knight. For a diagonal pin I can treat the king as a bishop. Remove all friendly pieces I can see.
            check all new pieces I can see and if they are a bishop or a queen of opposite color then they are pinning the removed piece.
            so ray from my king to the pinning piece gets added to the pinmask.
            source: https://www.codeproject.com/Articles/5313417/Worlds-Fastest-Bitboard-Chess-Movegenerator */

           
            //add knights
            uint64_t checkingPieces = seenByKnight[friendlyKingIndex] & opponentKnightBitboard;
            uint8_t checksN = std::popcount(checkingPieces);
            uint64_t checkMask = checkingPieces;

            /* add D12 pieces
            _pext_u64 is used for fast lookups for all squares seen by that piece. Thay are loaded in during startup. The idea is that
            attack bitboard contains all squares that might influence mobitlity. Next I check how many of those are non empty. Next I use
            pext for extracting that combination of blocking pieces for an index into the lookuptable.
            source: https://www.codeproject.com/Articles/5313417/Worlds-Fastest-Bitboard-Chess-Movegenerator */
            checkingPieces = seenByD12[friendlyKingIndex][_pext_u64(board.occupied[combined], attackD12[friendlyKingIndex])] & (opponentBishopBitboard | opponentQueenBitboard);
            checksN       += std::popcount(checkingPieces);
            checkMask     |= addToMask(friendlyKingIndex, checkingPieces);
            
            //add Hv pieces
            checkingPieces = seenByHV[friendlyKingIndex][_pext_u64(board.occupied[combined], attackHV[friendlyKingIndex])] & (opponentRookBitboard | opponentQueenBitboard);
            checksN       += std::popcount(checkingPieces);
            checkMask     |= addToMask(friendlyKingIndex, checkingPieces);
            
            //add pawns
            { //all valid king squares for a valid pawn check from the left (prevent wrap arround checks)
                uint64_t validKingSquares = friendlyKingBitboard & ~aFile;
    
                //shift to the startingsquare of that pawn and check if an enemy pawn exists at that square
                constexpr int shiftValue = whiteToMove ? 7 : -9;
                checkingPieces = opponentPawnBitboard & bitShift<shiftValue>(validKingSquares);
            }
            
            { //all valid king squares for a valid pawn check from the right (prevent wrap arround checks)
                uint64_t validKingSquares = (friendlyKingBitboard & ~hFile);

                //shift to the startingsquare of that pawn and check if an enemy pawn exists at that square
                constexpr int shiftValue = whiteToMove ? 9 : -7;
                checkingPieces |= opponentPawnBitboard & bitShift<shiftValue>(validKingSquares);
            }

            checkMask |= checkingPieces;
            checksN   += std::popcount(checkingPieces);

            /* If the checkmask is empty then all squares on the board resolve the ongoing check so it can be set to maxvalue
            if this position is in double check then 0 is returned which skips non king moves and set checksN to 2
            without the need for two return variables. */
            if (checksN >= 2) [[unlikely]] {
                return 0;
            } else if (checkMask == 0) [[likely]] {
                return std::numeric_limits<uint64_t>::max();
            } else {
                return checkMask;
            }
        }

        template<bool whiteToMove>
        static uint64_t generatePinMaskHV(const boardClass& board) {
            using namespace constants;

            /* pinmasks contain all the squares between the king and the piece that is pinning our piece. Splitting this mask into one for horizontal/vertical
            (HV) and one for both diagonals (D12) allows the movegenerator to detect if a move is legal. Because if a piece is part of a pinmask then it's
            targetsquare should also stay within this mask. This way the mobility of pinned pieces can be calculated without the need for expensive checks.*/

            const uint64_t opponentRookBitboard  = board.bitboard[whiteToMove ? blackRook  : whiteRook ];
            const uint64_t opponentQueenBitboard = board.bitboard[whiteToMove ? blackQueen : whiteQueen];
            const uint64_t allOccupiedSquares    = board.occupied[combined];
            const uint8_t friendlyKingIndex      = std::countr_zero(board.bitboard[whiteToMove ? whiteKing : blackKing]);
            
            //calculate the pinMaskHV
            //determine all pieces that might be pinned
            uint64_t potentialPins = seenByHV[friendlyKingIndex][_pext_u64(allOccupiedSquares, attackHV[friendlyKingIndex])];
            
            //remove potentially pinned pieces and check if a pinning pieces hides behind it.
            potentialPins = seenByHV[friendlyKingIndex][_pext_u64((allOccupiedSquares & ~potentialPins), attackHV[friendlyKingIndex])] & (opponentRookBitboard | opponentQueenBitboard);
            
            return addToMask(friendlyKingIndex, potentialPins);
        }

        template<bool whiteToMove>
        static uint64_t generatePinMaskD12(const boardClass& board) {
            using namespace constants;

            /* pinmasks contain all the squares between the king and the piece that is pinning our piece. Splitting this mask into one for horizontal/vertical
            (HV) and one for both diagonals (D12) allows the movegenerator to detect if a move is legal. Because if a piece is part of a pinmask then it's
            targetsquare should also stay within this mask. This way the mobility of pinned pieces can be calculated without the need for expensive checks.*/

            const uint64_t opponentBishopBitboard = board.bitboard[whiteToMove ? blackBishop : whiteBishop];
            const uint64_t opponentQueenBitboard  = board.bitboard[whiteToMove ? blackQueen  : whiteQueen ];
            const uint64_t allOccupiedSquares     = board.occupied[combined];
            const uint8_t friendlyKingIndex = std::countr_zero(board.bitboard[whiteToMove ? whiteKing : blackKing]);
                        
            //calculate the pinMaskHV
            //determine all pieces that might be pinned
            uint64_t potentialPins = seenByD12[friendlyKingIndex][_pext_u64(allOccupiedSquares, attackD12[friendlyKingIndex])];
            
            //remove potentially pinned pieces and check if a pinning pieces hides behind it.
            potentialPins = seenByD12[friendlyKingIndex][_pext_u64((allOccupiedSquares & ~potentialPins), attackD12[friendlyKingIndex])] & (opponentBishopBitboard | opponentQueenBitboard);
            
            return addToMask(friendlyKingIndex, potentialPins);
        }

        template<bool whiteToMove, bool enpassantAllowed, bool inQuiescence>
        static void pawnMoves(moveGenContextStruct<whiteToMove> &ctx) {
            using namespace constants;

            const uint64_t friendlyPawns      = ctx.board.bitboard[whiteToMove ? whitePawn : blackPawn];
            const uint64_t opponentHVPieces   = ctx.board.bitboard[whiteToMove ? blackRook : whiteRook] 
                                              | ctx.board.bitboard[whiteToMove ? blackQueen : whiteQueen];
            const uint64_t allOccupiedSquares = ctx.board.occupied[combined];
            const uint64_t pinMaskD12         = ctx.pinMaskD12;
            const uint64_t pinMaskHV          = ctx.pinMaskHV;
            const uint64_t checkMask          = ctx.checkMask;
            const uint64_t opponentBitboard   = ctx.opponentBitboard;
            const uint8_t  friendlyKingIndex  = ctx.friendlyKingIndex;
            
            uint64_t enpassantCheckMask;
            uint64_t enpassantTargetMask;
            if constexpr (enpassantAllowed) {
                enpassantCheckMask = generateEnpassantCheckMask<whiteToMove>(ctx);
                enpassantTargetMask = bitShift<(whiteToMove ? 40 : 16)>(static_cast<uint64_t>(ctx.board.enpassantFiles));
            }



            { //one forward
              //one forward not pinned
              constexpr int shiftValue = whiteToMove ? 8 : -8;
              
              //define the startingbitboard
              uint64_t notPinned = friendlyPawns & ~pinMaskD12 & ~pinMaskHV;
              uint64_t pinnedHV  = friendlyPawns & ~pinMaskD12 &  pinMaskHV; //only HV pins for oneforward
              
              //define the targetbitboard
              uint64_t validTargetSquares  = bitShift<shiftValue>(notPinned);
                       validTargetSquares |= bitShift<shiftValue>(pinnedHV) & pinMaskHV;
                       validTargetSquares &= ~allOccupiedSquares & checkMask;

              //add to the list
              addPawnMove<whiteToMove, shiftValue, inQuiescence>(validTargetSquares, ctx);
            }

            { //two forward
              //two forward not pinned
              constexpr int shiftValue = whiteToMove ? 8 : -8;

              constexpr uint64_t openingRank = whiteToMove ? secondRank : seventhRank;

              uint64_t notPinned = friendlyPawns & openingRank & ~pinMaskD12 & ~pinMaskHV;
              uint64_t pinnedHV  = friendlyPawns & openingRank & ~pinMaskD12 &  pinMaskHV;
              
              //prune away non empty squares
              uint64_t oneForwardNotPinned = bitShift<shiftValue>(notPinned);
              uint64_t oneForwardPinnedHV  = bitShift<shiftValue>(pinnedHV);
              oneForwardNotPinned &= ~allOccupiedSquares;
              oneForwardPinnedHV  &= ~allOccupiedSquares;
  
              //filter out occupied, and check squares
              uint64_t validTargetSquares  = bitShift<shiftValue>(oneForwardNotPinned);
                       validTargetSquares |= bitShift<shiftValue>(oneForwardPinnedHV) & pinMaskHV;
                       validTargetSquares &= ~allOccupiedSquares & checkMask;
  
              //add to the list
              addPawnMove<whiteToMove, 2 * shiftValue, inQuiescence>(validTargetSquares, ctx);
            }

            { //capture left
              constexpr int shiftValue = whiteToMove ? 7 : -9;
                          
              //get all the startingSquares
              uint64_t notPinned = friendlyPawns & ~(pinMaskD12 | pinMaskHV | aFile);
              uint64_t pinnedD12 = friendlyPawns & pinMaskD12 & ~aFile;
              
              //find all the valid targetsquares              
              uint64_t validTargetSquares  = bitShift<shiftValue>(notPinned);
                       validTargetSquares |= bitShift<shiftValue>(pinnedD12) & pinMaskD12;
                       validTargetSquares &= opponentBitboard & checkMask;

              if constexpr (enpassantAllowed) {
                  //enpassant left
                  //calculate the targetsquare and compensate for checks and pins
                  uint64_t enpassantMove  = bitShift<shiftValue>(notPinned);
                           enpassantMove |= bitShift<shiftValue>(pinnedD12) & pinMaskD12;
                           enpassantMove &= enpassantCheckMask & enpassantTargetMask;
    
                  //check if the enpassant is pinned. This pawn may not be part of the pinmaskHV because the captured pawn is also in between.
                  //However because both are removed from this rank I need to check for this pin explicitly
                  uint64_t movedAndCapturedPawn = bitShift<whiteToMove ? -8 : 8>(enpassantMove) | bitShift<whiteToMove ? -7 : 9>(enpassantMove);
                  uint64_t targetsHV = seenByHV[friendlyKingIndex][_pext_u64((allOccupiedSquares & ~movedAndCapturedPawn) | enpassantMove, attackHV[friendlyKingIndex])];
                  bool legalEnpassantMove = (targetsHV & opponentHVPieces) == 0;
                  validTargetSquares |= enpassantMove * legalEnpassantMove;
                }
  
              //add to the list
              addPawnMove<whiteToMove, shiftValue, inQuiescence>(validTargetSquares, ctx);
            }

            { //capture right
              constexpr int shiftValue = whiteToMove ? 9 : -7;

              //get all the startingSquares
              uint64_t notPinned = friendlyPawns & ~(pinMaskD12 | pinMaskHV | hFile);
              uint64_t pinnedD12 = friendlyPawns & pinMaskD12 & ~hFile;

              //find all the valid targetsquares              
              uint64_t validTargetSquares  = bitShift<shiftValue>(notPinned);
                       validTargetSquares |= bitShift<shiftValue>(pinnedD12) & pinMaskD12;
                       validTargetSquares &= opponentBitboard & checkMask;

              if constexpr (enpassantAllowed) {
                  //enpassant
                  //calculate the targetsquare and compensate for checks and pins
                  uint64_t enpassantMove  = bitShift<shiftValue>(notPinned);
                           enpassantMove |= bitShift<shiftValue>(pinnedD12) & pinMaskD12;
                           enpassantMove &= enpassantCheckMask & enpassantTargetMask;
    
                  //check if the enpassant is pinned. This pawn may not be part of the pinmaskHV because the captured pawn is also in between.
                  //However because both are removed from this rank I need to check for this pin explicitly
                  uint64_t movedAndCapturedPawn = whiteToMove ? (bitShift<-8>(enpassantMove) | bitShift<-9>(enpassantMove)) : (bitShift<8>(enpassantMove) | bitShift<7>(enpassantMove));
                  uint64_t targetsHV = seenByHV[friendlyKingIndex][_pext_u64((allOccupiedSquares & ~movedAndCapturedPawn) | enpassantMove, attackHV[friendlyKingIndex])];
                  bool legalEnpassantMove = (targetsHV & opponentHVPieces) == 0;
                  validTargetSquares |= enpassantMove * legalEnpassantMove;
              }
              
              addPawnMove<whiteToMove, shiftValue, inQuiescence>(validTargetSquares, ctx);
            }

        }

        template<bool whiteToMove, bool inQuiescence>
        static void knightMoves(moveGenContextStruct<whiteToMove> &ctx) {
            using namespace constants;

            const uint64_t checkMask        = ctx.checkMask;
            const uint64_t pinMaskD12       = ctx.pinMaskD12;
            const uint64_t pinMaskHV        = ctx.pinMaskHV;

            
            uint64_t toMove         = ctx.board.bitboard[whiteToMove ? whiteKnight : blackKnight];
                     toMove         &= ~(pinMaskD12 | pinMaskHV);

            while (toMove != 0) {
                
                //get first piece, convert it to an index and remove it from toMove
                uint64_t startingIndex = std::countr_zero(toMove);
                toMove ^= 1ULL << startingIndex;

                //calculate all the valid targetsquares
                uint64_t validTargetSquares = seenByKnight[startingIndex] & checkMask;

                //add to the list
                addMoves<whiteToMove, whiteKnight, inQuiescence>(startingIndex, validTargetSquares, ctx);
            }
        }

        template<bool whiteToMove, bool isPinned, bool inQuiescence>
        static void slidingPiecesD12(moveGenContextStruct<whiteToMove> &ctx) {
            using namespace constants;

            constexpr uint8_t friendlyBishop  = whiteToMove ? whiteBishop : blackBishop;
            constexpr uint8_t friendlyQueen   = whiteToMove ? whiteQueen  : blackQueen;
            const uint64_t allOccupiedSquares = ctx.board.occupied[combined];
            const uint64_t checkMask          = ctx.checkMask;
            const uint64_t pinMaskD12         = ctx.pinMaskD12;
            const uint64_t pinMaskHV          = ctx.pinMaskHV;

            uint64_t toMove  = ctx.board.bitboard[friendlyBishop] | ctx.board.bitboard[friendlyQueen];
                     toMove &= ~pinMaskHV & (isPinned ? pinMaskD12 : ~pinMaskD12);

            while (toMove != 0) {
                
                //get first piece, convert it to an index and remove it from toMove
                uint64_t startingIndex = std::countr_zero(toMove);
                toMove ^= 1ULL << startingIndex;

                //calculate all the valid targetsquares
                uint64_t seenByDiagonal = seenByD12[startingIndex][_pext_u64(allOccupiedSquares, attackD12[startingIndex])];
                uint64_t validTargetSquares =  seenByDiagonal & checkMask;
                if constexpr (isPinned) { validTargetSquares &= pinMaskD12;}

                //add to the list
                addMoves<whiteToMove, whiteBishop, inQuiescence>(startingIndex, validTargetSquares, ctx);
            }
        }

        template<bool whiteToMove, bool isPinned, bool inQuiescence>
        static void slidingPiecesHV(moveGenContextStruct<whiteToMove> &ctx) {
            using namespace constants;

            constexpr uint16_t friendlyRook   = whiteToMove ? whiteRook : blackRook;
            constexpr uint8_t  friendlyQueen  = whiteToMove ? whiteQueen  : blackQueen;
            const uint64_t checkMask          = ctx.checkMask;
            const uint64_t pinMaskHV          = ctx.pinMaskHV;
            const uint64_t pinMaskD12         = ctx.pinMaskD12;
            const uint64_t allOccupiedSquares = ctx.board.occupied[combined];



            uint64_t toMove  = ctx.board.bitboard[friendlyRook] | ctx.board.bitboard[friendlyQueen];
                     toMove &= ~pinMaskD12 & (isPinned ? pinMaskHV : ~pinMaskHV);

            while (toMove != 0) {

                //get first piece, convert it to an index and remove it from toMove
                uint64_t startingIndex = std::countr_zero(toMove);
                toMove ^= 1ULL << startingIndex;

                //calculate all the valid targetsquares
                uint64_t targetsHV = seenByHV[startingIndex][_pext_u64(allOccupiedSquares, attackHV[startingIndex])];
                uint64_t validTargetSquares =  targetsHV & checkMask;
                if constexpr (isPinned) {validTargetSquares &= pinMaskHV;}

                //add to the list
                addMoves<whiteToMove, whiteRook, inQuiescence>(startingIndex, validTargetSquares, ctx);
            }
        }

        template<bool whiteToMove, bool inQuiescence, uint8_t castleRights>
        static void kingMoves(moveGenContextStruct<whiteToMove> &ctx) {
            using namespace constants;

            //unpack the castlingrights
            constexpr bool queenCastleRight  = ( castleRights & whiteQueenCastle ) != 0;
            constexpr bool kingCastleRight   = ( castleRights & whiteKingCastle  ) != 0;
            const uint64_t friendlyKingIndex = ctx.friendlyKingIndex;
            const uint64_t seenByOpponent    = ctx.seenByOpponent;

            //valid targetSquares
            uint64_t friendlyKingBitboard = whiteToMove ? ctx.board.bitboard[whiteKing] : ctx.board.bitboard[blackKing];
            uint64_t validTargetSquares = seenByKing[friendlyKingIndex] & ~seenByOpponent;

            //castling
            if constexpr (queenCastleRight) {validTargetSquares |= castlingMove<whiteToMove, false>(ctx);}
            if constexpr (kingCastleRight)  {validTargetSquares |= castlingMove<whiteToMove, true>(ctx) ;}

            addMoves<whiteToMove, whiteKing, inQuiescence>(ctx.friendlyKingIndex, validTargetSquares, ctx);
        }
            
        template<bool whiteToMove, bool kingSide>
        static uint64_t castlingMove(moveGenContextStruct<whiteToMove> &ctx) {
            using namespace constants;

            //constants
            //It is not allowed to castle through a piece or through a square seen by the opponent. These masks are precomputed and shifted to blacks
            //half if it is black to move.
            uint64_t friendlyKingBitboard = whiteToMove ? ctx.board.bitboard[whiteKing] : ctx.board.bitboard[blackKing];
            constexpr uint64_t shouldBeEmpty   = (kingSide ? 0b1100000ULL : 0b01110ULL) << (56 * !whiteToMove);
            constexpr uint64_t shouldNotBeSeen = (kingSide ? 0b1110000ULL : 0b11100ULL) << (56 * !whiteToMove); 
            constexpr uint64_t shiftValue      = kingSide ? 2 : -2;
            const uint64_t seenByOpponent      = ctx.seenByOpponent;
            const uint64_t allOccupiedSquares  = ctx.board.occupied[combined];

            bool castleAllowed = ((allOccupiedSquares & shouldBeEmpty)           //castlingSquares not empty
                               |  (seenByOpponent & shouldNotBeSeen  ) ) == 0;   //castling through an attacked square

            return bitShift<shiftValue>(friendlyKingBitboard) * castleAllowed;
        }

        template<bool whiteToMove, uint8_t pieceType, bool inQuiescence>
        static void addMoves(uint8_t startingIndex, uint64_t validTargetSquares, moveGenContextStruct<whiteToMove> &ctx){

            //inQuiescence only allow captures
            if constexpr (inQuiescence) {
                validTargetSquares &= ctx.opponentBitboard;
            } else {
                validTargetSquares &= ~ctx.friendlyBitboard;
            }


            //loop over all the targetsquares
            while (validTargetSquares != 0) {

                //extract and remove the piece
                uint8_t targetIndex = std::countr_zero(validTargetSquares);
                uint64_t targetBitboard = 1ULL << targetIndex;
                validTargetSquares ^= targetBitboard;

                //convert to decimal index and add to movelist
                (*ctx.firstEmptyMovePtr++).move = startingIndex | (targetIndex << 6);
            }
        }

        template<bool whiteToMove, int shiftValue, bool inQuiescence>
        static void addPawnMove(uint64_t validTargetSquares, moveGenContextStruct<whiteToMove> &ctx) {
            using namespace constants;

            //in quiescence search only generate captures, checks and promotions
            if constexpr (inQuiescence) {
                validTargetSquares = validTargetSquares & ctx.opponentBitboard                   //captures
                                   | validTargetSquares & (whiteToMove ? eightRank : firstRank); //promotions
            }

            //loop over all the targetsquares
            while (validTargetSquares != 0) {
                
                //get and remove first targetsquare
                uint64_t targetBitBoard = validTargetSquares & -validTargetSquares;
                validTargetSquares ^= targetBitBoard;
                uint16_t targetIndex = std::countr_zero(targetBitBoard);
                uint16_t startingIndex = targetIndex - shiftValue;

                uint16_t baseMove = startingIndex | (targetIndex << 6);

                //promotions
                if ((targetBitBoard & (firstRank | eightRank)) != 0) [[unlikely]] {

                    //add all possible promotions: 1 = rook, 2 = knight, 3 = bishop, 4 = queen
                    (*ctx.firstEmptyMovePtr++).move = baseMove | (1U << 12);
                    (*ctx.firstEmptyMovePtr++).move = baseMove | (2U << 12);
                    (*ctx.firstEmptyMovePtr++).move = baseMove | (3U << 12);
                    (*ctx.firstEmptyMovePtr++).move = baseMove | (4U << 12);
                } else {
                    //most pawn moves are not promotions so promotion = 0
                    (*ctx.firstEmptyMovePtr++).move = baseMove;
                }
            }
        }

        static uint64_t addToMask(uint64_t kingIndex, uint64_t checkingPieces){

            //start with an empty checkmask
            uint64_t checkMask = 0;

            //determine how many pieces we have to add
            int itterations = std::popcount(checkingPieces);

            //all checking pieces
            for (int counter = 0; counter < itterations; counter++) {

                //get bitboard, convert to decimal index, remove from checkingpieces
                uint64_t lowestBitBoard = checkingPieces & -checkingPieces; 
                uint64_t targetIndex = std::countr_zero(lowestBitBoard);
                checkingPieces ^= lowestBitBoard;

                //look up all the squares inbetween the checking piece and the king and add to check mask
                checkMask |= fromToTable[kingIndex + (targetIndex << 6)];
            }

            return checkMask;
        }

        template<int shiftValue>
        static uint64_t bitShift(uint64_t unshifted) {
            if constexpr (shiftValue >= 0) {
                return unshifted << shiftValue;
            } else {
                return unshifted >> -shiftValue;
            }
        }

        template<bool whiteToMove>
        static uint64_t generateEnpassantCheckMask(const moveGenContextStruct<whiteToMove> &ctx) {
            using namespace constants;

            /* if the king is in check from a pawn that can be captured via enpassant then the enpassant square
            itself is also a valid targetsquare for a pawn. So for enpassant move generation a slightly adjusted
            checkMask is needed*/

            uint64_t opponentPawns = whiteToMove ? ctx.board.bitboard[blackPawn] : ctx.board.bitboard[whitePawn];
            uint64_t enpassantPawnMask = static_cast<uint64_t>(ctx.board.enpassantFiles);

            //lay the enpassant mask over the checkmask
            constexpr int shiftValue = whiteToMove ? 32 : 24;
            uint64_t enpassantCheckingPawns = ctx.checkMask & bitShift<shiftValue>(enpassantPawnMask);

            return ctx.checkMask | bitShift<(whiteToMove ? 8 : -8)>(enpassantCheckingPawns);
        }

        template<bool whiteToMove>
        static uint64_t generateSeenByOpponent(const boardClass& board){
            using namespace constants;

            uint64_t seenByOpponent = 0ULL;
            uint64_t friendlyKingBitBoard = whiteToMove ? board.bitboard[whiteKing] : board.bitboard[blackKing];

            { //add pawns
              uint64_t opponentPawns = board.bitboard[whiteToMove ? blackPawn : whitePawn];

              { //attack to the left
                constexpr int shiftValue = whiteToMove ? -9 : 7;
                seenByOpponent |= bitShift<shiftValue>(opponentPawns & ~aFile);
              }

              { //attack to the right
                constexpr int shiftValue = whiteToMove ? -7 : 9;
                seenByOpponent |= bitShift<shiftValue>(opponentPawns & ~hFile);
              }
            }

            { //HV sliding pieces
                uint64_t toAdd = board.bitboard[whiteToMove ? blackRook : whiteRook] | board.bitboard[whiteToMove ? blackQueen : whiteQueen];
                while (toAdd != 0) {

                    //get first piece, convert it to an index and remove it from toAdd
                    uint64_t squareIndex = std::countr_zero(toAdd);
                    toAdd ^= 1ULL << squareIndex;

                    //calculate all the squares it can see
                    seenByOpponent |= seenByHV[squareIndex][_pext_u64(board.occupied[combined] & ~friendlyKingBitBoard, attackHV[squareIndex])];
                }
            }

            { //D12 sliding pieces
                uint64_t toAdd = board.bitboard[whiteToMove ? blackBishop : whiteBishop] | board.bitboard[whiteToMove ? blackQueen : whiteQueen];
                while (toAdd != 0) {

                    //get first piece, convert it to an index and remove it from toAdd
                    uint64_t squareIndex = std::countr_zero(toAdd);
                    toAdd ^= 1ULL << squareIndex;

                    //calculate all the squares it can see
                    seenByOpponent |= seenByD12[squareIndex][_pext_u64(board.occupied[combined] & ~friendlyKingBitBoard, attackD12[squareIndex])];
                }
            }

            { //add knights
                uint64_t toAdd = board.bitboard[whiteToMove ? blackKnight : whiteKnight];
                while (toAdd != 0) {

                    //get first piece, convert it to an index and remove it from toAdd
                    uint64_t squareIndex = std::countr_zero(toAdd);
                    toAdd ^= 1ULL << squareIndex;

                    //calculate all the squares it can see
                    seenByOpponent |= seenByKnight[squareIndex];
                }
            }

            { //add opponent king
                uint64_t opponentKingBitboard = board.bitboard[whiteToMove ? blackKing : whiteKing];
                uint64_t squareIndex = std::countr_zero(opponentKingBitboard);

                seenByOpponent |= seenByKing[squareIndex];
            }

            return seenByOpponent;
        }

};





#endif