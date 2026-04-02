#ifndef TRANSPOSITIONTABLECLASS_HPP
#define TRANSPOSITIONTABLECLASS_HPP


#include<bitset>
#include<vector>
#include<array>
#include<limits>
#include<intrin.h> //for prefetching the transposition table

#include "globals.hpp"
#include "boardClass.hpp"





class transpositionTableClass {
    //variables for the transposition table
    //the buckets are alligned so they fit a 64 byte cacheline. However this is not guaranteed to be preserved by the allocator in std::vector.
    //In order to guarantee this a costum allocater would be necessary. However from runtime testing MSVC seems to preserve this in this case.


    public:

        uint8_t currentGeneration = 0;
        uint64_t ttBuckets = 0;
        struct alignas(64) ttBucket {std::array<TTentryStruct, 4> entry;};
        std::vector<ttBucket> TT;

        std::array<uint64_t, 4> distribution = {0, 0, 0, 0};

        inline const void* ttBucketPtr(const uint64_t hash) const {
            return &TT[(uint64_t(uint32_t(hash >> 32ULL)) * ttBuckets) >> 32];
        }

        transpositionTableClass() {
            uint64_t defaultTTSize = 128; //Mb
            resizeTT(defaultTTSize);
        }

        void newGeneration() {
            currentGeneration++;
        }    

        void updateTT(searchEnvStruct& env, searchNodeStruct* nodePtr){
            /* The transposition table is a design with buckets meaning every index (derived from the position hash) contains four entries. Multiple entries allows for more control
            over which entries are kept or rejected and four is the maximum that would fit in a cacheline of 64 bytes. When the transposition table is updated with a new entry
            it is compared and the least usefull (could be the new entry) is rejected. This is done by ranking them based on desired statistics. First generation, this
            keeps the entries relevant. Next based on depth since higher depths can save more searchtime. Next based on node type exact > lower > upper. Next based on eval.
            
            Important things to note:
            -keep old or new entry but don't keep multiple entries of the same hash.
            -prefer filling an empty slot over replacing another (unless it would duplicate this hash)
            */
           
            // NOTE: Can be heavily optimized with SIMD.
            constexpr int64_t doubleBias = -10'000'000'000'000;
            constexpr int64_t emptyBias  = -1'000'000'000'000;
           
            TTentryStruct newEntry = assembleNewEntry(env, nodePtr);

            //current entry
            uint64_t currentBucketIndex = (uint64_t(uint32_t(newEntry.hash >> 32ULL)) * ttBuckets) >> 32;
            TTentryStruct* ttEntry = &TT[currentBucketIndex].entry[0];
            int64_t newEntryScore = ttEntryScore(newEntry);
            
            
            //find the least valuable entry in the bucket
            uint8_t lowestValueIndex = 5; //fifth element does not exist
            int64_t lowestScore = std::numeric_limits<int64_t>::max();
            for (uint8_t i = 0; i < 4; i++) { //this loop can most likely be vectorized with SIMD
                
                int64_t currentEntryScore = ttEntryScore(ttEntry[i]);
                currentEntryScore        -= -doubleBias * (ttEntry[i].hash == newEntry.hash);
                currentEntryScore        -= -emptyBias  * (ttEntry[i].hash == 0ULL);
                
                bool scoreDecreased = lowestScore > currentEntryScore;
                lowestValueIndex    = scoreDecreased ? i : lowestValueIndex;
                lowestScore         = scoreDecreased ? currentEntryScore : lowestScore;
            }
            

            //replace if the new entry is considered more valuable then the current entry
            newEntryScore  -= -doubleBias * (ttEntry[lowestValueIndex].hash == newEntry.hash);
            newEntryScore  -= -emptyBias  * (ttEntry[lowestValueIndex].hash == 0ULL);

            //branchless write back SIMD would be prefered since it removes a static variable.
            //and this commented implementation does not track node distribution correctly.
            /*
            static thread_local TTentryStruct sink;
            TTentryStruct* newEntryDestinationPtr = ( newEntryScore > lowestScore ) ? &ttEntry[lowestValueIndex] : &sink;
            replaceEntry(*newEntryDestinationPtr, newEntry);
            */

            if ( newEntryScore > lowestScore ) {

                replaceEntry(ttEntry[lowestValueIndex], newEntry);
            }
        }

