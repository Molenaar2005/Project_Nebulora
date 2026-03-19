
#ifndef EVALUATIONCLASS_HPP
#define EVALUATIONCLASS_HPP


#include<array>

#include "boardClass.hpp"
#include "evalWeights.hpp"

namespace evaluationOffsets {
  enum sections : uint16_t {
    phase = 0,
    pst_mg = 6,
    pst_eg = 7
  };
  
}


class evaluationClass {

    public:

        struct evaluationStruct {
            int16_t mg = 0;
            int16_t eg = 0;
        };


        template<bool whitePerspective>
        int16_t static_evaluation(boardClass& board){
          using namespace evaluationOffsets;


            int16_t phase = 0;
            { //calculate the current phase
                for (int pieceType = 0; pieceType < 6; pieceType++) { //loop over all six piecetypes
    
                    //merge both colors
                    uint64_t bothColors = board.bitboard[pieceType] | board.bitboard[pieceType + 6];
                    
                    phase += evalWeights[pieceType + sections::phase] * std::popcount(bothColors);
                }
            }

            evaluationStruct score;

            constexpr bool white = true;
            constexpr bool black = false;

            pieceSquareTables<white>(score, board);
            pieceSquareTables<black>(score, board);

            //tempo bonus
            int16_t signToMove = board.whiteToMove ? 1 : -1;
            score.mg += signToMove * 16;
            score.eg += signToMove * 0;


            //interpolate to get the final score
            const int16_t maxPhase = calculateMaxPhase();
            phase = std::min(phase, maxPhase); //clip in case of early promotions

            int16_t whiteScore = (score.mg * phase  + score.eg * (maxPhase - phase)) / maxPhase;

            if constexpr (whitePerspective) {
              return whiteScore;
            } else {
              //search requires the evaluation to be from the perspective of the side to move.
              return signToMove * whiteScore;
            }
        }


    private:

        template<bool whiteSide>
        void pieceSquareTables(evaluationStruct& score, boardClass& board) {
          using namespace evaluationOffsets;

            constexpr int16_t blackSide = 6 * !whiteSide;

            //evaluate all the pieces and the square they occupy
            for (int pieceType = 0; pieceType < 6; pieceType++) {

                uint64_t pieces = board.bitboard[pieceType + blackSide];
                while (pieces != 0) {
                    int squareIndex = std::countr_zero(pieces);
                    pieces ^= 1ULL << squareIndex;

                    if constexpr (!whiteSide) {
                      squareIndex ^= 56;
                    }

                    //update the score
                    int16_t sign = whiteSide ? 1 : -1;
                    score.mg += sign * evalWeights[128 * pieceType + 2 * squareIndex + sections::pst_mg];
                    score.eg += sign * evalWeights[128 * pieceType + 2 * squareIndex + sections::pst_eg];
                }
            }
        }

        constexpr int16_t calculateMaxPhase() {
          using namespace constants;
          return evalWeights[0] * 16 //pawns
               + evalWeights[1] * 4  //rooks
               + evalWeights[2] * 4  //knighs
               + evalWeights[3] * 4  //bishops
               + evalWeights[4] * 2  //queens
               + evalWeights[5] * 2; //kings

        }

};


#endif