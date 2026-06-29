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

#ifndef EVALUATIONCLASS_HPP
#define EVALUATIONCLASS_HPP


#include<array>

#include "boardClass.hpp"
#include "evalWeights.hpp"


class evaluationClass {

    public:

        template<bool whitePerspective>
        int16_t static_evaluation(boardClass& board){

            uint64_t phase = 0;

            { //calculate the current phase
                for (uint64_t pieceType = 0; pieceType < 6; pieceType++) { //loop over all six piecetypes

                    //merge both colors
                    uint64_t bothColors = board.bitboard[pieceType] | board.bitboard[pieceType + 6];

                    phase += packedEvalWeights[pieceType] * std::popcount(bothColors);
                }
            }


            constexpr bool white = true;
            constexpr bool black = false;
            uint64_t packedScoreWhite = 0;
            uint64_t packedScoreBlack = 0;

            pieceSquareTables<white>(packedScoreWhite, board);
            pieceSquareTables<black>(packedScoreBlack, board);



            //tempo bonus
            //packed at compile time
            constexpr uint64_t mg_TempoBonus = uint16_t( 16);
            constexpr uint64_t eg_TempoBonus = uint16_t(  0);
            constexpr uint64_t tempoBonus = (eg_TempoBonus << 32) | mg_TempoBonus;

            packedScoreWhite += tempoBonus * board.whiteToMove;
            packedScoreBlack += tempoBonus * !board.whiteToMove;


            //extract and interpolate the final score
            //the intermediate casting is used to preserve signs in the compressed layout.
            const uint64_t maxPhase = calculateMaxPhase();
            phase = std::min(phase, maxPhase); //clip in case of early promotions

            int scoreWhite_mg = int16_t(packedScoreWhite);
            int scoreWhite_eg = int16_t(packedScoreWhite >> 32);
            int scoreBlack_mg = int16_t(packedScoreBlack);
            int scoreBlack_eg = int16_t(packedScoreBlack >> 32);

            int score_mg = scoreWhite_mg - scoreBlack_mg;
            int score_eg = scoreWhite_eg - scoreBlack_eg;


            int16_t whiteScore = (score_mg * int(phase)  + score_eg * int(maxPhase - phase)) / int(maxPhase);

            if constexpr (whitePerspective) {
              return whiteScore;
            } else {
              //search requires the evaluation to be from the perspective of the side to move.
              return board.whiteToMove ? whiteScore : -whiteScore;
            }
        }


    private:

        template<bool whiteSide>
        void pieceSquareTables(uint64_t& packedScore, boardClass& board) {

            //evaluate all the pieces and the square they occupy
            for (uint64_t pieceType = 0; pieceType < 6; pieceType++) {

                constexpr uint64_t blackBias = 6 * !whiteSide;
                uint64_t pieces = board.bitboard[pieceType + blackBias];

                const uint64_t* pieceWeightPtr = &packedEvalWeights[64 * pieceType + 6];

                while (pieces != 0) {
                    uint64_t squareIndex = std::countr_zero(pieces);
                    pieces ^= 1ULL << squareIndex;

                    if constexpr (!whiteSide) {
                      squareIndex ^= 56;
                    }

                    packedScore += pieceWeightPtr[squareIndex];
                }
            }
        }

        constexpr uint64_t calculateMaxPhase() {
            using namespace constants;
            return packedEvalWeights[0] * 16 //pawns
                 + packedEvalWeights[1] * 4  //rooks
                 + packedEvalWeights[2] * 4  //knighs
                 + packedEvalWeights[3] * 4  //bishops
                 + packedEvalWeights[4] * 2  //queens
                 + packedEvalWeights[5] * 2; //kings
        }

};


#endif
