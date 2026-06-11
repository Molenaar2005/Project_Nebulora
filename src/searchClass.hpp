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

#ifndef SEARCHCLASS_HPP
#define SEARCHCLASS_HPP

#include<bitset>
#include<array>
#include<iostream>

#include "globals.hpp"
#include "boardclass.hpp"
#include "moveGeneratorClass.hpp"
#include "moveOrderingClass.hpp"
#include "evaluationClass.hpp"
#include "transpositionTableClass.hpp"

#include<stdexcept>


class searchClass {

    public:
    alignas(64) std::array<searchNodeStruct, depth::maxPly> searchStack; //allignment is redundant since the struct is already alligned
    

    int16_t quiescenceSearch(searchEnvStruct& env, searchNodeStruct* nodePtr){

        /*  It is very dangerous to stop search completely once at a leafnode due to the horizon effect. In order to minimize this a quiescencesearch is used.
        This is a search that continues searching when a position is not quiet. Traditionally this means there is no check, no captures and no promotions.
        For now I only search captures for simplicity. However neglecting checks can result in ELO loss. Especially since static_evaluation
        is not reliable when in check. So not done yet but enough for now.*/

        //return immidiatly if out of time or the node budget is reached.
        if (hardLimitReached(env)) [[unlikely]] { return -constants::inf; }

        //stand pat
        //requires adjustment for positions in check since static eval is not allowed
        nodePtr->bestEval = nodePtr->staticEval;
        if (nodePtr->bestEval >= nodePtr->beta) {
            return nodePtr->bestEval; //beta cut off
        } 
        nodePtr->alpha = std::max(nodePtr->alpha, nodePtr->bestEval);

        
        //generate and sort moves
        env.moveGenerator.quiescenceMoves(env.board, nodePtr->currMovePtr, nodePtr); //updates nodePtr
        env.moveSorter.quiescence(env.board, nodePtr);


        //evaluate all the moves
        while (nodePtr->movesN > 0) {
            nodePtr->currMovePtr = env.moveSorter.movePicker(nodePtr);

            nodePtr->unMakeInfo = env.board.makeMove<false, false>(nodePtr->currMovePtr->move);

            setupNode<false>(env, nodePtr);
            nodePtr->currentEval = -quiescenceSearch(env, nodePtr + 1);

            env.board.unMakeMove(nodePtr->unMakeInfo, nodePtr->currMovePtr->move);


            //return immidiatly if out of time or the node budget is reached.
            if (hardLimitReached(env)) [[unlikely]] { return -constants::inf; }


            if (nodePtr->currentEval >= nodePtr->beta) {
                return nodePtr->currentEval;
            }

            if (nodePtr->currentEval >= nodePtr->alpha) {
                nodePtr->alpha = nodePtr->currentEval;
                nodePtr->bestEval = nodePtr->currentEval;
            }

            if (nodePtr->currentEval > nodePtr->bestEval) {
                nodePtr->bestEval = nodePtr->currentEval;
            }

        }

        return nodePtr->bestEval;
    }

