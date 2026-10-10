# Glimmer
A chess engine under 30,000 bytes on Linux
## Build
make
### Design Philosophy

Unlike those minimal engines that pack themselves into self-extracting shell scripts and then call the host’s compiler at runtime, Glimmer is a genuine standalone native binary — pure machine code, nothing more.

Windows is a different story. The PE/COFF format brings so much extra baggage (section alignment padding, runtime initialization overhead…) that any `.exe` would blow past the 30,000-byte limit. That’s why Glimmer stays deliberately Linux-only and ELF-native. No Windows builds are provided or supported.
#### 📊 Benchmark & Strength
Glimmer was tested natively on Linux in **257** head-to-head games (no adjudication — everything played out to the end) at **10s + 0.1s** increment. Here’s how it performed:

| Opponent Engine     | Baseline Elo | Games | Score (+ / – / =) | Win Rate | Glimmer Performance |
|---------------------|:------------:|:-----:|:-----------------:|:--------:|:-------------------:|
| **Belette 3.0.0**   |     2806     |  109  |    43 / 31 / 35   |  55.5%   |     **~2844 Elo**   |
| **Fatalii 0.10.0**  |     2737     |  100  |    54 / 22 / 24   |  66.0%   |     **~2852 Elo**   |
| **Toad 3.0.0**      |     2616     |   48  |    31 / 5 / 12    |  77.1%   |     **~2827 Elo**   |
| **TOTAL / OVERALL** |   **~2744**  | **257** | **128 / 58 / 71** | **63.6%** | **~2841 ± 28 Elo** |

📁 *All game logs (PGN) are available in the [`benchmarks`](./benchmarks) folder.
#### Size
```bash
$ wc -c Glimmer1.0
24824 Glimmer1.0
```
