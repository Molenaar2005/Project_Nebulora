#include<iostream>
//#include<bitset>
#include<string>
#include<sstream>
#include<array>
#include<fstream>
//#include<vector>
//#include<cmath>
//#include<random>
#include<fstream>
#include<bit>
#include<chrono>
#include<random>
#include<format> //debugging

#include "globals.hpp"
#include "boardClass.hpp"
#include "moveGeneratorClass.hpp"
#include "evaluationClass.hpp"

#include "moveOrderingClass.hpp"
#include "transpositionTableClass.hpp"
#include "searchClass.hpp"


void initialize(){

    #include "generateLookupTables.ipp"

    //initialize all zobrist hashes.
    std::mt19937_64 rng(0);
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
    for (uint64_t& i : zobristHashes) {i = dist(rng);}

    for (int squareIndex = 0; squareIndex < 64; squareIndex++) {
      zobristHashes[squareIndex * 13 + constants::emptySquare] = 0;
    }
    
    
    zobristHashes[hash::enPassantFileHash + 8] = 0; //the 8th file does not exist and correspond to no possible enpassant capture
    
    
    for (uint8_t castlingRights = 0; castlingRights < 16; castlingRights++) {
      if (std::popcount(castlingRights) == 1) {
        continue;
      }

      uint8_t copyCastlinRights = castlingRights;
      zobristHashes[hash::CastlingHash + castlingRights] = 0;
      while (copyCastlinRights != 0) {
        uint8_t lsb = copyCastlinRights & -copyCastlinRights;
        copyCastlinRights ^= lsb;
        zobristHashes[hash::CastlingHash + castlingRights] ^= zobristHashes[hash::CastlingHash + lsb];
      }
    }
}