    template<int8_t expectedType>
    int16_t negaMax(searchEnvStruct& env, searchNodeStruct* nodePtr){
        using namespace nodeType;

        //return immidiatly if out of time or the node budget is reached.
        if (hardLimitReached(env)) [[unlikely]] { return -constants::inf; }

        //check for repetition and 50-move rule. Insufficient material still left to do.
        if (drawReturn(env, nodePtr)) [[unlikely]] {return -int16_t(env.contemptValue);}


        //try an early reaturn from the TT table.
        //ttEarlyReturn writes the ttScore to nodePtr->bestEval when an early return is possible.
        if (ttEarlyReturn<expectedType>(env, nodePtr)) { return nodePtr->bestEval;}

        //if this is a leaf node then it requires a quiescence call. depth < onePly is for later support for fractional reductions.
        //NOTE: nodesetup is redundant if this is a call to quiescence since it does the same internally. Point for improvement.
        if (nodePtr->depth < depth::ply) { return quiescenceSearch(env, nodePtr); }

        //Null move pruning
        if (nullMovePruning<expectedType>(env, nodePtr)) {return nodePtr->beta;}
        
        //generate all legal moves
        env.moveGenerator.legalMoves(env.board, nodePtr->currMovePtr, nodePtr);

        //handle checkmate, stalemate.
        if (isGameEndingState(env, nodePtr)) { return nodePtr->bestEval; }

        //sort all the moves
        env.moveSorter.negaMax(env.board, nodePtr);

        //evaluate the pv move if inside a pv node
        bool earlyReturn = searchPVMove<expectedType>(env, nodePtr);
        if (earlyReturn) { return nodePtr->bestEval; }


        //evaluate all the moves
        while (nodePtr->movesN > 0) {
            nodePtr->currMovePtr = env.moveSorter.movePicker(nodePtr);

            nodePtr->unMakeInfo = env.board.makeMove<true, true>(nodePtr->currMovePtr->move, &env, nodePtr);
            
            if (zeroWindowPruning<expectedType>(env, nodePtr)) {

                // ALL ==> CUT, CUT ==> ALL, PV ==> PV
                constexpr int8_t nodePattern[] = {CUT, ALL, PV};
                constexpr int8_t expectedChildType = nodePattern[expectedType];

                setupNode<false>(env, nodePtr);
                nodePtr->currentEval = -negaMax<expectedChildType>(env, nodePtr + 1);
            }
            
            env.board.unMakeMove(nodePtr->unMakeInfo, nodePtr->currMovePtr->move);
            
            //return immidiatly if out of time or the node budget is reached.
            if (hardLimitReached(env)) [[unlikely]] { return -constants::inf; }
            
            bool isBetaCut = updateNodePtr(env, nodePtr);
            if (isBetaCut) { return nodePtr->bestEval; }
        }

        /*if no early return has been triggered in the move loop and the best score is lowerbound then it is a PV node*/
        nodePtr->trueType = (nodePtr->trueType == lowerBound) ? PV : ALL;
        env.tt.updateTT(env, nodePtr);

        return nodePtr->bestEval;
    }

    void rootSearch(searchContextStruct& ctx){
        using namespace nodeType;

        auto startTime = std::chrono::high_resolution_clock::now();

        uint64_t maxDepth = ctx.maxDepth * depth::ply;
        uint64_t maxTime  = calculateMaxTime(ctx);
        searchEnvStruct env(ctx, maxTime);
        env.tt.newGeneration();
        searchNodeStruct* baseNodePtr = setupBaseNodePtr(env);

        env.moveSorter.decayHistory();

        env.moveGenerator.legalMoves(env.board, baseNodePtr->currMovePtr, baseNodePtr);

        if (gameHasEnded(env.board, baseNodePtr)) {return;}

        //itterative deepening
        do {

            baseNodePtr->alpha = -constants::inf;
            baseNodePtr->beta = constants::inf;

            baseNodePtr->depth += depth::ply;

            //root does not use a movepicker to prevent regenerating moves
            env.moveSorter.negaMax(env.board, baseNodePtr);
            env.moveSorter.insertionSort(baseNodePtr->currMovePtr - baseNodePtr->movesN, baseNodePtr->currMovePtr);

            //evaluate all the moves
            moveStruct* currMovePtrCopy = baseNodePtr->currMovePtr;
            for (int i = baseNodePtr->movesN; i > 0; i--) {
                currMovePtrCopy--;

                baseNodePtr->unMakeInfo = env.board.makeMove<true, false>(currMovePtrCopy->move, &env);
                
                bool isFirstMove = i == baseNodePtr->movesN;

                if ( isFirstMove || zeroWindowPruning<PV>(env, baseNodePtr) ) {

                    setupNode<false>(env, baseNodePtr);
                    baseNodePtr->currentEval = -negaMax<PV>(env, baseNodePtr + 1);
                }
                
                env.board.unMakeMove(baseNodePtr->unMakeInfo, currMovePtrCopy->move);

                //return immidiatly if out of time or the node budget is reached.
                if (hardLimitReached(env)) [[unlikely]] {
                    std::cout << "bestmove " << env.board.moveToCoordinates(baseNodePtr->bestMove) << '\n';
                    ctx.maxNodes = env.nodes;  
                    return;
                }

                if (baseNodePtr->currentEval > baseNodePtr->alpha) { //inside window so far
                    baseNodePtr->alpha = baseNodePtr->currentEval;
                    baseNodePtr->bestMove = currMovePtrCopy->move;
                    baseNodePtr->bestEval = baseNodePtr->currentEval; //only used for TT updates.
                }
            }

            baseNodePtr->trueType = nodeType::PV; //can change for aspiration windows
            env.tt.updateTT(env, baseNodePtr);
            baseNodePtr->ttMove = env.tt.probeTTMove(env.board);
            baseNodePtr->TTHit = baseNodePtr->ttMove != 0;

            prinInfoLine(baseNodePtr, env, startTime);

        } while ((baseNodePtr->depth < maxDepth));

        std::cout << "bestmove " << env.board.moveToCoordinates(baseNodePtr->bestMove) << '\n'; 
        ctx.maxNodes = env.nodes;  
 
    }


