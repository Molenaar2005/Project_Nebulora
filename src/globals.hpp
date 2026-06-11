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

#ifndef GLOBALS_HPP
#define GLOBALS_HPP

#include<cstdint>
#include<chrono>



//########## forward declarations ############
class boardClass;
class moveGeneratorClass;
class moveOrderingClass;
class evaluationClass;
class transpositionTableClass;
//################################ ############

//precomputed tables
std::array<uint64_t, 64>                   seenByKing;
std::array<uint64_t, 64>                   seenByKnight;
std::array<std::array<uint64_t, 512>, 64>  seenByD12;
std::array<std::array<uint64_t, 4096>, 64> seenByHV;
std::array<uint64_t, 64>                   attackHV;
std::array<uint64_t, 64>                   attackD12;
std::array<uint64_t, 4096>                 fromToTable;
std::array<uint64_t, 858>                  zobristHashes;



namespace nodeType {
    enum types : int8_t {
        ALL = 0,
        CUT = 1,
        PV = 2,
        empty = 3, //used for TT distribution display.

        upperBound = 0,
        lowerBound = 1
    };  
};

namespace depth {
    enum depth : uint16_t {
        ply = 16, //how many units is a ply for fractional reductions
        maxPly = 128
    };
};

namespace hash {
    enum index : uint16_t {
        whiteToMoveHash      = 832,
        enPassantFileHash    = 833,
        CastlingHash  = 842
    };
};

namespace mateScores {
    enum matingValues : int16_t {
        mateThreshold = 30000,
        mateValue = 32750
    };
};

struct alignas(4) moveStruct {
    uint16_t move;
    int16_t value;
};

struct alignas(64) searchNodeStruct {

    moveStruct* currMovePtr;       // 8 8
    uint16_t* historyCurrMovePtr;  // 8 16
    uint64_t seenByOpponent;       // 8 24
    uint64_t unMakeInfo;           // 8 32

    uint16_t bestMove;             // 2 34
    uint16_t ttMove;               // 2 36
    int16_t currentEval;           // 2 38
    int16_t shiftMargin;           // 2 40
    int16_t staticEval;            // 2 42
    int16_t reduction;             // 2 44
    int16_t extension;             // 2 46
    int16_t bestEval;              // 2 48
    int16_t stash;                 // 2 50
    int16_t depth;                 // 2 52
    int16_t alpha;                 // 2 54
    int16_t beta;                  // 2 56
    
    uint8_t lockedSquare;          // 1 57
    uint8_t historyMovesN;         // 1 58
    uint8_t trueType;              // 1 59    
    uint8_t movesN;                // 1 60
    uint8_t ply;                   // 1 61
    bool TTIsCapture;              // 1 62
    bool inCheck;                  // 1 63
    bool TTHit;                    // 1 64
};


namespace constants {
    
        enum pieceTypes : uint8_t {
            whitePawn   = 0,
            whiteRook   = 1,
            whiteKnight = 2,
            whiteBishop = 3,
            whiteQueen  = 4,
            whiteKing   = 5,
            blackPawn   = 6,
            blackRook   = 7,
            blackKnight = 8,
            blackBishop = 9,
            blackQueen  = 10,
            blackKing   = 11,
            emptySquare = 12
        };    

        enum occupancy : uint8_t {
            white    = 0,
            black    = 1,
            combined = 2
        };    

        enum castlingTypes : uint8_t {
            whiteKingCastle  = 0b0001,
            whiteQueenCastle = 0b0010,
            blackKingCastle  = 0b0100,
            blackQueenCastle = 0b1000,
        };    

        enum boardmasks : uint64_t {
            aFile       = 0x0101010101010101ULL,
            hFile       = 0x8080808080808080ULL,
            eightRank   = 0xFF00000000000000ULL,
            seventhRank = 0x00FF000000000000ULL,
            firstRank   = 0x00000000000000FFULL,
            secondRank  = 0x000000000000FF00ULL,
            leftHalf    = 0x0f0f0f0f0f0f0f0fULL,
            rightHalf   = 0xf0f0f0f0f0f0f0f0ULL,
            allSquares  = 0xffffffffffffffffULL,
            e1Square    = 1ULL << 4,
            e8Square    = 1ULL << 60
        };

        enum remaining : uint8_t {
            Npieces  = 12,
            Nsquares = 64
        };

        enum searchWindowConstants : int16_t {
            inf = 32'750
        };

};

namespace packedBits {

        enum packedBits : uint64_t {
            oneBit = 0b1,
            twoBits = 0b11,
            threeBits = 0b111,
            fourBits = 0b1111,
            sixBits = 0b111111,
            sevenBits = 0b1111111,
            eightBits = 0xFF,
            fourteenBits = 0x3FFF
        };

};

namespace moveOrderingConstatns {

    enum orderingBiasses : int {
        ttMoveBias = 32750,
        winningCaptureBias = 32'700,
        losingCaptureBias = 65'000,
        firstKillerBias = 32'500,
        secondKillerBias = 32'250
    };

    enum packedWeights : uint64_t {
        packedMaterialValue = 0x0093351093351,
        packedPromotionValue = 0x0000000082240
    };
    
};



struct searchContextStruct {

    boardClass& board;
    moveGeneratorClass& moveGenerator;
    evaluationClass& evaluation;
    moveOrderingClass& moveSorter;
    transpositionTableClass& tt;
    int8_t contemptFactor = 0;

    uint64_t wtime;
    uint64_t btime;
    uint64_t wInc;
    uint64_t bInc = 0;
    uint64_t maxNodes;
    uint64_t maxDepth;
    uint64_t movetime;
    uint64_t movesToGo;

   searchContextStruct( boardClass& boardRef, moveGeneratorClass& moveGeneratorRef, evaluationClass& evaluationRef,
                        moveOrderingClass& moveSorterRef, transpositionTableClass& ttRef, const int8_t contemptFactorParam = 0)
                        
                        : //initializer list
                        board(boardRef), moveGenerator(moveGeneratorRef), evaluation(evaluationRef),
                        moveSorter(moveSorterRef), tt(ttRef), contemptFactor(contemptFactorParam) 
                        
        { //constructor
    
        //load in default values
        constexpr uint64_t inf = 100'000'000'000'000;

        wtime = inf;
        btime = inf;
        wInc = 0;
        bInc = 0;
        maxNodes = inf;
        maxDepth = inf;
        movetime = inf;
        movesToGo = 20;
   };

};

struct searchEnvStruct {
    boardClass& board;
    moveGeneratorClass& moveGenerator;
    moveOrderingClass& moveSorter;
    evaluationClass& evaluation;
    transpositionTableClass& tt;
    uint64_t nodes = 0;
    uint64_t maxNodes;
    std::chrono::time_point<std::chrono::high_resolution_clock> endTime;
    uint8_t selDepth = 0;
    int8_t contemptValue;

    searchEnvStruct(searchContextStruct& ctx, uint64_t searchTimeParameter) :
                    board(ctx.board),
                    moveGenerator(ctx.moveGenerator),
                    moveSorter(ctx.moveSorter),
                    evaluation(ctx.evaluation),
                    tt(ctx.tt),
                    endTime(std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(searchTimeParameter)),
                    maxNodes(ctx.maxNodes),
                    contemptValue(ctx.contemptFactor)
                    { }
};

struct TTentryStruct {
    uint64_t hash;
    uint16_t meta;  // format: (type << 14) | depth
    uint16_t move;
    int16_t eval;
    uint8_t generation;
    bool unUsed;
};





#endif