int main(){

    //startup
    initialize();

    boardClass board;
    moveGeneratorClass moveGenerator;
    evaluationClass evaluation;
    moveOrderingClass moveSorter;
    searchClass search;
    transpositionTableClass tt;

    int8_t contemptFactor = 0;



    //handling commandline inputs
    std::string command;
    std::string nextToken;
    while (getline(std::cin, command)) {
        std::istringstream ss(command);
        ss >> nextToken;

        if (nextToken == "go") {
        ss >> nextToken;

            if (nextToken == "perft") {
            ss >> nextToken;
            uint8_t maxDepth = std::stoi(nextToken); //doesn't handle exceptions properly

            uint64_t leafNodes = moveGenerator.perftRoot(board, maxDepth);
            continue;        
        }

        //gather all relevant termination criteria and start the search.
        searchContextStruct searchContext( board, moveGenerator, evaluation, moveSorter, tt, contemptFactor );

        do {

            /* does not yet support go mate, certainmoves*/
            
            if (nextToken == "nodes")     { ss >> searchContext.maxNodes; } else
            if (nextToken == "binc")      { ss >> searchContext.bInc;     } else
            if (nextToken == "movestogo") { ss >> searchContext.movesToGo;} else
            if (nextToken == "winc")      { ss >> searchContext.wInc;     } else
            if (nextToken == "btime")     { ss >> searchContext.btime;    } else
            if (nextToken == "wtime")     { ss >> searchContext.wtime;    } else
            if (nextToken == "depth")     { ss >> searchContext.maxDepth; } else
            if (nextToken == "movetime")  { ss >> searchContext.movetime; }
        } while (ss >> nextToken );

        search.rootSearch(searchContext);
        } else

        if (nextToken == "bench") {

            if ((ss >> nextToken) && (nextToken == "perft")) {
                
                std::array<std::string, 6> testPositions = {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                                                            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                                                            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
                                                            "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
                                                            "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
                                                            "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"};
                std::array<int, 6> depths = {7, 6, 8, 6, 5, 6};
                std::array<uint64_t, 6> expectedOutcome = {3'195'901'860, 8'031'647'685, 3'009'794'393, 706'045'033, 89'941'194, 6'923'051'137};
                std::array<uint64_t, 6> speed;
                std::array<bool, 6> passed;
    
                for (int iTest = 0; iTest < 6; iTest++) {
        
                    board.setupFEN(testPositions[iTest]);

                    auto startTime = std::chrono::high_resolution_clock::now();
                    uint64_t leafNodes = moveGenerator.perftRoot(board, depths[iTest]);
                    auto endTime = std::chrono::high_resolution_clock::now();
                    uint64_t runTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
        
                    passed[iTest] = (leafNodes == expectedOutcome[iTest]);
                    speed[iTest] = leafNodes / runTime;
                }
    
                for (int iTest = 0; iTest < 6; iTest++) {
                    std::cout << "nodes per second: " << speed[iTest] << (passed[iTest] ? " passed" : " failed") << '\n';   
                }

                continue;
            }

            std::array<std::string, 6> testPositions = {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                                                        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                                                        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
                                                        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
                                                        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
                                                        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"};

            for (std::string testFen : testPositions) {

                board.setupFEN(testFen);
                moveSorter.resetState();
                std::cout << "testing fen: " << testFen << '\n';

                for (int plyPlayed = 0; plyPlayed < 6; plyPlayed++) {

                searchContextStruct searchContext( board, moveGenerator, evaluation, moveSorter, tt, contemptFactor );
                searchContext.maxDepth = 10; //search limit
                search.rootSearch(searchContext);
                
                uint16_t rawMove = search.searchStack[0].bestMove;
                uint64_t unMakeInfo = board.makeMove<false, false>(rawMove);
                }
            }
        } else

        if (nextToken == "position") {
            ss >> nextToken;

            if (nextToken == "startpos") {
                board.setupStartingPosition();
            } else

            if (nextToken == "fen") {
                std::string fen; //a fenstring contains 6 fields so 6 fields need to be extracted from the stringstream
                for (int fenField = 0; fenField < 6; fenField++) { ss >> nextToken; fen += nextToken + " "; }
                board.setupFEN(fen);
            }

            //are there moves made from that position?
            if (ss >> nextToken && nextToken == "moves") {
                while (ss >> nextToken) {
                    uint16_t move = board.coordinatesToMove(nextToken);
                    board.makeMove<false, false>(move);
                }
            }
        } else

        if (nextToken == "uci") {
            std::cout << "id name Nebulora\n";
            std::cout << "id author Pluk2005\n";
            std::cout << '\n';

            //options
            std::cout << "option name Hash type spin default 128 min 128 max 274877\n";
            std::cout << "option name Contempt type spin default 0 min -128 max 127\n";

            std::cout << "uciok\n";
        } else

        if (nextToken == "isready"){
            std::cout << "readyok\n";
        } else

        if (nextToken == "ucinewgame"){
            tt.resetTT();
            moveSorter.resetState();
        } else

        if (nextToken == "setoption") { //could use some denesting.
            ss >> nextToken;
        
            if (nextToken == "name") {
                ss >> nextToken;

                if (nextToken == "hash") {
                ss >> nextToken;

                    if (nextToken == "value") {
                        ss >> nextToken;
                        tt.resizeTT(std::stoi(nextToken));
                    }
                }

                if (nextToken == "contempt") {
                    ss >> nextToken;
                    if (nextToken == "value") {
                        ss >> nextToken;
                        contemptFactor = std::clamp<int8_t>(std::stoi(nextToken), -128, 127);
                    }
                }
            }
        } else


        if (nextToken == "eval") {
            if (board.inCheck()) {
                std::cout << "this position is in check\n";
            } else {
                std::cout << "static_eval: " << evaluation.static_evaluation<true>(board) << '\n';
            }
        } else 

        if (nextToken == "d") {
            board.visualizeBoard();
        } else

        if (nextToken == "quit") {
            break;
        } else

        {std::cout << "unrecognized command\n";}

    }

    return 0;
}