    private:


        //helper functions for the root node
        searchNodeStruct* setupBaseNodePtr(searchEnvStruct& env) {

            searchNodeStruct* nodePtr = &searchStack[0];

            nodePtr->currMovePtr    = &env.moveGenerator.moveStack[0];
            nodePtr->quietsPtr      = &env.moveSorter.historyStack[0];
            nodePtr->seenByOpponent = 0;
            nodePtr->unMakeInfo     = 0;

            nodePtr->bestMove    = 0;
            nodePtr->ttMove      = env.tt.probeTTMove(env.board);
            nodePtr->currentEval = 0;
            nodePtr->shiftMargin = 0;
            nodePtr->staticEval  = env.evaluation.static_evaluation<false>(env.board);
            nodePtr->reduction   = 0;
            nodePtr->extension   = 0;
            nodePtr->bestEval    = std::numeric_limits<int16_t>::min();
            nodePtr->depth       = 0;
            nodePtr->alpha       = -constants::inf;
            nodePtr->beta        = constants::inf;
            
            nodePtr->lockedSquare   = std::numeric_limits<int16_t>::max(); //flag meaning unused
            nodePtr->quietsSearched = 0;
            nodePtr->trueType       = nodeType::upperBound; //an eval below alpha is an upperbound
            nodePtr->movesN         = 0;
            nodePtr->ply            = 0;
            nodePtr->TTIsCapture    = 0; //unused for now
            nodePtr->inCheck        = env.board.inCheck();
            nodePtr->TTHit          = nodePtr->ttMove != 0;

            env.selDepth = std::max(env.selDepth, nodePtr->ply);
            env.nodes++;
            
            return nodePtr;
        }

        uint64_t calculateMaxTime(searchContextStruct& ctx) {

            uint64_t timeOnTheClock = ctx.board.whiteToMove ? ctx.wtime : ctx.btime;
            uint64_t inc = ctx.board.whiteToMove ? ctx.wInc : ctx.bInc;
            uint64_t movesToGo = std::max(ctx.movesToGo, 1ULL);
            uint64_t maxTime = std::min(ctx.movetime, std::max(100ULL, timeOnTheClock / movesToGo + inc / 2) - 100ULL);

            return maxTime;
        }

        bool gameHasEnded(boardClass& board, searchNodeStruct* baseNodePtr) {

            if (board.isTwoRepetitions()) {
                std::cout << "info string this position is a three fold repetition\n";
                return false;
            }
            
            if (baseNodePtr->movesN == 0) {

                if (baseNodePtr->inCheck) {
                    std::cout << "info string " << ((board.whiteToMove) ? "black " : "white ") << "has won\n";
                } else {
                    std::cout << "info string this position is stalemate\n";
                }

                return false;
            }

            if (board.fiftyMoveClockPly >= 100) {
                std::cout << "info string this position is a draw by the 50-move draw rule\n";
                return false;
            }
            
            return false;
        }

