# Group 8 – Buffering and Spooling Simulation

**Scenario:** University Printing Centre During Exam Week
**Group 8:** Charbel Ngeti (221441) · Russel Muiga (220510) · Alvin Otanga (220267) · Handel Songoro (220337) · Jesse Kyongo (180904)

Five students send documents to one shared printer at almost the same time. Their computers produce print data much faster than the printer can print it. The simulation shows how the operating system copes with this:

- **Buffering (Part A)** moves the *data* of one job from a fast producer (the computer) to a slow consumer (the printer) through memory buffers.
- **Spooling (Part B)** queues complete *jobs* from many students on disk so that they can share one non-sharable printer. Device reservation is the alternative it is compared against.

There are two versions of the program: `buffering_spooling.cpp` (C++17) and `buffering_spooling.py` (Python 3). They are equivalent: they use the same workload, rules and simulated clock, and for the same keyboard input they print **exactly the same output**. This was checked by running both programs on the same input scripts and comparing every line. Use whichever one suits the presentation computer.

---

## 1. Requirements

| Version | You need | External libraries |
|---|---|---|
| C++ | Any C++17 compiler (g++ 7+, clang 5+, or Visual Studio 2017+) | None (standard library only) |
| Python | Python 3.6 or later | None (standard library only) |

The program only needs a text console and uses plain ASCII output, so it works in Windows Terminal, PowerShell, cmd, macOS Terminal, Linux and WSL Ubuntu.

## 2. Compiling and running

### C++

```bash
# Linux / macOS / WSL / Git Bash / MSYS2
g++ -std=c++17 -O2 -Wall -o buffering_spooling buffering_spooling.cpp
./buffering_spooling
```

```powershell
# Windows (MinGW / MSYS2 g++)
g++ -std=c++17 -O2 -Wall -o buffering_spooling.exe buffering_spooling.cpp
.\buffering_spooling.exe

# Windows (Visual Studio Developer Command Prompt)
cl /std:c++17 /EHsc buffering_spooling.cpp
buffering_spooling.exe
```

### Python

```bash
python3 buffering_spooling.py      # Linux / macOS / WSL
py buffering_spooling.py           # Windows
```

**Presenting tip:** enlarge the terminal font to at least 18 pt and widen the window to about 90 columns so the class can read the timelines.

## 3. Using the simulation

A menu appears when the program starts. Type an option number and press **Enter**. The program stops at `[Press Enter to continue]` so the presenter can explain each step. At a `CLASS PREDICTION` prompt, ask the audience for their guess before pressing Enter to reveal the result.

| Option | Demonstration | Concept shown |
|---|---|---|
| 1 | No buffering: (a) pages are lost (data overrun); (b) lock-step waiting | Speed mismatch: why the OS buffers |
| 2 | 1 KB application pieces vs 4 KB printer blocks | Transfer-size mismatch |
| 3 | A student edits a document while it prints | Copy semantics |
| 4 | One kernel buffer slot | Single buffering |
| 5 | Two slots, filled and emptied alternately | Double buffering |
| 6 | A ring of 3–6 slots with `in`/`out` pointers (**the class chooses N**) | Circular buffering |
| 7 | All approaches on the whole 75-page workload | Buffering trade-offs (memory vs waiting) |
| 8 | Five programs print directly at once, so their output is mixed | Why a non-sharable device needs coordination |
| 9 | `allocate()`/`release()`, plus (b) a crash causing starvation and (c) printer + scanner deadlock | Device reservation and its failure cases |
| 10 | Spool files, print queue and spooler; **the class may cancel a job** | Spooling and the spooler |
| 11 | Side-by-side results for the same five students | Spooling vs device reservation trade-off |
| 12 | Runs options 1–11 in presentation order with compact output (about 10 minutes) | Whole story |
| 13 | Lists every parameter | Assumptions |
| 14 | Enter classmates' names for Students A–E | Class participation |
| 15 | Turns the detailed per-event log on or off | – |

In the timelines, each character is one time unit:

```
Computer  |RCRWWWC...    R = preparing a page, C = copying into a buffer, W = blocked, - = finished
Printer   |..####.###    # = printing, . = idle
```

In the buffer display, each `[...]` is one kernel buffer slot:

