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

#ifndef MOVEORDERING_HPP
#define MOVEORDERING_HPP


#include<bitset>
#include<algorithm>
#include<array>
#include<limits>

#include "globals.hpp"
#include "boardClass.hpp"


class moveOrderingClass {

    public:

        std::array<uint16_t, 2 * depth::maxPly> killerMoves;
        alignas(64) std::array<uint16_t, 20'000> historyStack;
        alignas(64) std::array<int16_t, constants::Npieces * constants::Nsquares> history; //making color explicit can help reduce cache footprint.


        moveOrderingClass() {
            resetState();
        }

        void resetState() {
            killerMoves.fill(0);
            historyStack.fill(0);
            history.fill(0);
        }

        void updateHistory(searchNodeStruct* nodePtr) {

            uint16_t* iMovePtr = nodePtr->historyCurrMovePtr;
            int16_t depth = int16_t(nodePtr->depth / depth::ply); //depth in ply

            //reward or penalize all the moves in the stack
            for (int i = nodePtr->historyMovesN; i > 0; i--) {
                iMovePtr--;
                applyHistoryBonus(*iMovePtr, depth);
            }
        }

        void decayHistory() {
            for (int16_t &i:history) { i /= 4; }
        }

        void markLastMoveAsBetaCut(uint16_t* historyCurrMovePtr) {
            *(historyCurrMovePtr - 1) |= uint16_t(1ULL << 10);
        }



        void quiescence(boardClass& board, searchNodeStruct* nodePtr) {
            using namespace moveOrderingConstatns;
            using namespace packedBits;
            using namespace constants;

            /* for better cache locality I loop back over the movestack. with baseIndexPtr to the first added move
            which will be scored last. And currMovePtr to the first empty slot in the moveStack.

            quiescence only uses mvv-lva ordering.
            */

            moveStruct* iMovePtr = nodePtr->currMovePtr; //copy pointer
            const uint64_t seenByOpponent = nodePtr->seenByOpponent;

            for (int i = nodePtr->movesN; i > 0; i--){
                iMovePtr--;

                uint8_t startingIndex = iMovePtr->move          & sixBits;
                uint8_t targetIndex   = ((iMovePtr->move) >> 6) & sixBits;
                uint8_t promotion     = iMovePtr->move >> 12; // 3 bits

                uint8_t startingType = board.pieceAt[startingIndex];
                uint8_t targetType   = board.pieceAt[targetIndex];

                int16_t materialGain   = (packedMaterialValue  >> (targetType * 4)   ) & fourBits;
                int16_t materialLoss   = (packedMaterialValue  >> (startingType * 4) ) & fourBits;
                int16_t promotionvalue = (packedPromotionValue >> (promotion * 4)    ) & fourBits;
                int16_t isDefended     = ((1ULL << targetIndex) & seenByOpponent) != 0;            
                iMovePtr->value        = materialGain + promotionvalue - (materialLoss + promotionvalue) * isDefended;
            }

        //Search loops backwards over the moves for improved cache locality. As a result the moves need to be sorted
        //from low to high value. This is also why the LastMovePtr is the start of the array and the firstEmptyMovePtr is the first
        //empty slot after all the moves.
        insertionSort(nodePtr->currMovePtr - nodePtr->movesN, nodePtr->currMovePtr);
    }

