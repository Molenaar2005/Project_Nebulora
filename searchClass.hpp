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
    

    int16_t quiescenceSearch(searchEnvStruct& env, searchNodeStruct* parentNodePtr){

        /*  It is very dangerous to stop search completely once at a leafnode due to the horizon effect. In order to minimize this a quiescencesearch is used.
        This is a search that continues searching when a position is not quiet. Traditionally this means there is no check, no captures and no promotions.
        For now I only search captures for simplicity. However neglecting checks can result in ELO loss. Especially since static_evaluation
        is not reliable when in check. So not done yet but enough for now.*/


        //setup the current node
        searchNodeStruct* nodePtr = setupNode(parentNodePtr);

        env.selDepth = std::max(env.selDepth, nodePtr->ply);
        env.nodes++;

        //return immidiatly if out of time or the node budget is reached.
        if (hardLimitReached(env)) [[unlikely]] { return -32750; }


        //stand pat
        //requires adjustment for positions in check since static eval is not allowed
        nodePtr->bestEval = env.evaluation.static_evaluation<false>(env.board);
        if (nodePtr->bestEval >= nodePtr->beta) {
            return nodePtr->bestEval; //beta cut off
        } 
        nodePtr->alpha = std::max(nodePtr->alpha, nodePtr->bestEval);

        
        //generate and sort moves
        env.moveGenerator.quiescenceMoves(env.board, nodePtr->currMovePtr, nodePtr);
        env.moveSorter.quiescence(env.board, nodePtr);


        //evaluate all the moves
        while (nodePtr->currMovePtr > nodePtr->baseIndexPtr) {
            nodePtr->currMovePtr--;

            /*
            //delta pruning
            constexpr uint64_t pieceValues     = 0x0093351093351ULL;
            constexpr uint64_t promotionValues = 0x00000000008224ULL;
            uint8_t targetIndex = ((nodePtr->currMovePtr->move >> 6) & 0b111111);
            uint8_t promotion   = ((nodePtr->currMovePtr->move >> 12) & 0b111);
            uint8_t targetPieceType = env.board.pieceAt[targetIndex];
            int16_t materialGain = pieceValues     >> (4 * targetPieceType);
            int16_t promotionGain = promotionValues >> (4 * promotion);
            constexpr int16_t margin = 80;
            if ((nodePtr->bestEval + (materialGain + promotionGain) * 100 + margin) < nodePtr->alpha) {
                continue;
            }
            */

            nodePtr->unMakeInfo = env.board.makeMove<false>(nodePtr->currMovePtr->move);
            nodePtr->currentEval = -quiescenceSearch(env, nodePtr);
            env.board.unMakeMove(nodePtr->unMakeInfo, nodePtr->currMovePtr->move);


            //return immidiatly if out of time or the node budget is reached.
            if (hardLimitReached(env)) [[unlikely]] { return -32750; }


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

    int16_t negaMax(searchEnvStruct& env, searchNodeStruct* parentNodePtr){

        searchNodeStruct* nodePtr = setupNode(parentNodePtr);
        env.selDepth = std::max(env.selDepth, nodePtr->ply);
        env.nodes++;

        //return immidiatly if out of time or the node budget is reached.
        if (hardLimitReached(env)) [[unlikely]] { return -32750; }

        //check for repetition
        if (repetitionReturn(env, nodePtr)) [[unlikely]] {return -int16_t(env.contemptValue);}


        //try an early reaturn from the TT table.
        //ttEarlyReturn writes the ttScore to nodePtr->bestEval when an early return is possible.
        TTentryStruct* ttEntryPtr = env.tt.probeTT(env, nodePtr);
        if (ttEarlyReturn(env, ttEntryPtr, nodePtr)) { return nodePtr->bestEval;}

        //if this is a leaf node then it requires a quiescence call. depth < onePly is for later support for fractional reductions.
        //NOTE: nodesetup is redundant if this is a call to quiescence since it does the same internally. Point for improvement.
        if (nodePtr->depth < depth::ply) {
            env.nodes--; //prevent double counting when entering quiescence.
          return quiescenceSearch(env, parentNodePtr);
        }
        
        //generate all legal moves
        env.moveGenerator.legalMoves(env.board, nodePtr->currMovePtr, nodePtr);

        //handle checkmate, stalemate. 50 move rule and insufficient material still left to do.
        if ((nodePtr->currMovePtr - nodePtr->baseIndexPtr) == 0) {
            if (env.board.inCheck()) {
                //forced mate
                return -mateScores::mateValue + nodePtr->ply;
             } else {
                //stalemate
                return -int16_t(env.contemptValue);
            }
        }

        env.moveSorter.negaMax(env.board, nodePtr, ttEntryPtr);


        //evaluate all the moves
        while (nodePtr->currMovePtr > nodePtr->baseIndexPtr) {
            nodePtr->currMovePtr--;

            nodePtr->unMakeInfo = env.board.makeMove<true>(nodePtr->currMovePtr->move, &env.tt);
            nodePtr->currentEval = -negaMax(env, nodePtr);
            env.board.unMakeMove(nodePtr->unMakeInfo, nodePtr->currMovePtr->move);
            
            //return immidiatly if out of time or the node budget is reached.
            if (hardLimitReached(env)) [[unlikely]] { return -32750; }
            
            env.moveSorter.writeToHistoryStack(env.board, nodePtr);

            /*this block of if statements can be reduced in terms of branches.*/
            if (nodePtr->currentEval >= nodePtr->beta) { //beta cutoff
                nodePtr->bestMove = *(nodePtr->currMovePtr);
                nodePtr->trueType = nodeType::CUT;
                nodePtr->bestEval = nodePtr->currentEval;
                env.moveSorter.updateKillerMoves(env.board, nodePtr);
                env.moveSorter.updateHistory(nodePtr);
                env.tt.updateTT(env, nodePtr);
                return nodePtr->currentEval;
            }

            if (nodePtr->currentEval > nodePtr->alpha) { //inside window so far
                nodePtr->alpha = nodePtr->currentEval;
                nodePtr->bestMove = *(nodePtr->currMovePtr);
                nodePtr->bestEval = nodePtr->currentEval;
                nodePtr->trueType = nodeType::CUT;
            }

            if (nodePtr->currentEval > nodePtr->bestEval) { //new best score found
                nodePtr->bestMove = *(nodePtr->currMovePtr);
                nodePtr->bestEval = nodePtr->currentEval;
            }
        }

        /*if no early return has been triggered in the move loop and the best score is lowerbound (CUT type) then it is a PV node*/
        nodePtr->trueType = (nodePtr->trueType == nodeType::CUT) ? nodeType::PV : nodeType::ALL;
        env.tt.updateTT(env, nodePtr);

        return nodePtr->bestEval;
    }

    void rootSearch(searchContextStruct& ctx){

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

            baseNodePtr->alpha = -32750;
            baseNodePtr->beta = 32750;

            baseNodePtr->depth += depth::ply;

            TTentryStruct* ttEntryPtr = env.tt.probeTT(env, baseNodePtr);
            env.moveSorter.negaMax(env.board, baseNodePtr, ttEntryPtr);

            //evaluate all the moves
            moveStruct* currMovePtrCopy = baseNodePtr->currMovePtr;
            while (currMovePtrCopy > baseNodePtr->baseIndexPtr) {
                currMovePtrCopy--;

                baseNodePtr->unMakeInfo = env.board.makeMove<true>(currMovePtrCopy->move, &env.tt);
                baseNodePtr->currentEval = -negaMax(env, baseNodePtr);
                env.board.unMakeMove(baseNodePtr->unMakeInfo, currMovePtrCopy->move);

                //return immidiatly if out of time or the node budget is reached.
                if (hardLimitReached(env)) [[unlikely]] {
                    std::cout << "bestmove " << env.board.moveToCoordinates(baseNodePtr->bestMove.move) << '\n';  
                    return;
                }

                if (baseNodePtr->currentEval > baseNodePtr->alpha) { //inside window so far
                    baseNodePtr->alpha = baseNodePtr->currentEval;
                    baseNodePtr->bestMove = *(currMovePtrCopy);
                    baseNodePtr->bestEval = baseNodePtr->currentEval; //only used for TT updates.
                }
            }

            baseNodePtr->trueType = nodeType::PV; //can change for aspiration windows
            env.tt.updateTT(env, baseNodePtr);

            prinInfoLine(baseNodePtr, env, startTime);

        } while ((baseNodePtr->depth < maxDepth));

        std::cout << "bestmove " << env.board.moveToCoordinates(baseNodePtr->bestMove.move) << '\n';  
    }


    private:

        searchNodeStruct* setupBaseNodePtr(searchEnvStruct& env) {

            searchNodeStruct* baseNodePtr = &searchStack[0];

            baseNodePtr->depth = 0;
            baseNodePtr->ply = 0;
            baseNodePtr->baseIndexPtr = &env.moveGenerator.moveStack[0];
            baseNodePtr->currMovePtr = &env.moveGenerator.moveStack[0];
            baseNodePtr->bestMove = {0, 0}; //this is the best move found so far.
            baseNodePtr->historyBaseIndexPtr = &env.moveSorter.historyStack[0];
            baseNodePtr->historyCurrMovePtr  = &env.moveSorter.historyStack[0];

            return baseNodePtr;
        }

        searchNodeStruct* setupNode(searchNodeStruct* parentNodePtr) {

            searchNodeStruct* nodePtr = parentNodePtr + 1; //next frame in the stack

            nodePtr->alpha        = -parentNodePtr->beta;
            nodePtr->beta         = -parentNodePtr->alpha;
            
            nodePtr->baseIndexPtr = parentNodePtr->currMovePtr + 1;
            nodePtr->currMovePtr  = nodePtr->baseIndexPtr;
            
            nodePtr->historyBaseIndexPtr = parentNodePtr->historyCurrMovePtr;
            nodePtr->historyCurrMovePtr  = nodePtr->historyBaseIndexPtr;
       
            nodePtr->depth        = parentNodePtr->depth - depth::ply; //Does not work for reductions
            nodePtr->ply          = parentNodePtr->ply + 1;
            nodePtr->bestEval     = std::numeric_limits<int16_t>::min();
            nodePtr->trueType     = nodeType::ALL;

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
                std::cout << "this position is a three fold repetition\n";
                return true;
            }
            
            uint64_t nLegalMoves = (baseNodePtr->currMovePtr - baseNodePtr->baseIndexPtr);
            if (nLegalMoves == 0) {

                if (board.inCheck()) {
                    std::cout << ((board.whiteToMove) ? "black " : "white ") << "has won\n";
                } else {
                    std::cout << "this position is stalemate\n";
                }

                return true;
            }
            
            return false;
        }

        bool hardLimitReached(searchEnvStruct& env) {

            //only check the conditions every 4096 nodes for speed
            if ((env.nodes & 0xFFFULL) != 0ULL) [[likely]] {
                return false;
            }

            if ((env.endTime < std::chrono::high_resolution_clock::now()) || //out of time
                (env.nodes > env.maxNodes) /*node budget exceeded*/) [[unlikely]] {
    
                return true;
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

        bool repetitionReturn(searchEnvStruct& env, searchNodeStruct* nodePtr) {

            if (nodePtr->ply > 2) [[likely]] { 
                if ( env.board.isOneRepetition()  ) [[unlikley]] { return true; }
            
            } else {
                if ( env.board.isTwoRepetitions() ) [[unlikley]] { return true; }
            }

            return false;
        }

        bool ttEarlyReturn(searchEnvStruct& env, TTentryStruct* ttEntryPtr, searchNodeStruct* nodePtr) {

            if (ttEntryPtr == nullptr) { //no tt hit
                return false;
            }

            uint16_t ttEntrydepth      = ttEntryPtr->meta & uint16_t(0x3FFFU);
            uint16_t ttNodeType = ttEntryPtr->meta >> 14;

            if (ttEntrydepth < nodePtr->depth) { //inssufficient depth for a return
                return false;
            }
    
            int16_t correctedScore = env.tt.localToMateScore(nodePtr, ttEntryPtr->eval);
            switch (ttNodeType) {
                case nodeType::ALL: if ( correctedScore <= nodePtr->alpha) {nodePtr->bestEval = correctedScore; return true; } break;
                case nodeType::CUT: if ( correctedScore >= nodePtr->beta)  {nodePtr->bestEval = correctedScore; return true; } break;
                case nodeType::PV:                                         {nodePtr->bestEval = correctedScore; return true; } break;
                default: std::cerr << "incorrect node type in TT early return\n";                                              break;
            }

            return false;
        }

        void prinInfoLine(searchNodeStruct* baseNodePtr, searchEnvStruct& env, std::chrono::time_point<std::chrono::high_resolution_clock> startTime) {
            
            auto currentTime = std::chrono::high_resolution_clock::now();
            uint64_t currentSearchTime = std::max<uint64_t>(1, std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count());

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

};

#endif
