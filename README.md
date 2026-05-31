# Project_Nebulora
Project_Nebulora is a uci-compatible chess engine written in C++. Development has started in December of 2024 and is still in active development.

### Supported Commands
* `uci`, `isready`, `ucinewgame`, `eval`, `d`, `quit`
* `position` (startpos, fen, moves)
* `go` (nodes, movetime, wtime, btime, winc, binc, depth)
* `bench` (perft)
* `setoption` (Hash, Contempt)

---

## Features

### Search
* **Iterative Deepening** with Negamax (Alpha-Beta pruning), pvs_search and Null Move Pruning.
* **Quiescence Search** for tactical stability.
* **Transposition Table:** Bucketed (4 entries per bucket).
* **Move Generation:** Bitboard-based, templated for speed.

### Move Ordering
* **TT Move** (Transposition Table move) first.
* **Basic Capture Ranking** (MVV-LVA approximation).
* **Killer Heuristic** (2 moves per ply).
* **History Heuristic** (with gravity).

### Evaluation
* **PeSTO's Evaluation:** Phased evaluation with tempo bonus.

---

## Building:
* **Current compile command:** cl /EHsc /std::c++20 /O2 /Ob3 /arch:AVX2 /favor:INTEL64 /GL /c src/runNebulora.cpp
* **Current link command:** link /LTCG runNebulora.obj

## Performance & State
Project_Nebulora is in **active development**. It currently plays on Lichess as a registered bot account: [Project_Nebulora on Lichess](https://lichess.org/@/Project_Nebulora).

Despite having minimal search heuristics, it reaches a **Lichess Elo of 2000+** primarily through raw speed.

**Benchmarks (Intel i7-13700H):**
* **Perft Speed:** 420+ Mnps (bulk-counting).
* **Search Speed:** 8+ Mnps (average, heavily influenced by Quiescence nodes).

---

## Future Direction
* **Search:** (Reverse) Futility Pruning and LMR/Reductions.
* **Evaluation:** King safety, mobility, and pawn structure terms.
* **Move Ordering:** Continuation history (1, 2, 4, 6) and Static Exchange Evaluation (SEE).

---

## Credits
* **Ronald Friederich:** Special thanks for the weights and logic behind the [PeSTO evaluation function](https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function).
* **Chess Programming Wiki:** An invaluable resource for bitboard techniques and search algorithms.
* **The Community:** Thanks to the chess programming community for the wealth of shared knowledge found across the web.

## About the Author
Project_Nebulora is developed by **Molenaar2005** (known as **Pluk** in the commit history). I am a self-taught hobbyist developer passionate about chess programming and C++ optimization. I am always looking to learn; any constructive feedback is greatly appreciated.

## Contributing & Feedback
Project_Nebulora is a learning project, and I am very open to advice! 
* **Constructive Criticism:** If you see an optimization I missed or a bug in my bitboard logic, please open an **Issue**.
* **Ideas:** Feel free to suggest search heuristics or evaluation terms.
* **Code:** Pull requests are welcome, though I prefer to implement core search logic myself as I learn.

## License
This project is licensed under the **GNU General Public License v3.0 (GPLv3)**. See the [LICENSE](./LICENSE.txt) and [AUTHORS](./AUTHORS.txt) files for full legal details.

> *“This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.”*


