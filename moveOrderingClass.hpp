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
            while ((iMovePtr--) > (nodePtr->historyBaseIndexPtr)) {
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
            using namespace constants;

            /* for better cache locality I loop back over the movestack. with baseIndexPtr to the first added move
            which will be scored last. And currMovePtr to the first empty slot in the moveStack.

            quiescence only uses mvv-lva ordering.
            */

            moveStruct* iMovePtr = nodePtr->currMovePtr; //copy pointer
            const uint64_t seenByOpponent = nodePtr->seenByOpponent;

            while ((iMovePtr--) > (nodePtr->baseIndexPtr)) {

                uint8_t startingIndex = iMovePtr->move & 0b111111ULL;
                uint8_t targetIndex   = ((iMovePtr->move) >> 6) & 0b111111ULL;
                uint8_t promotion     = iMovePtr->move >> 12;

                uint8_t startingType = board.pieceAt[startingIndex];
                uint8_t targetType   = board.pieceAt[targetIndex];

               int16_t materialGain   = int16_t( (0x0093351093351 >> (targetType * 4)   ) & 0b1111ULL);
               int16_t materialLoss   = int16_t( (0x0093351093351 >> (startingType * 4) ) & 0b1111ULL);
               int16_t promotionvalue = int16_t( (0x0000000082240 >> (promotion * 4)    ) & 0b1111ULL);
               int16_t isDefended     = ((1ULL << targetIndex) & seenByOpponent) != 0;            
               iMovePtr->value        = materialGain + promotionvalue - (materialLoss + promotionvalue) * isDefended;
            }

        //Search loops backwards over the moves for improved cache locality. As a result the moves need to be sorted
        //from low to high value. This is also why the LastMovePtr is the start of the array and the firstEmptyMovePtr is the first
        //empty slot after all the moves.
        //std::sort(nodePtr->baseIndexPtr, nodePtr->currMovePtr, [](moveStruct& moveOne, moveStruct& moveTwo) {return moveOne.value < moveTwo.value;});
        insertionSort(nodePtr->baseIndexPtr, nodePtr->currMovePtr);
    }

        void negaMax(boardClass& board, searchNodeStruct* nodePtr, TTentryStruct* ttEntryPtr) {
            using namespace constants;

            /* for better cache locality I loop back over the movestack. with baseIndexPtr to the first added move
            which will be scored last. And currMovePtr to the first empty slot in the moveStack.
            TO BE ADDED:
            -TTmove
            -Promotion
            -History
            -Counter history
            -Follow-up history
            -SEE
            -Killer heuristic?
            */


            moveStruct* iMovePtr = nodePtr->currMovePtr; //copy pointer

            const uint64_t seenByOpponent = nodePtr->seenByOpponent;
            const uint16_t ttMove = (ttEntryPtr != nullptr) ? ttEntryPtr->move : 0;
            const uint16_t firstKillerMove  = getFirstKillerMove(nodePtr);
            const uint16_t secondKillerMove = getSecondKillerMove(nodePtr);



            while ((iMovePtr--) > (nodePtr->baseIndexPtr)) {

                uint8_t startingIndex = iMovePtr->move & 0b111111ULL;
                uint8_t targetIndex   = ((iMovePtr->move) >> 6) & 0b111111ULL;
                uint8_t promotion     = iMovePtr->move >> 12;

                uint8_t startingType = board.pieceAt[startingIndex];
                uint8_t targetType   = board.pieceAt[targetIndex];

                uint8_t isCapture   = targetType != emptySquare;
                uint8_t isTactical = (isCapture | promotion) != 0;

                
                //The transposition table move is always first
                if (iMovePtr->move == ttMove) [[unlikely]] {
                    iMovePtr->value = 32750;
                } else
                
                // captures and promotions are given priority if they are not expected to lose material
                if (isTactical) [[unlikely]] {
                    
                    int32_t materialGain   = int32_t( (0x0093351093351 >> (targetType   * 4) ) & 0b1111ULL);
                    int32_t materialLoss   = int32_t( (0x0093351093351 >> (startingType * 4) ) & 0b1111ULL);
                    int32_t promotionvalue = int32_t( (0x0000000082240 >> (promotion    * 4) ) & 0b1111ULL);
                    int32_t isDefended     = ((1ULL << targetIndex) & seenByOpponent) != 0;
                    
                    int value       = materialGain + promotionvalue - (materialLoss + promotionvalue) * isDefended + 32'700;
                    iMovePtr->value = int16_t(value - (value < 32'700) * 65'000);
                } else
                
                //killer moves are recent quiet moves that resulted in a beta cut
                if (iMovePtr->move == firstKillerMove) [[unlikely]] {

                    iMovePtr->value = 32'500;
                } else

                if (iMovePtr->move == secondKillerMove) [[unlikely]] {

                    iMovePtr->value = 32'250;
                } else
                

                //remaining moves are sorted by history
                {
                    //int16_t isDefended = ((1ULL << targetIndex) & seenByOpponent) != 0;
                    //iMovePtr->value = 5 - isDefended;

                    iMovePtr->value = history[startingType * 64 + targetIndex];
                }
            }

        //Search loops backwards over the moves for improved cache locality. As a result the moves need to be sorted
        //from low to high value. This is also why the LastMovePtr is the start of the array and the firstEmptyMovePtr is the first
        //empty slot after all the moves.
        //std::sort(nodePtr->baseIndexPtr, nodePtr->currMovePtr, [](moveStruct& moveOne, moveStruct& moveTwo) {return moveOne.value < moveTwo.value;});
        insertionSort(nodePtr->baseIndexPtr, nodePtr->currMovePtr);
    }

    
        
        void updateKillerMoves(boardClass& board, searchNodeStruct* nodePtr) {

            //move contains a expected value and the raw move.
            //for now promotions and checks are also treated as killers.
            //possible improvement is to treat all tactical (including promotions and checks) moves as non-killers
            uint16_t rawMove = nodePtr->bestMove.move;
            uint16_t targetIndex = (rawMove >> 6) & 0b111111ULL;
            bool isCapture = board.pieceAt[targetIndex] != constants::emptySquare;

            if (isCapture) {
                return;
            }

            uint8_t plyFromRoot = nodePtr->ply;


            /*
            When updating killers it is required for the new move to be different.
            If the second killer equals the new move then the normal approach of rejecting the second and moving the
            first to second works fine. However if the first move equals the newKiller then an early return is needed
            to prevent storing the same killerMove twice.
            */
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
                uint16_t startingType = packedMove         & uint16_t(0b1111ULL);
                uint16_t targetSquare = (packedMove >> 4)  & uint16_t(0b111111ULL);
                uint16_t isBetaCut    = (packedMove >> 10) & uint16_t(0b1);
                int16_t sign          = -1 + 2 * isBetaCut;

                constexpr int16_t maxHistory = 512; // abs(x) <= 30k
                int16_t clampedBonus = std::clamp<int16_t>( sign * depth * depth, -maxHistory, maxHistory);
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
