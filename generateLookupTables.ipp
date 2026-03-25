#ifndef GENERATELOOKUPTABLES_HPP
#define GENERATELOOKUPTABLES_HPP

#include<array>
#include<chrono> //for setup time testing
#include<immintrin.h>
#include<bit>

extern std::array<uint64_t, 4096>                 fromToTable;
extern std::array<uint64_t, 64>                   seenByKing;
extern std::array<uint64_t, 64>                   seenByKnight;
extern std::array<std::array<uint64_t, 512>, 64>  seenByD12;
extern std::array<std::array<uint64_t, 4096>, 64> seenByHV;
extern std::array<uint64_t, 64>                   attackHV;
extern std::array<uint64_t, 64>                   attackD12;

auto bitShift = [](uint64_t unShifted, int shiftValue) -> uint64_t {
    if (shiftValue >= 0) {return unShifted << shiftValue;}
    else {return unShifted >> -shiftValue;}
};


struct toEdgeData {

    std::array<uint64_t, 8> toEdge;

    toEdgeData(int startingIndex) {
        //calculate the distance to the edge
        toEdge[3] = startingIndex % 8;        
        toEdge[2] = (startingIndex - toEdge[3]) / 8;    
        toEdge[1] = 7 - toEdge[3];    
        toEdge[0] = 7 - toEdge[2]; 
        toEdge[4] = std::min(toEdge[0], toEdge[1]);        
        toEdge[5] = std::min(toEdge[1], toEdge[2]);        
        toEdge[6] = std::min(toEdge[2], toEdge[3]);
        toEdge[7] = std::min(toEdge[3], toEdge[0]);
    }
};


{ //fromToTable

    std::array<int, 8> directions = {8, 1, -8, -1, 9, -7, -9, 7};

    //loop over all the startingsquares
    for (int startingIndex = 0; startingIndex < 64; startingIndex++) {

        //calculate the distance to the edge
        toEdgeData data(startingIndex);

        //loop over all directions
        for (int direction = 0; direction < 8; direction++) {

            uint64_t bitboard = 0;

            //to the edge of the board
            for (int steps = 1; steps <= data.toEdge[direction]; steps++) {
                uint64_t targetIndex = startingIndex + directions[direction] * steps;
                
                bitboard |= 1ULL << targetIndex;

                fromToTable[startingIndex + 64 * targetIndex] = bitboard;
            }
        }
    }
}

{ //attackHV

    int directions[4] = {8, 1, -8, -1};

    //loop over all the squares
    for (int startingIndex = 0; startingIndex < 64; startingIndex++) {

        toEdgeData data(startingIndex);
        attackHV[startingIndex] = 0ULL;
        
        //create empty board mask
        uint64_t startingSquare = 1ULL << startingIndex;

        //loop over all directions
        for (int direction = 0; direction < 4; direction++) {
            for (int steps = 1; steps < data.toEdge[direction]; steps++) {

                attackHV[startingIndex] |= bitShift(startingSquare, directions[direction] * steps);
            }

        }

    }
}

{ //attackD12

    int directions[4] = {9, -7, -9, 7};

    //loop over all the squares
    for (int startingIndex = 0; startingIndex < 64; startingIndex++) {

        toEdgeData data(startingIndex);
        attackD12[startingIndex] = 0ULL;

        //loop over all directions
        for (int direction = 0; direction < 4; direction++) {
            for (int steps = 1; steps < data.toEdge[direction + 4]; steps++) {
                
                uint64_t startingSquare = 1ULL << startingIndex;
                attackD12[startingIndex] |= bitShift(startingSquare, directions[direction] * steps);
            }
        }
    }
}

{ //seenByKing

    //bunch of constants that describe how a king moves
    int kingMove[8] = { 8,  1,  -8,  -1,  9,  -7, -9,  7 };
    
    //loop over all the squares
    for (int startingIndex = 0; startingIndex < 64; startingIndex++) {

        toEdgeData data(startingIndex);
        seenByKing[startingIndex] = 0ULL;

        //check if the move is valid
        for (int direction = 0; direction < 8; direction++) {
            
            //check if the target square is on the board
            if (data.toEdge[direction] > 0) {

                //add to the list
                uint64_t startingSquare = 1ULL << startingIndex;
                seenByKing[startingIndex] |= bitShift(startingSquare, kingMove[direction]);
            }
        }
    }
}