        std::string commandLineScore(searchEnvStruct& env, searchNodeStruct* baseNodePtr) {
            
            //handle mating scores for the UCI protocol
            int16_t isMate = std::abs(baseNodePtr->alpha) > mateScores::mateThreshold;
            if (isMate) {

                int16_t sign        = (baseNodePtr->alpha > 0) ? 1 : -1;
                int16_t movesToMate = ( (mateScores::mateValue - std::abs(baseNodePtr->alpha)) + 1 ) / 2;

                return "mate " + std::to_string(sign * movesToMate);
            }

            return "cp " + std::to_string(baseNodePtr->alpha);
        }

        template<bool showDetailedState>
        std::string hashState(searchEnvStruct& env) {
            using namespace nodeType;

            uint64_t filledElements = env.tt.distribution[ALL]
                                    + env.tt.distribution[CUT]
                                    + env.tt.distribution[PV];

            uint64_t hashTotal = filledElements + env.tt.distribution[empty]; // hashTotal is never zero due to minium of 128 kb
            
            if constexpr (showDetailedState) {

                std::string detailedOutput = "";
                for (uint64_t nodeTypeCount:env.tt.distribution) {detailedOutput += " " + std::to_string((1000 * nodeTypeCount) / hashTotal);}
                return detailedOutput;
            }

            return " " + std::to_string((1'000 * filledElements) / hashTotal);
        }

        void prinInfoLine(searchNodeStruct* baseNodePtr, searchEnvStruct& env, std::chrono::time_point<std::chrono::high_resolution_clock> startTime) {
            using namespace std::chrono;


            auto currentTime = high_resolution_clock::now();
            uint64_t currentSearchTime = std::max<uint64_t>(1, duration_cast<milliseconds>(currentTime - startTime).count());

            std::cout << "info depth " << (baseNodePtr->depth / depth::ply)
                      << " seldepth "  << (uint16_t(env.selDepth))
                      << " score "     << commandLineScore(env, baseNodePtr)
                      << " nodes "     << (env.nodes) 
                      << " nps "       << ((1'000 * env.nodes) / currentSearchTime)
                      << " time "      << (currentSearchTime)
                      << " hashfull"   << hashState<false>(env)
                      << " pv "        << env.tt.pvLine(env.board, baseNodePtr->depth / depth::ply)
                      << std::endl; //prevent buffer delays
        }


        //helper functions for in negamax
        template<bool zeroWindow>
        void setupNode(searchEnvStruct& env, searchNodeStruct* parentPtr) {

            searchNodeStruct* childPtr = parentPtr + 1; //next frame in the stack

            childPtr->currMovePtr    = parentPtr->currMovePtr + 1;
            childPtr->quietsPtr      = parentPtr->quietsPtr;
            childPtr->seenByOpponent = 0;
            childPtr->unMakeInfo     = 0;

            childPtr->bestMove    = 0;
            childPtr->currentEval = 0;
            childPtr->shiftMargin = 0;
            childPtr->staticEval  = env.evaluation.static_evaluation<false>(env.board);
            childPtr->reduction   = 0;
            childPtr->extension   = parentPtr->extension;
            childPtr->bestEval    = std::numeric_limits<int16_t>::min();
            childPtr->depth       = parentPtr->depth - parentPtr->reduction - depth::ply; //extentions are negative reductions
            childPtr->beta        = -(parentPtr->alpha + parentPtr->shiftMargin);
            childPtr->alpha       = zeroWindow ? (childPtr->beta - 1) : -(parentPtr->beta + parentPtr->shiftMargin);
            
            childPtr->lockedSquare   = std::numeric_limits<int16_t>::max(); //flag meaning unused
            childPtr->quietsSearched = 0;
            childPtr->trueType       = nodeType::upperBound; //an eval below alpha is an upperbound
            childPtr->movesN         = 0;
            childPtr->ply            = parentPtr->ply + 1;
            childPtr->TTIsCapture    = 0;
            childPtr->inCheck        = env.board.inCheck();
            childPtr->TTHit          = 0;

            env.selDepth = std::max(env.selDepth, childPtr->ply);
            env.nodes++;
        }
    
        bool drawReturn(searchEnvStruct& env, searchNodeStruct* nodePtr) {

            if (env.board.fiftyMoveClockPly >= 100) [[unlikely]] {
                return true; //draw if 100 half moves have not resulted in a reset
            }

            if (nodePtr->ply > 2) [[likely]] { 
                if ( env.board.isOneRepetition()  ) [[unlikley]] { return true; }
            
            } else {
                if ( env.board.isTwoRepetitions() ) [[unlikley]] { return true; }
            }

            return false;
        }

        template<int8_t expectedType>
        bool ttEarlyReturn(searchEnvStruct& env, searchNodeStruct* nodePtr) {
            using namespace nodeType;
            using namespace packedBits;

            TTentryStruct* ttEntryPtr = env.tt.probeTT(env, nodePtr);
            nodePtr->TTHit = ttEntryPtr != nullptr;

            if (!nodePtr->TTHit) { //no tt hit
                nodePtr->ttMove = 0;
                return false;
            }
            nodePtr->ttMove = ttEntryPtr->move;

            uint16_t ttEntrydepth = ttEntryPtr->meta & uint16_t(fourteenBits);
            uint16_t ttNodeType   = ttEntryPtr->meta >> 14; // top 2 bits

            if (ttEntrydepth < nodePtr->depth) { //inssufficient depth for a return
                return false;
            }
    
            int16_t correctedScore = env.tt.localToMateScore(nodePtr, ttEntryPtr->eval);

            bool allTypeReturn = (ttNodeType == ALL) & (correctedScore <= nodePtr->alpha);
            bool cutTypeReturn = (ttNodeType == CUT) & (correctedScore >= nodePtr->beta);
            bool pvTypeReturn  = (ttNodeType == PV);
            
            
            //tt early returns can miss repetitions in some situations so a bestEval update
            //and early return is disabled. (bestMove is still used in move ordering)
            bool isPVNode = expectedType == PV;
            bool earlyReturnPossible = (allTypeReturn | cutTypeReturn | pvTypeReturn) & !isPVNode;

            nodePtr->bestEval = earlyReturnPossible ? correctedScore : nodePtr->bestEval;
            
            return earlyReturnPossible;
        }

        bool hardLimitReached(searchEnvStruct& env) {

            //only check the conditions every 4096 nodes for speed
            if ((env.nodes & 0xFFFULL) != 0ULL) [[likely]] {
                return false;
            }

            if ((env.endTime < std::chrono::high_resolution_clock::now()) || //out of time
                (env.nodes >= env.maxNodes) /*node budget exceeded*/) [[unlikely]] {
    
                return true;
            }

            return false;
        }
        
        bool isGameEndingState(searchEnvStruct& env, searchNodeStruct* nodePtr) {

            if (nodePtr->movesN == 0) { 
                if (nodePtr->inCheck) { //is branchless faster here?

                    //forced mate
                    nodePtr->bestEval = -mateScores::mateValue + nodePtr->ply;
                    return true;
                } else {
                    //stalemate
                    nodePtr->bestEval = -int16_t(env.contemptValue);
                    return true;
                }
            }

            return false;
        }

        bool updateNodePtr(searchEnvStruct& env, searchNodeStruct* nodePtr) {
            
            /*this block of if statements can be reduced in terms of branches.*/
            if (nodePtr->currentEval >= nodePtr->beta) { //beta cutoff
                nodePtr->bestMove = nodePtr->currMovePtr->move;
                nodePtr->trueType = nodeType::lowerBound; //the score is at least this value if it improved alpha
                nodePtr->bestEval = nodePtr->currentEval;

                bool isNonCapture = env.board.pieceAt[(((nodePtr->currMovePtr->move >> 6) & 0b111111))] == constants::emptySquare; //does not handle enpassant
                if (isNonCapture) {
                    env.moveSorter.updateKillerMoves(env.board, nodePtr);
                    env.moveSorter.markLastMoveAsBetaCut(nodePtr->quietsPtr);
                }
                env.moveSorter.updateHistory(nodePtr);
                env.tt.updateTT(env, nodePtr);
                return true;
            }

            if (nodePtr->currentEval > nodePtr->alpha) { //inside window so far
                nodePtr->alpha = nodePtr->currentEval;
                nodePtr->bestMove = nodePtr->currMovePtr->move;
                nodePtr->bestEval = nodePtr->currentEval;
                nodePtr->trueType = nodeType::lowerBound;
            }

            if (nodePtr->currentEval > nodePtr->bestEval) { //new best score found
                nodePtr->bestMove = nodePtr->currMovePtr->move;
                nodePtr->bestEval = nodePtr->currentEval;
            }

            return false; //beta cut is false
        }

        template<int8_t expectedType>
        bool nullMovePruning(searchEnvStruct& env, searchNodeStruct* nodePtr) {

            //null move pruning is only done at expected cut nodes
            if (expectedType != nodeType::CUT) { return false; }

            //checks to disable NMP
            bool sufficientDepth     = nodePtr->depth >= (depth::ply * 4);
            bool highEnoughEval      = (nodePtr->beta + 0) < nodePtr->staticEval;
            bool isZugZwang          = zugZwangLikely(env);
            bool notIncheck          = !nodePtr->inCheck;
            bool allowNMP = sufficientDepth & highEnoughEval & !isZugZwang & notIncheck;
            if (!allowNMP) {return false;}

            //reduce the null move search depth
            nodePtr->reduction = 2 * depth::ply + nodePtr->depth / 6; //very conservative
            
            //searchNullMove
            nodePtr->unMakeInfo = env.board.makeNullMove();

            setupNode<false>(env, nodePtr);
            int16_t nullEval = -negaMax<nodeType::CUT>(env, nodePtr + 1);
            env.board.unMakeNullMove(nodePtr->unMakeInfo);
            
            //restore the original depth
            nodePtr->reduction = 0;

            return nullEval > nodePtr->beta;
        }

        bool zugZwangLikely(searchEnvStruct& env) {
            using namespace constants;

            bool whiteToMove = env.board.whiteToMove;
            int blackOffSet = 6 * whiteToMove;
            uint64_t friendlyPieceValue = 0;
            friendlyPieceValue += 5 * std::popcount(env.board.bitboard[whiteRook   + blackOffSet]);
            friendlyPieceValue += 3 * std::popcount(env.board.bitboard[whiteKnight + blackOffSet]);
            friendlyPieceValue += 3 * std::popcount(env.board.bitboard[whiteBishop + blackOffSet]);
            friendlyPieceValue += 9 * std::popcount(env.board.bitboard[whiteQueen  + blackOffSet]);

            return friendlyPieceValue < 10;
        }

        template<int8_t expectedType>
        bool zeroWindowPruning(searchEnvStruct& env, searchNodeStruct* nodePtr) {
            using namespace nodeType;

            //zero window pruning is only attempted in pv nodes
            if constexpr (expectedType != PV) { return true; }

            setupNode<true>(env, nodePtr); //zeroWindow = true
            nodePtr->currentEval = -negaMax<CUT>(env, nodePtr + 1);

            //does this move need a full search?
            bool failLow  = nodePtr->currentEval <= nodePtr->alpha;
            bool failHigh = nodePtr->currentEval >= nodePtr->beta;
            bool needsFullSearch = !(failLow | failHigh);
            return needsFullSearch;
        }

        template<int8_t expectedType>
        bool searchPVMove(searchEnvStruct& env, searchNodeStruct* nodePtr) {
            using namespace nodeType;

            //only done at pv nodes
            if constexpr (expectedType != PV) { return false; }


            nodePtr->currMovePtr = env.moveSorter.movePicker(nodePtr);

            nodePtr->unMakeInfo = env.board.makeMove<true, true>(nodePtr->currMovePtr->move, &env, nodePtr);
            
            setupNode<false>(env, nodePtr);
            nodePtr->currentEval = -negaMax<PV>(env, nodePtr + 1);
            
            env.board.unMakeMove(nodePtr->unMakeInfo, nodePtr->currMovePtr->move);
            
            //return immidiatly if out of time or the node budget is reached.
            if (hardLimitReached(env)) [[unlikely]] { return true; }
            
            bool isBetaCut = updateNodePtr(env, nodePtr);
            return isBetaCut;
        }

};

#endif