```
[A3  ] full, waiting to print   [A3 *] being printed   [A3 +] being filled   [....] empty
in:2 out:0   circular buffer pointers (next slot to fill / next slot to print)
```

## 4. Simulation model and assumptions

Time is counted in simulated **time units (tu)**, not real seconds, so every run is identical and repeatable. Real devices differ in speed by far larger ratios. These values are scaled down so the class can follow the timelines.

**Workload (used by every demonstration):**

| Student | Pages | Clicks Print at |
|---|---|---|
| A | 10 | t = 0 |
| B | 20 | t = 1 |
| C | 15 | t = 2 |
| D | 5 | t = 3 |
| E | 25 | t = 4 |

**Buffering parameters:**

| Symbol | Meaning | Value |
|---|---|---|
| C | Computer prepares a text page | 1 tu |
| C | Computer prepares a page with a diagram (every 5th page) | 8 tu |
| M | Copy one page into a kernel buffer slot | 1 tu |
| T | Printer prints one page | 4 tu |
| – | Size of one page (= one buffer slot) | 4 KB |
| – | Transfer-size demo | 1 KB application pieces, 4 KB device blocks, 2 tu set-up per device transfer + 1 tu per KB |

The diagram pages create **bursts**. The computer is faster than the printer on average (about 2.4 + 1 tu per page versus 4 tu), but it is sometimes slower. This is what separates double buffering from circular buffering.

**Model rules (the producer–consumer problem):**

- **No buffering:** the printer reads each page directly from program memory, so the program is blocked while each page prints. In the "no waiting" variant, pages that arrive while the printer is busy are lost.
- **Buffered (N slots):** the computer prepares a page, waits for an *empty* slot at `in`, copies the page into it, then moves `in` forward with `(in + 1) mod N`. The printer takes the *full* slot at `out`, prints from it, frees it, then moves `out` forward with `(out + 1) mod N`. A slot cannot be filled and printed at the same time. N = 1 is single buffering, N = 2 is double buffering and N ≥ 3 is circular buffering.

**Spooling and device reservation parameters:**

- **Printing speed:** the printer prints 4 tu per page in both approaches.
- **Spooling:** writing a spool file to disk takes 1 tu per 10 pages (rounded up). A job joins the FIFO print queue only when its spool file is complete. The spooler then prints one whole job at a time and deletes the spool file afterwards.
- **Device reservation:** a student's program calls `allocate(printer)`. If the printer is busy, the student waits in a FIFO queue. The owner sends its pages directly to the printer and calls `release(printer)` at the end. Student B pauses 12 tu after page 10 to check the margins. Student D keeps the printer for 8 tu after finishing, to review the pages. During both pauses the printer is idle but still reserved.
- **Starvation demo:** Student A's program crashes before calling `release()`. A watchdog stops the run at t = 200.

## 5. Key results (identical in both versions)

| Approach (75-page workload) | Slots | Finish (tu) | Computer blocked | Printer idle | Printer utilisation |
|---|---|---|---|---|---|
| No buffering (lock-step) | 0 | 480 | 300 | 180 | 62.5% |
| Single buffering | 1 | 436 | 177 | 136 | 68.8% |
| Double buffering | 2 | 332 | 73 | 32 | 90.4% |
| Circular buffering | 4 | 302 | 37 | 2 | 99.3% |
| Circular buffering | 6 | 302 | 29 | 2 | 99.3% |

With no buffering and no waiting, **45 of the 75 pages are lost**.

| Five students | Device reservation | Spooling |
|---|---|---|
| Total time students are blocked | 894 tu | 9 tu |
| All jobs finished at | 320 tu | 301 tu |
| Printer utilisation | 93.8% | 99.7% |
| Printer idle while reserved | 20 tu | 0 tu |
| Disk space needed (peak) | 0 KB | 300 KB |

## 6. Testing that was carried out

- The C++ version compiles with no warnings using `g++ -std=c++17 -Wall -Wextra -pedantic` and also using `-std=c++20` (g++ 15.1).
- Both programs were run with the same three input scripts: every menu option, the guided demonstration, and edge cases (invalid menu input, renamed students, a 19-character name, job cancellation in upper and lower case, and input ending early). The outputs were compared line by line and were **identical**.