{ //seenByKnight

    //bunch of constants that describe how a knight moves
    int knightMove[8]         = {17, 10, -6, -15, -17, -10, 6,  15 };
    uint64_t toNorthKnight[8] = { 2,  1,  0,   0,   0,   0,  1,  2 };
    uint64_t toEastKnight[8]  = { 1,  2,  2,   1,   0,   0,  0,  0 };
    uint64_t toSouthKnight[8] = { 0,  0,  1,   2,   2,   1,  0,  0 };
    uint64_t toWestKnight[8]  = { 0,  0,  0,   0,   1,   2,  2,  1 }; 

    //loop over all the squares
    for (int startingIndex = 0; startingIndex < 64; startingIndex++) {

        toEdgeData data(startingIndex);
        seenByKnight[startingIndex] = 0ULL;

        //check if the move is valid
        uint64_t startingSquare = 1ULL << startingIndex;
        for (int move = 0; move < 8; move++) {

            //check if the target square is on the board
            if (data.toEdge[0] >= toNorthKnight[move] &&
                data.toEdge[1] >= toEastKnight[move] &&
                data.toEdge[2] >= toSouthKnight[move] &&
                data.toEdge[3] >= toWestKnight[move]) {

                    //add to the list
                    seenByKnight[startingIndex] |= bitShift(startingSquare, knightMove[move]);
                }
        }
    }
}

{ //seenByD12

    int directions[4] = {9, -7, -9, 7};
   
    //loop over all the squares
    for (int startingIndex = 0; startingIndex < 64; startingIndex++) {
        uint64_t startingSquare = 1ULL << startingIndex;

        toEdgeData data(startingIndex);

        //loop over all the possible attack boards for that square
        int attackCombinations = 1ULL << std::popcount(attackD12[startingIndex]);

        for (uint64_t attackBoard = 0; attackBoard < attackCombinations; attackBoard++) {

            //setup the arrangment of pieces on the board
            uint64_t combined = _pdep_u64(attackBoard, attackD12[startingIndex]);
            seenByD12[startingIndex][attackBoard] = 0ULL;

            //calculate the possible moves
            //loop over all directions
            for (int direction = 0; direction < 4; direction++) {

                //loop to the edge of the board
                for (int steps = 1; steps <= data.toEdge[direction + 4]; steps++) {

                    //append to the seenSquares
                    uint64_t targetSquare = bitShift(startingSquare, directions[direction] * steps);
                    seenByD12[startingIndex][attackBoard] |= targetSquare;

                    //is that square empty
                    if ((combined & targetSquare) != 0) {
                        break;
                    }
                }
            }
        }
    }
}

{ //seenByHV

    int directions[4] = {8, 1, -8, -1};
   
    //loop over all the squares
    for (int startingIndex = 0; startingIndex < 64; startingIndex++) {

        uint64_t startingSquare = 1ULL << startingIndex;

        toEdgeData data(startingIndex);
        
        //loop over all the possible attack boards for that square
        uint64_t bitCombinations = 1ULL << (std::popcount(attackHV[startingIndex]));

        for (uint64_t attackBoard = 0; attackBoard < bitCombinations; attackBoard++) {

            //setup the arrangment of pieces on the board
            uint64_t combined = _pdep_u64(attackBoard, attackHV[startingIndex]);
            seenByHV[startingIndex][attackBoard] = 0ULL;

            //calculate the possible moves
            //loop over all directions
            for (int direction = 0; direction < 4; direction++) {

                //loop to the edge of the board
                for (int steps = 1; steps <= data.toEdge[direction]; steps++) {

                    //append to the seenSquares
                    uint64_t targetSquare = bitShift(startingSquare, directions[direction] * steps);
                    seenByHV[startingIndex][attackBoard] |= targetSquare;

                    //is that square empty
                    if ((combined & targetSquare) != 0) {
                        break;
                    }
                }
            }
        }
    }
}

#endif //GENERATELOOKUPTABLES_HPP