        TTentryStruct* probeTT(searchEnvStruct& env, searchNodeStruct* nodePtr){

            //TT scores are not reliable when close to the 50 move draw rule.
            uint64_t requestedHash = env.board.positionHash;
            if ((env.board.fiftyMoveClockPly > 80) | (requestedHash == 0)) { return nullptr;}
            
            //find out what bucket it is located in
            uint64_t currentBucketIndex = (uint64_t(uint32_t(requestedHash >> 32ULL)) * ttBuckets) >> 32;
            TTentryStruct* currentEntry = &TT[currentBucketIndex].entry[0];
            
            //loop over all the entries to find a matching hash
            uintptr_t returnPtr = reinterpret_cast<uintptr_t>(nullptr);
            for (uint8_t i = 0; i < 4; i++) {

                bool matchFound = currentEntry[i].hash == requestedHash;

                /* using ternary writes like this can hurt multithreading since every instance causes
                the memory adress to be marked as M under the MESI memory protocol. It doesn't hurt
                now for single threaded performance.
                */
                currentEntry[i].generation = matchFound ? currentGeneration : currentEntry[i].generation; 
                returnPtr |= reinterpret_cast<uintptr_t>(matchFound ? &currentEntry[i] : nullptr);
            }
            
            return reinterpret_cast<TTentryStruct*>(returnPtr);
            
        }

        void resizeTT(uint64_t TTSizeMb) {

            //lower limit for hash size is for search stability
            ttBuckets = std::max(TTSizeMb, 128ULL) * 1'000'000 / sizeof(ttBucket);

            //upper limit due to multiplicative hashing limits
            ttBuckets = std::min(ttBuckets,  1ULL << 32);

            //setup the hash table
            distribution = {0, 0, 0, ttBuckets * 4};
            TT.clear();
            TT.resize(ttBuckets, ttBucket{});
        }

        void resetTT() {
            distribution = {0, 0, 0, ttBuckets * 4};
            std::fill(TT.begin(), TT.end(), ttBucket{});
        }
 
        int16_t localToMateScore(searchNodeStruct* nodePtr, int16_t eval) {

            int16_t isPositiveMate = eval > mateScores::mateThreshold;
            int16_t isNegativeMate = eval < -mateScores::mateThreshold;

            return eval - (isPositiveMate * int16_t(nodePtr->ply)) + (isNegativeMate * int16_t(nodePtr->ply));
        }

        int16_t mateScoreToLocal(searchNodeStruct* nodePtr, int16_t eval) {

            bool isMateScore = std::abs(eval) > mateScores::mateThreshold;
            int16_t isPositiveMate = eval > mateScores::mateThreshold;
            int16_t isNegativeMate = eval < -mateScores::mateThreshold;

            return eval + (isPositiveMate * int16_t(nodePtr->ply)) - (isNegativeMate * int16_t(nodePtr->ply));
        }

        std::string pvLine(boardClass& board, int8_t depth, bool isFirstMove = true){   

            uint16_t ttMove = probeTTMove(board);

            if (ttMove != 0 && depth > 0) { //check if non empty return
                uint64_t unMakeInfo = board.makeMove<false, false>(ttMove);
                std::string restOfLine = pvLine(board, depth - 1, false);
                board.unMakeMove(unMakeInfo, ttMove);
                return (isFirstMove ? "" : " ") + board.moveToCoordinates(ttMove) + restOfLine;
            }
            
            return "";
        }

        uint16_t probeTTMove(boardClass& board){
            
            //find out what bucket it is located in
            uint64_t requestedHash = board.positionHash;
            uint64_t currentBucketIndex = (uint64_t(uint32_t(requestedHash >> 32ULL)) * ttBuckets) >> 32;
            TTentryStruct* entry = &TT[currentBucketIndex].entry[0];
            
            //loop over all the entries to find a matching hash
            uint16_t returnMove = 0;
            for (uint8_t i = 0; i < 4; i++) {

                uint16_t isHashMatch = requestedHash == entry[i].hash;
                returnMove |= isHashMatch * entry[i].move;    
            }
            
            return returnMove;            
        }

    private:
                
        int64_t ttEntryScore(TTentryStruct entry){
            /* The transposition table is ordered in buckets of four entries. In order to determine which one to keep a score is made classifying how usefull
            every entry is. This is done by the following criteria:
            1. A newer entry is likely more relavant and takes priority over an older one
            NOTE: I treat the current generation and one ago as the same age because it might still be used this search.
            This prevents a higher depth node that is used later from being replaced by a shallow newer one that is used early.
            2. Entries that are searched to a higher depth allow for more savings when a TT-hit occurs
            3. PV-nodes allow for more saving since the stored evaluation is exact and not a bound. This is so usefull that they are prefered over CUT and ALL nodes
            when the depth reduction is no more then 1. CUT nodes are prefered over ALL nodes since they often allow for more time savings.
            4. Lastly the entry with the higher eval is kept over a lower eval because the engine tries to maximize it's score and will try to reach higher evaluations.
            ALL nodes are an exception to this rule. An all node store a maximum score where the true score can be lower. As a result of this lower evals are prefered
            because they allow for more pruning.
            */
           
           uint32_t age = (currentGeneration - entry.generation) & 0xFF;
           
           uint64_t unUsedGenerations = std::max<uint64_t>(1, age);
           uint64_t type              = uint64_t(entry.meta >> 14) & 0b11ULL;
           uint64_t depth             = uint64_t(entry.meta & 0x3FFF) + depth::ply * (type == nodeType::PV);
           int64_t eval               = (type == nodeType::ALL) ? -entry.eval : entry.eval;
           
           return -int64_t(unUsedGenerations << 32) + int64_t(depth << 18 ) + int64_t(type << 16 ) + eval;
        }
        