        void negaMax(boardClass& board, searchNodeStruct* nodePtr, TTentryStruct* ttEntryPtr) {
            using namespace moveOrderingConstatns;
            using namespace packedBits;
            using namespace constants;

            /* for better cache locality I loop back over the movestack. with baseIndexPtr to the first added move
            which will be scored last. And currMovePtr to the first empty slot in the moveStack.
            TO BE ADDED:
            -Counter history
            -Follow-up history
            -SEE
            */


            moveStruct* iMovePtr = nodePtr->currMovePtr; //copy pointer

            const uint64_t seenByOpponent = nodePtr->seenByOpponent;
            const uint16_t ttMove = (ttEntryPtr != nullptr) ? ttEntryPtr->move : 0;
            const uint16_t firstKillerMove  = getFirstKillerMove(nodePtr);
            const uint16_t secondKillerMove = getSecondKillerMove(nodePtr);


            for (int i = nodePtr->movesN; i > 0; i--) {
                iMovePtr--;

                uint8_t startingIndex = iMovePtr->move          & sixBits;
                uint8_t targetIndex   = ((iMovePtr->move) >> 6) & sixBits;
                uint8_t promotion     = iMovePtr->move >> 12; // 3 bits

                uint8_t startingType = board.pieceAt[startingIndex];
                uint8_t targetType   = board.pieceAt[targetIndex];

                uint8_t isCapture   = targetType != emptySquare;
                uint8_t isTactical = (isCapture | promotion) != 0;

                
                //The transposition table move is always first
                if (iMovePtr->move == ttMove) [[unlikely]] {
                    iMovePtr->value = ttMoveBias;
                } else
                
                // captures and promotions are given priority if they are not expected to lose material
                if (isTactical) [[unlikely]] {
                    
                    int32_t materialGain   = (packedMaterialValue  >> (targetType   * 4) ) & fourBits;
                    int32_t materialLoss   = (packedMaterialValue  >> (startingType * 4) ) & fourBits;
                    int32_t promotionvalue = (packedPromotionValue >> (promotion    * 4) ) & fourBits;
                    int32_t isDefended     = ((1ULL << targetIndex) & seenByOpponent) != 0;
                    
                    int value       = materialGain + promotionvalue - (materialLoss + promotionvalue) * isDefended + winningCaptureBias;
                    iMovePtr->value = int16_t(value - (value < winningCaptureBias) * losingCaptureBias);
                } else
                
                //killer moves are recent quiet moves that resulted in a beta cut
                if (iMovePtr->move == firstKillerMove) [[unlikely]] {

                    iMovePtr->value = firstKillerBias;
                } else

                if (iMovePtr->move == secondKillerMove) [[unlikely]] {

                    iMovePtr->value = secondKillerBias;
                } else
                                
                { //remaining moves are sorted by history
                    iMovePtr->value = history[startingType * 64 + targetIndex];
                }
            }

        //Search loops backwards over the moves for improved cache locality. As a result the moves need to be sorted
        //from low to high value. This is also why the LastMovePtr is the start of the array and the firstEmptyMovePtr is the first
        //empty slot after all the moves.
        insertionSort(nodePtr->currMovePtr - nodePtr->movesN, nodePtr->currMovePtr);
    }

    
        
        void updateKillerMoves(boardClass& board, searchNodeStruct* nodePtr) {

            //move contains a expected value and the raw move.
            //for now promotions and checks are also treated as killers.
            //possible improvement is to treat all tactical (including promotions and checks) moves as non-killers
            uint16_t rawMove = nodePtr->bestMove;
            uint8_t plyFromRoot = nodePtr->ply;

            /* When updating killers it is required for the new move to be different.
            If the second killer equals the new move then the normal approach of rejecting the second and moving the
            first to second works fine. However if the first move equals the newKiller then an early return is needed
            to prevent storing the same killerMove twice. */
            bool isNewMove = killerMoves[2 * plyFromRoot] != rawMove;

            //shift first to second and make the first the new move. Unless the newMove is already in the first slot.
            killerMoves[2 * plyFromRoot + 1] = isNewMove ? killerMoves[2 * plyFromRoot] : killerMoves[2 * plyFromRoot + 1];
            killerMoves[2 * plyFromRoot] = rawMove;
        }

        uint16_t getFirstKillerMove(searchNodeStruct* nodePtr) {

            return killerMoves[nodePtr->ply * 2];
        }

        uint16_t getSecondKillerMove(searchNodeStruct* nodePtr) {

            return killerMoves[nodePtr->ply * 2 + 1];
        }


    private:

        void applyHistoryBonus(uint16_t packedMove, int16_t depth) {
            using namespace packedBits;

            uint16_t startingType = packedMove         & fourBits;
            uint16_t targetSquare = (packedMove >> 4)  & sixBits;
            uint16_t isBetaCut    = (packedMove >> 10) & oneBit;
            int16_t sign          = -1 + 2 * isBetaCut;

            constexpr int16_t maxHistory = 8192; // abs(x) <= 30k
            int16_t clampedBonus = std::clamp<int16_t>( sign * 4 * depth * depth, -maxHistory, maxHistory);
            uint64_t iHistory = startingType * 64 + targetSquare;
            history[iHistory] += clampedBonus - (history[iHistory] * std::abs(clampedBonus)) / maxHistory;
        }

        void insertionSort(moveStruct* firstPtr, moveStruct* lastPtr) {

            moveStruct* i = firstPtr;

            for (i++; i < lastPtr; i++) { //loop over every element

                moveStruct insertionElement = *i;
                moveStruct* j = i - 1;

                while ( (j >= firstPtr) && (j->value > insertionElement.value) ) {

                    *(j + 1)   = *(j);
                    j--;
                }

                *(j + 1) = insertionElement;
            }
        }
};








#endif