        TTentryStruct assembleNewEntry(searchEnvStruct& env, searchNodeStruct* nodePtr) {
            TTentryStruct assembledEntry;

            assembledEntry.hash       = env.board.positionHash;
            assembledEntry.meta       = uint16_t(nodePtr->depth) | (uint16_t(nodePtr->trueType) << 14);
            assembledEntry.move       = nodePtr->bestMove;
            assembledEntry.eval       = mateScoreToLocal(nodePtr, nodePtr->bestEval);
            assembledEntry.generation = currentGeneration;
            assembledEntry.unUsed     = 0; //unused entry

            return assembledEntry;
        }

        void replaceEntry(TTentryStruct& currentEntry, TTentryStruct& newEntry) {
            
            uint64_t oldType = currentEntry.meta >> 14;
            uint64_t newType = newEntry.meta >> 14;

            oldType = (currentEntry.hash == 0) ? 3 : oldType;
            distribution[oldType]--;
            distribution[newType]++;

            currentEntry = newEntry;
        }    

};
    



/*defenition of make move to prevent circulair dependency*/

template<bool prefetchTT, bool addToHistoryStack>
inline uint64_t boardClass::makeMove(const uint16_t& move, searchEnvStruct* envPtr, searchNodeStruct* nodePtr){
    using namespace constants;

    //add the current hash to the repetitionHashList and advance the pointer
    *(repetitionListPtr++) = positionHash;

    //undo any possible enpassantFileHash
    positionHash ^= zobristHashes[hash::enPassantFileHash + std::countr_zero(uint32_t(enpassantFiles) | 0x100U)];


    uint8_t startCastlingFlags = castlingFlags;
    
    //unpack the move
    int64_t startingIndex = move & 0b111111ULL;
    int64_t targetIndex = (move >> 6) & 0b111111ULL;
    uint64_t promotion = (move >> 12) & 0b111ULL;

    uint64_t startingBitBoard = 1ULL << startingIndex;
    uint64_t targetBitBoard = 1ULL << targetIndex;

    uint64_t startPieceType = pieceAt[startingIndex];
    uint64_t targetPieceType = pieceAt[targetIndex];


    //generate an approximation of the new hash quickly in order to prefetch the TT.
    //this hash is not correct for castling and enpasant.
    positionHash ^= zobristHashes[startingIndex * 13 + startPieceType];
    positionHash ^= zobristHashes[targetIndex * 13 + startPieceType + promotion];
    positionHash ^= zobristHashes[targetIndex * 13 + targetPieceType];
    positionHash ^= zobristHashes[hash::whiteToMoveHash];
    if constexpr (prefetchTT) {
        if (envPtr == nullptr) [[unlikely]] {
            std::cerr << "nullptr for envPtr is passed to makeMove" << std::endl;
        } else {
            _mm_prefetch(static_cast<const char*>(envPtr->tt.ttBucketPtr(positionHash)), _MM_HINT_T0);
        }
    }

    //assemble the info needed to unmake this move
    uint64_t unmakeInfo = (castlingFlags)
                        | (enpassantFiles << 4)
                        | (fiftyMoveClockPly << 12)
                        | (targetPieceType << 19);



    //update the board
    uint8_t isCaptureMove = targetPieceType != emptySquare; //this does not trigger for enpassant but since that moves a pawn the fiftyMoveClock still gets reset.
    uint8_t pawnMove = (startPieceType == whitePawn) | (startPieceType == blackPawn);
    fiftyMoveClockPly = ((isCaptureMove | pawnMove) != 0) ? 0 : fiftyMoveClockPly + 1;
    moveClockPly += 1;
    enpassantFiles = 0;
    
    //clear the startingSquare
    bitboard[startPieceType] ^= startingBitBoard;
    pieceAt[startingIndex] = emptySquare;
    occupied[whiteToMove ? white : black] ^= startingBitBoard;
    occupied[combined] ^= startingBitBoard;

    //clear the targetSquare
    if (isCaptureMove) {  //enpassant is handled seperately
        bitboard[targetPieceType] ^= targetBitBoard;
        occupied[whiteToMove ? black : white] ^= targetBitBoard;
        occupied[combined] ^= targetBitBoard;
    }

    //add the piece to the targetSquare
    bitboard[startPieceType + promotion] |= targetBitBoard;
    pieceAt[targetIndex] = startPieceType + promotion;
    occupied[whiteToMove ? white : black] ^= targetBitBoard;
    occupied[combined] ^= targetBitBoard;

    if constexpr (addToHistoryStack) {

        if (nodePtr == nullptr) [[unlikely]] {
            std::cerr << "nullptr for nodePtr is passed to makeMove" << std::endl;
        } else {

            //repack and write to the history stack
            //targetIndex (6)  | startingType (4)
            *(nodePtr->historyCurrMovePtr) = uint16_t((targetIndex << 4) | startPieceType);
    
            //if this move is a capture then it shouldn't be saved
            uint16_t isNonCapture = targetPieceType == emptySquare;
            nodePtr->historyCurrMovePtr += isNonCapture;
            nodePtr->historyMovesN += isNonCapture;
        }
    }

    //white enpassant to the left
    if ( ((startingIndex + 7) == targetIndex) && (startPieceType == whitePawn) && (targetPieceType == emptySquare)) [[unlikely]] {
        enpassantCapture<true, false, false>(startingIndex);
    } else

    //white enpassant to the right
    if ( ((startingIndex + 9) == targetIndex) && (startPieceType == whitePawn) && (targetPieceType == emptySquare)) [[unlikely]] {
        enpassantCapture<true, true, false>(startingIndex);
    } 

    //double pawn push
    if ( ((startingIndex + 16) == targetIndex) && (startPieceType == whitePawn)) [[unlikely]] {
        enpassantFiles = (startingBitBoard >> 8);
    }
    
    //black enpassant to the left
    if ( ((startingIndex - 9) == targetIndex) && (startPieceType == blackPawn) && (targetPieceType == 12)) [[unlikely]] {
        enpassantCapture<false, false, false>(startingIndex);
    } else

    //black enpassant to the right
    if ( ((startingIndex - 7) == targetIndex) && (startPieceType == blackPawn) && (targetPieceType == 12)) [[unlikely]] {
        enpassantCapture<false, true, false>(startingIndex);
    } else

    //double pawn push
    if ( ((startingIndex - 16) == targetIndex) && (startPieceType == blackPawn)) [[unlikely]] {
        enpassantFiles = startingBitBoard >> 48;
    }

    
    //white castling
    if ((startPieceType == whiteKing) && (startingIndex - 2) == targetIndex) [[unlikely]] { castledRook<true, false, false>();} //white castling queen side
    else
    if ((startPieceType == whiteKing) && (startingIndex + 2) == targetIndex) [[unlikely]] { castledRook<true, true, false>();} //white castling king side

    //black castling
    if ((startPieceType == blackKing) && (startingIndex - 2) == targetIndex) [[unlikely]] {castledRook<false, false, false>();} //black castling queen side
    else 
    if ((startPieceType == blackKing) && (startingIndex + 2) == targetIndex) [[unlikely]] {castledRook<false, true, false>();}
    
    
    castlingFlags &= ~((whiteKingCastle | whiteQueenCastle) * uint32_t(startPieceType == whiteKing));
    castlingFlags &= ~((blackKingCastle | blackQueenCastle) * uint32_t(startPieceType == blackKing));

    //adjust castling rights if a rook moved.
    //white
    castlingFlags &= ~(whiteQueenCastle * uint32_t(startingIndex == 0));
    castlingFlags &= ~(whiteKingCastle  * uint32_t(startingIndex == 7));
    //black
    castlingFlags &= ~(blackQueenCastle * uint32_t(startingIndex == 56));
    castlingFlags   &= ~(blackKingCastle  * uint32_t(startingIndex == 63));



    //remove castling flag if rook is captured
    /* starting and target squares can be mapped to caslint rights with pext64. However that requires a remap of the
    current casling rights and a fast pext64 implementation. Still left to do.*/

    if      (targetIndex == 0 ) [[unlikely]] { castlingFlags &= ~whiteQueenCastle; } //white queen side
    else if (targetIndex == 7 ) [[unlikely]] { castlingFlags &= ~whiteKingCastle;  } //white king side
    else if (targetIndex == 56) [[unlikely]] { castlingFlags &= ~blackQueenCastle; } //black queen side
    else if (targetIndex == 63) [[unlikely]] { castlingFlags &= ~blackKingCastle;  } //black king side


    whiteToMove = !whiteToMove;

    //add the hash for castling changes and current enpassant state
    positionHash ^= zobristHashes[hash::enPassantFileHash + std::countr_zero(uint32_t(enpassantFiles) | 0x100U)];
    uint8_t changedCastlingRights = startCastlingFlags ^ castlingFlags;
    positionHash ^= zobristHashes[hash::CastlingHash + changedCastlingRights];


    return unmakeInfo;

}

#endif



