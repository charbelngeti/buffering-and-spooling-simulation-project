/*
===============================================================================
 OPERATING SYSTEMS - GROUP 8 - BUFFERING AND SPOOLING SIMULATION (C++17)
 Scenario: University Printing Centre During Exam Week
===============================================================================
 Group members: Charbel Ngeti (221441), Russel Muiga (220510),
                Alvin Otanga (220267), Handel Songoro (220337),
                Jesse Kyongo (180904), Abigael Wambui (183891)

 This program is a line-for-line equivalent of buffering_spooling.py.
 Both programs use the same workload, the same rules and the same simulated
 clock, so they print the same results.

 Compile:  g++ -std=c++17 -O2 -Wall -o buffering_spooling buffering_spooling.cpp
 Run:      ./buffering_spooling          (Windows: buffering_spooling.exe)

 HOW TIME WORKS
 --------------
 Nothing here depends on the real clock. The simulation advances a counter
 called a "time unit" (tu). In every tu each component (computer, buffer,
 printer, spooler) does one tu of work. This makes every run repeatable, so
 the class sees the same behaviour every time.

 THE TWO PARTS
 -------------
 Part A - BUFFERING: how the OS moves the data of ONE print job from a fast
          producer (the computer) to a slow consumer (the printer) using
          memory buffers.
 Part B - SPOOLING:  how the OS lets MANY students share ONE printer by
          queueing complete jobs on disk (versus device reservation).
===============================================================================
*/

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace std;

// =============================================================================
// SIMULATION ASSUMPTIONS (identical in the Python version)
// =============================================================================
const int PRINT_TIME_PER_PAGE = 4;   // T: tu for the printer to print one page
const int COPY_TIME_PER_PAGE = 1;    // M: tu to copy one page into a kernel buffer slot
const int TEXT_PAGE_RENDER = 1;      // C: tu for the computer to prepare a text page
const int DIAGRAM_PAGE_RENDER = 8;   // C: tu to prepare a page that contains a diagram
const int DIAGRAM_EVERY = 5;         // every 5th page of a document contains a diagram
const int PAGE_SIZE_KB = 4;          // one page of print data = one 4 KB block
const int DEFAULT_CIRCULAR_SLOTS = 4;

// Transfer-size mismatch demonstration
const int CHUNK_KB = 1;              // the application hands data to the OS in 1 KB pieces
const int DEVICE_SETUP_TIME = 2;     // fixed cost of starting ONE device transfer
const int TIME_PER_KB = 1;           // cost of moving 1 KB to the printer

// Spooling / device reservation demonstration
const int PAGES_SPOOLED_PER_TU = 10; // writing a spool file to disk: 10 pages per tu
const int WATCHDOG_LIMIT = 200;      // starvation demo stops after this many tu

const int LINE_WIDTH = 78;

// =============================================================================
// THE WORKLOAD: five students submit documents almost at the same time
// =============================================================================
struct Student {
    string letter;          // short label used in page names (A1, B7..)
    string name;            // display name (classmates can rename)
    int pages;              // document length in pages
    int submitTime;         // when the student clicks "Print"
    // Behaviour that only matters under DEVICE RESERVATION:
    int pauseAfterPage;     // stops to check printout...
    int pauseLength;        // ...for this many tu
    int holdAfterFinish;    // keeps printer before release()
};

vector<Student> makeStudents() {
    return {
        {"A", "Student A", 10, 0, 0, 0, 0},
        {"B", "Student B", 20, 1, 10, 12, 0},   // checks margins after page 10
        {"C", "Student C", 15, 2, 0, 0, 0},
        {"D", "Student D", 5, 3, 0, 0, 8},      // reviews pages before release
        {"E", "Student E", 25, 4, 0, 0, 0},
    };
}

vector<Student> STUDENTS = makeStudents();
bool SHOW_EVENT_LOG = true;

// One page of print data: its label (e.g. "A5") and its preparation time.
struct Page {
    string label;
    int renderTime;
};

// Time the computer needs to prepare page n (1-based) of a document.
int renderTime(int pageNumber) {
    if (pageNumber % DIAGRAM_EVERY == 0) return DIAGRAM_PAGE_RENDER;
    return TEXT_PAGE_RENDER;
}

vector<Page> documentPages(const Student& s) {
    vector<Page> pages;
    for (int p = 1; p <= s.pages; ++p) pages.push_back({s.letter + to_string(p), renderTime(p)});
    return pages;
}

// All five documents as one continuous stream of 75 pages.
vector<Page> wholeWorkload() {
    vector<Page> pages;
    for (const Student& s : STUDENTS) {
        vector<Page> d = documentPages(s);
        pages.insert(pages.end(), d.begin(), d.end());
    }
    return pages;
}

// =============================================================================
// CONSOLE HELPERS (kept identical to the Python version so outputs match)
// =============================================================================
// printf-style formatting into a std::string
string fmt(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return string(buffer);
}

void out(const string& text = "") { cout << text << "\n"; }

string repeatChar(char c, int n) { return string(max(n, 0), c); }

string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

string join(const vector<string>& parts, const string& sep) {
    string r;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) r += sep;
        r += parts[i];
    }
    return r;
}

// Greedy word wrap (same algorithm in Python).
void wrap(const string& text, int indent = 2) {
    string prefix(indent, ' ');
    string line;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find(' ', start);
        if (end == string::npos) end = text.size();
        string w = text.substr(start, end - start);
        start = end + 1;
        if (w.empty()) continue;
        if (line.empty()) line = w;
        else if ((int)(prefix.size() + line.size() + 1 + w.size()) <= LINE_WIDTH) line += " " + w;
        else { out(prefix + line); line = w; }
    }
    if (!line.empty()) out(prefix + line);
}

void banner(const string& title) {
    out();
    out(repeatChar('=', LINE_WIDTH));
    out("  " + title);
    out(repeatChar('=', LINE_WIDTH));
}

void section(const string& title) {
    out();
    out(repeatChar('-', LINE_WIDTH));
    out("  " + title);
    out(repeatChar('-', LINE_WIDTH));
}

// Highlighted box linking what happens on screen to the OS concept.
void explainConcept(const string& text) {
    out();
    out("  >> OS CONCEPT: ");
    wrap(text, 5);
}

string readLine(const string& prompt) {
    cout << prompt << flush;
    string line;
    if (!getline(cin, line)) {
        out();
        out("(Input ended - exiting.)");
        exit(0);
    }
    return line;
}

void pause() { readLine("\n  [Press Enter to continue] "); }

// Class participation: ask the audience to predict before revealing.
void predict(const string& question) {
    out();
    out("  ?? CLASS PREDICTION: " + question);
    readLine("  [Discuss, then press Enter to reveal] ");
}

int readInt(const string& prompt, int low, int high, int defaultValue) {
    while (true) {
        string text = trim(readLine(prompt));
        if (text.empty()) return defaultValue;
        bool digits = all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
        if (digits && text.size() <= 6) {
            int v = stoi(text);
            if (v >= low && v <= high) return v;
        }
        out("  Please enter a whole number from " + to_string(low) + " to " + to_string(high) + ".");
    }
}

void logEvent(int t, const string& message) {
    if (SHOW_EVENT_LOG) out(fmt("  t=%3d  %s", t, message.c_str()));
}

string pct(int part, int whole) {
    if (whole == 0) return "0.0%";
    return fmt("%.1f%%", 100.0 * part / whole);
}

// Gantt-style chart: one character per tu.
void drawTimeline(const string& computer, const string& printer) {
    int length = (int)computer.size();
    string ruler(length, ' ');
    for (int i = 0; i < length; i += 10) {
        string label = to_string(i);
        for (int j = 0; j < (int)label.size(); ++j)
            if (i + j < length) ruler[i + j] = label[j];
    }
    out();
    out("  Time (tu) |" + ruler);
    out("  Computer  |" + computer);
    out("  Printer   |" + printer);
    out("  Key: R=preparing page  C=copying into buffer  W=WAITING (blocked)");
    out("       -=finished, free  #=printer printing     .=printer IDLE");
}

// =============================================================================
// PART A - BUFFERING
// =============================================================================
enum SlotState { EMPTY, FILLING, FULL, PRINTING };

/*
 A set of N buffer slots in KERNEL memory, used as a ring.
   N = 1 -> single buffering
   N = 2 -> double buffering (one slot fills while the other drains)
   N > 2 -> circular buffering
 The producer always fills the slot at 'inPtr'; the consumer (printer)
 always drains the slot at 'outPtr'. Both pointers wrap around with
 modulo arithmetic: ptr = (ptr + 1) % N.
 (Because every slot keeps its own state, all N slots can be used. A ring
 that only stores the two pointers must leave one slot empty to tell
 'full' apart from 'empty'.)
*/
class RingBuffer {
public:
    explicit RingBuffer(int numSlots)
        : n(numSlots), page(numSlots, ""), state(numSlots, EMPTY), inPtr(0), outPtr(0) {}

    // --- producer side -------------------------------------------------------
    bool canFill() const { return state[inPtr] == EMPTY; }
    int startFill(const string& label) {
        page[inPtr] = label;
        state[inPtr] = FILLING;
        return inPtr;
    }
    int finishFill() {
        int slot = inPtr;
        state[slot] = FULL;
        inPtr = (inPtr + 1) % n;
        return slot;
    }

    // --- consumer side -------------------------------------------------------
    bool canDrain() const { return state[outPtr] == FULL; }
    int startDrain() {
        int slot = outPtr;
        state[slot] = PRINTING;
        return slot;
    }
    int finishDrain() {
        int slot = outPtr;
        state[slot] = EMPTY;
        page[slot] = "";
        outPtr = (outPtr + 1) % n;
        return slot;
    }

    const string& pageIn(int slot) const { return page[slot]; }

    string describe() const {
        string text;
        for (int i = 0; i < n; ++i) {
            if (state[i] == EMPTY) {
                text += "[....]";
            } else {
                char flag = state[i] == FILLING ? '+' : (state[i] == FULL ? ' ' : '*');
                text += fmt("[%-3s%c]", page[i].c_str(), flag);
            }
        }
        if (n >= 3) text += fmt(" in:%d out:%d", inPtr, outPtr);
        return text;
    }

private:
    int n;
    vector<string> page;
    vector<SlotState> state;
    int inPtr;
    int outPtr;
};

struct BufferResult {
    int totalTime = 0;
    int computerFreeAt = 0;
    int computerBlocked = 0;
    int printerIdle = 0;
    int printerBusy = 0;
    int pagesPrinted = 0;
    vector<string> lostPages;
    string computerLine;
    string printerLine;
};

enum ProducerState { RENDER, READY, COPY, BLOCKED, DONE };

/*
 Producer-consumer with N kernel buffer slots.
 Producer (computer): prepare page (C) -> wait for an EMPTY slot ->
                      copy page into slot (M) -> next page.
 Consumer (printer):  take the oldest FULL slot -> print it (T) -> slot
                      becomes EMPTY again.
*/
BufferResult simulateBuffered(const vector<Page>& pages, int numSlots) {
    RingBuffer buf(numSlots);
    BufferResult r;
    int n = (int)pages.size();
    int t = 0;
    int index = 0;                       // page the computer is working on
    ProducerState prodState = RENDER;
    int prodLeft = pages[0].renderTime;
    string printerPage;
    int printerLeft = 0;
    bool blockedReported = false;
    bool idleReported = false;

    auto show = [&](const string& message) {
        logEvent(t, fmt("%-36s %s", message.c_str(), buf.describe().c_str()));
    };

    while (r.pagesPrinted < n) {
        // ---- 1. decisions at the start of tick t ---------------------------
        if (prodState == READY) {
            if (buf.canFill()) {
                buf.startFill(pages[index].label);
                prodState = COPY;
                prodLeft = COPY_TIME_PER_PAGE;
                blockedReported = false;
            } else if (!blockedReported) {
                show("Computer BLOCKED: no free slot");
                blockedReported = true;
            }
        }
        if (printerPage.empty()) {
            if (buf.canDrain()) {
                int slot = buf.startDrain();
                printerPage = buf.pageIn(slot);
                printerLeft = PRINT_TIME_PER_PAGE;
                idleReported = false;
                show(fmt("Printer starts %s (slot B%d)", printerPage.c_str(), slot));
            } else if (!idleReported) {
                show("Printer IDLE: buffer is empty");
                idleReported = true;
            }
        }

        // ---- 2. record what each component does during this tick ----------
        char c = prodState == RENDER ? 'R' : prodState == COPY ? 'C' : prodState == READY ? 'W' : '-';
        r.computerLine += c;
        if (prodState == READY) r.computerBlocked++;
        if (!printerPage.empty()) { r.printerLine += '#'; r.printerBusy++; }
        else { r.printerLine += '.'; r.printerIdle++; }

        // ---- 3. one tu passes; apply completions at time t+1 --------------
        t++;
        if (prodState == RENDER || prodState == COPY) prodLeft--;
        if (!printerPage.empty()) printerLeft--;

        if (prodState == RENDER && prodLeft == 0) {
            prodState = READY;
        } else if (prodState == COPY && prodLeft == 0) {
            int slot = buf.finishFill();
            show(fmt("%s copied into slot B%d", pages[index].label.c_str(), slot));
            index++;
            if (index < n) {
                prodState = RENDER;
                prodLeft = pages[index].renderTime;
            } else {
                prodState = DONE;
                r.computerFreeAt = t;
                show("Computer done: free for other work");
            }
        }
        if (!printerPage.empty() && printerLeft == 0) {
            buf.finishDrain();
            r.pagesPrinted++;
            printerPage = "";
        }
    }
    r.totalTime = t;
    return r;
}

/*
 NO BUFFERING (with a handshake): the printer reads each page straight
 from the application's own memory, so the program must stop and wait
 until the slow printer has finished with that page.
*/
BufferResult simulateUnbufferedLockstep(const vector<Page>& pages) {
    BufferResult r;
    int n = (int)pages.size();
    int t = 0;
    int index = 0;
    ProducerState prodState = RENDER;
    int prodLeft = pages[0].renderTime;
    int printerLeft = 0;
    while (r.pagesPrinted < n) {
        if (prodState == READY) {
            prodState = BLOCKED;
            printerLeft = PRINT_TIME_PER_PAGE;
            logEvent(t, fmt("Printer prints %s straight from program memory -> computer BLOCKED",
                            pages[index].label.c_str()));
        }
        bool busy = prodState == BLOCKED;
        r.computerLine += prodState == RENDER ? 'R' : prodState == BLOCKED ? 'W' : '-';
        r.printerLine += busy ? '#' : '.';
        if (busy) { r.computerBlocked++; r.printerBusy++; }
        else r.printerIdle++;
        t++;
        if (prodState == RENDER) {
            prodLeft--;
            if (prodLeft == 0) prodState = READY;
        } else if (prodState == BLOCKED) {
            printerLeft--;
            if (printerLeft == 0) {
                r.pagesPrinted++;
                index++;
                if (index < n) {
                    prodState = RENDER;
                    prodLeft = pages[index].renderTime;
                } else {
                    prodState = DONE;
                    r.computerFreeAt = t;
                }
            }
        }
    }
    r.totalTime = t;
    return r;
}

/*
 NO BUFFERING and NO waiting: the fast computer pushes each page the
 moment it is ready. The printer can only hold the page it is printing,
 so any page that arrives while it is busy is LOST (data overrun).
*/
BufferResult simulateUnbufferedOverrun(const vector<Page>& pages) {
    BufferResult r;
    int t = 0;
    int printerLeft = 0;
    int totalRender = 0;
    for (const Page& p : pages) {
        totalRender += p.renderTime;
        for (int i = 0; i < p.renderTime; ++i) {   // computer prepares the page
            r.computerLine += 'R';
            r.printerLine += printerLeft > 0 ? '#' : '.';
            if (printerLeft > 0) printerLeft--;
            t++;
        }
        if (printerLeft == 0) {
            logEvent(t, fmt("%s sent -> printer free, accepts it", p.label.c_str()));
            printerLeft = PRINT_TIME_PER_PAGE;
            r.pagesPrinted++;
        } else {
            logEvent(t, fmt("%s sent -> printer BUSY: %s is LOST!", p.label.c_str(), p.label.c_str()));
            r.lostPages.push_back(p.label);
        }
    }
    while (printerLeft > 0) {
        r.computerLine += '-';
        r.printerLine += '#';
        printerLeft--;
        t++;
    }
    r.computerFreeAt = totalRender;
    r.totalTime = t;
    return r;
}

void printBufferResult(const string& title, const BufferResult& r, int nPages, int memoryKb) {
    out();
    out("  RESULT - " + title);
    out(fmt("    Kernel buffer memory used ........ %d KB", memoryKb));
    out(fmt("    All %2d pages printed at .......... t = %d tu", nPages, r.totalTime));
    out(fmt("    Computer free for other work at .. t = %d tu", r.computerFreeAt));
    out(fmt("    Computer blocked (waiting) ....... %d tu", r.computerBlocked));
    out(fmt("    Printer idle (starved of data) ... %d tu  -> printer utilisation %s",
            r.printerIdle, pct(r.printerBusy, r.totalTime).c_str()));
}

void describeDocument(const Student& s) {
    vector<Page> pages = documentPages(s);
    out(fmt("  Document: %s's revision notes, %d pages.", s.name.c_str(), s.pages));
    vector<string> parts;
    for (const Page& p : pages) parts.push_back(fmt("%s=%d", p.label.c_str(), p.renderTime));
    out("  Preparation time per page: " + join(parts, " "));
    out(fmt("  (Every 5th page has a diagram, so it takes %d tu to prepare instead of %d.)",
            DIAGRAM_PAGE_RENDER, TEXT_PAGE_RENDER));
    out(fmt("  The printer needs %d tu per page. Copying a page into a buffer takes %d tu.",
            PRINT_TIME_PER_PAGE, COPY_TIME_PER_PAGE));
}

void bufferKey() {
    out("  Buffer key: [A3  ]=full, waiting to print  [A3 *]=being printed");
    out("              [A3 +]=being filled             [....]=empty slot");
}

// ---------------------------------------------------------------------------
void demoNoBuffering() {
    banner("DEMO 1 - WHY BUFFER? SPEED MISMATCH WITH NO BUFFERING");
    wrap("Student A presses Print. The computer can prepare most pages in 1 tu, "
         "but the printer needs 4 tu to print each page. There is NO buffer "
         "between them. Two things can happen, and both are bad.");
    const Student& student = STUDENTS[0];
    vector<Page> pages = documentPages(student);
    out();
    describeDocument(student);

    section("1(a) Computer pushes pages without waiting -> DATA OVERRUN");
    predict("With no buffer and no waiting, how many of the 10 pages will print?");
    BufferResult r = simulateUnbufferedOverrun(pages);
    drawTimeline(r.computerLine, r.printerLine);
    out();
    out(fmt("  Pages printed: %d of %d", r.pagesPrinted, (int)pages.size()));
    out("  Pages LOST   : " + join(r.lostPages, ", "));
    explainConcept("Speed mismatch. The producer (computer) is faster than the "
            "consumer (printer). With nowhere to hold the extra data, pages "
            "that arrive while the printer is busy are lost.");
    pause();

    section("1(b) Computer waits for the printer (lock-step) -> NOTHING LOST, BUT SLOW");
    r = simulateUnbufferedLockstep(pages);
    drawTimeline(r.computerLine, r.printerLine);
    printBufferResult("No buffering (lock-step)", r, (int)pages.size(), 0);
    explainConcept("Without a buffer the fast computer is forced to run at the speed "
            "of the slow printer: it is blocked for every page it sends, and "
            "the printer sits idle while the next page is being prepared. A "
            "buffer in memory decouples the producer from the consumer.");
    pause();
}

void demoTransferSize() {
    banner("DEMO 2 - WHY BUFFER? TRANSFER-SIZE MISMATCH");
    const Student& student = STUDENTS[3];
    int totalKb = student.pages * PAGE_SIZE_KB;
    int chunks = totalKb / CHUNK_KB;
    wrap(fmt("%s prints a %d-page document (%d KB). The word processor hands the "
             "data to the OS in small %d KB pieces, but the printer interface moves "
             "data in %d KB blocks (one page). Every device transfer has a fixed "
             "set-up cost of %d tu (command + interrupt) plus %d tu per KB.",
             student.name.c_str(), student.pages, totalKb, CHUNK_KB, PAGE_SIZE_KB,
             DEVICE_SETUP_TIME, TIME_PER_KB));

    section("2(a) No buffer: every small piece becomes its own device transfer");
    int unbufferedOps = 0;
    int unbufferedTime = 0;
    for (int piece = 0; piece < chunks; ++piece) {
        unbufferedOps++;                     // one device operation per piece
        unbufferedTime += DEVICE_SETUP_TIME + CHUNK_KB * TIME_PER_KB;
    }
    out(fmt("  %d pieces x (%d tu set-up + %d tu data) = %d tu, %d device transfers",
            chunks, DEVICE_SETUP_TIME, CHUNK_KB * TIME_PER_KB, unbufferedTime, unbufferedOps));
    out(fmt("  The printer also receives %d partial blocks instead of whole pages.", chunks));

    section("2(b) Kernel buffer collects pieces until one full 4 KB block is ready");
    int fill = 0;          // KB currently held in the kernel buffer
    int ops = 0;
    int t = 0;
    for (int piece = 1; piece <= chunks; ++piece) {
        fill += CHUNK_KB;
        string bar = repeatChar('#', fill) + repeatChar('-', PAGE_SIZE_KB - fill);
        if (fill == PAGE_SIZE_KB) {
            // a whole block is assembled: ONE device transfer moves it
            ops++;
            t += DEVICE_SETUP_TIME + PAGE_SIZE_KB * TIME_PER_KB;
            if (SHOW_EVENT_LOG || piece <= PAGE_SIZE_KB)
                out(fmt("    piece %2d -> buffer [%s] %d/%d KB FULL -> transfer %d sends page %d",
                        piece, bar.c_str(), fill, PAGE_SIZE_KB, ops, ops));
            fill = 0;
        } else if (SHOW_EVENT_LOG || piece <= PAGE_SIZE_KB) {
            out(fmt("    piece %2d -> buffer [%s] %d/%d KB, keep collecting",
                    piece, bar.c_str(), fill, PAGE_SIZE_KB));
        }
    }
    if (!SHOW_EVENT_LOG) out(fmt("    ... the same pattern repeats for pages 2 to %d", student.pages));
    out();
    out(fmt("  %-30s %18s %12s", "Approach", "Device transfers", "Time (tu)"));
    out(fmt("  %-30s %18d %12d", "No buffer (1 KB pieces)", unbufferedOps, unbufferedTime));
    out(fmt("  %-30s %18d %12d", "Buffered (4 KB blocks)", ops, t));
    explainConcept("Transfer-size mismatch. The two sides use different data sizes. "
            "The buffer reassembles small pieces into the block size the "
            "device expects, so there are fewer, larger transfers and fewer "
            "interrupts. (Networking does the reverse too: it fragments large "
            "messages into small packets and reassembles them at the receiver.)");
    pause();
}

// Runs one copy-semantics scenario; returns when write() returned and what printed.
pair<int, vector<string>> runCopyScenario(const vector<string>& original, bool copySemantics) {
    vector<string> userMemory = original;          // the application's own buffer
    vector<string> kernelBuffer;
    const vector<string>* source;
    int returnsAt;
    if (copySemantics) {
        // write() copies the data into a KERNEL buffer before returning.
        kernelBuffer = userMemory;                 // a real copy, not a reference
        returnsAt = (int)original.size() * COPY_TIME_PER_PAGE;
        source = &kernelBuffer;
    } else {
        // The printer is pointed straight at the application's memory.
        returnsAt = 0;
        source = &userMemory;
    }
    map<int, pair<int, string>> edits = {
        {returnsAt + 1, {1, "Q2: [deleted - rewriting answer]"}},
        {returnsAt + 3, {2, "Q3: Answer = ??? (draft)"}},
    };
    vector<string> printed;
    int start = returnsAt;
    int count = (int)original.size();
    for (int t = start; t < start + count * PRINT_TIME_PER_PAGE + 1; ++t) {
        auto e = edits.find(t);
        if (e != edits.end()) {                    // the student edits first ...
            userMemory[e->second.first] = e->second.second;
            logEvent(t, fmt("student edits page %d in the editor", e->second.first + 1));
        }
        int offset = t - start;                    // ... then the printer reads
        if (offset % PRINT_TIME_PER_PAGE == 0 && offset / PRINT_TIME_PER_PAGE < count) {
            int page = offset / PRINT_TIME_PER_PAGE;
            printed.push_back((*source)[page]);
            logEvent(t, fmt("printer reads page %d: \"%s\"", page + 1, (*source)[page].c_str()));
        }
    }
    return {returnsAt, printed};
}

void demoCopySemantics() {
    banner("DEMO 3 - WHY BUFFER? COPY SEMANTICS");
    const Student& student = STUDENTS[2];
    vector<string> original = {"Q1: F = m x a (Newton's 2nd law)",
                               "Q2: Answer = 42 m/s",
                               "Q3: Answer = 17 J"};
    wrap(fmt("%s prints a 3-page answer sheet and the write() call returns at once "
             "(asynchronous I/O). Straight after, the student keeps editing the "
             "SAME document in the editor: 1 tu later page 2 is cleared to be "
             "rewritten, and 3 tu later page 3 is changed. The printer reads each "
             "page when it starts printing it (every %d tu).",
             student.name.c_str(), PRINT_TIME_PER_PAGE));
    out();
    out("  What the student printed (version at the moment of write()):");
    for (const string& line : original) out("    | " + line);

    section("3(a) WITHOUT copy semantics: printer reads the application's memory");
    auto result = runCopyScenario(original, false);
    out();
    out(fmt("  write() returned at t=%d. Printed output:", result.first));
    int wrong = 0;
    for (size_t i = 0; i < result.second.size(); ++i) {
        const string& line = result.second[i];
        string mark = line == original[i] ? "OK" : "WRONG";
        if (line != original[i]) wrong++;
        out(fmt("    %-5s | %s", mark.c_str(), line.c_str()));
    }
    out(fmt("  -> %d of %d pages do NOT match what the student printed.", wrong, (int)original.size()));
    pause();

    section("3(b) WITH copy semantics: write() copies the data into a kernel buffer");
    result = runCopyScenario(original, true);
    out();
    out(fmt("  write() returned at t=%d (it spent %d tu copying %d KB). Printed output:",
            result.first, result.first, (int)original.size() * PAGE_SIZE_KB));
    for (size_t i = 0; i < result.second.size(); ++i) {
        const string& line = result.second[i];
        string mark = line == original[i] ? "OK" : "WRONG";
        out(fmt("    %-5s | %s", mark.c_str(), line.c_str()));
    }
    explainConcept("Copy semantics: the data written to the device must be the "
            "version that existed when write() was called, even if the "
            "application changes its buffer afterwards. The OS guarantees "
            "this by copying the data into a kernel buffer before write() "
            "returns. Trade-off: the copy costs time and kernel memory.");
    pause();
}

BufferResult runSingleMode(const string& title, int slots, const string& before, const string& after) {
    const Student& student = STUDENTS[0];
    vector<Page> pages = documentPages(student);
    banner(title);
    wrap(before);
    out();
    describeDocument(student);
    if (SHOW_EVENT_LOG) {
        out();
        bufferKey();
        out();
    }
    BufferResult r = simulateBuffered(pages, slots);
    drawTimeline(r.computerLine, r.printerLine);
    string name;
    if (slots == 1) name = "Single buffering (1 slot)";
    else if (slots == 2) name = "Double buffering (2 slots)";
    else name = fmt("Circular buffering (%d slots)", slots);
    printBufferResult(name, r, (int)pages.size(), slots * PAGE_SIZE_KB);
    explainConcept(after);
    pause();
    return r;
}

void demoSingle() {
    runSingleMode(
        "DEMO 4 - SINGLE BUFFERING (one kernel buffer slot)",
        1,
        "The OS places ONE buffer slot in kernel memory between the computer "
        "and the printer. The computer copies a page into the slot and is then "
        "free to prepare the next page while the printer prints. But it cannot "
        "copy the next page in until the printer has emptied the slot.",
        "Single buffering lets preparing (C) overlap with printing (T), so it "
        "beats no buffering. But the slot cannot be filled and emptied at the "
        "same time: each page costs about max(C, T) + M. Look for the W "
        "(blocked) and . (idle) gaps in the timeline.");
}

void demoDouble() {
    runSingleMode(
        "DEMO 5 - DOUBLE BUFFERING (two kernel buffer slots)",
        2,
        "The OS now uses TWO slots, B0 and B1. While the printer prints from "
        "one slot, the computer copies the next page into the other. Then they "
        "swap roles.",
        "Double buffering overlaps filling and emptying, so for steady work "
        "each page costs about max(C + M, T). The computer still blocks and the "
        "printer still starves after a slow diagram page, because only one page "
        "can be stored ahead. Cost: twice the buffer memory.");
}

void demoCircular() {
    int slots = readInt(fmt("\n  CLASS CHOICE: how many buffer slots should the ring have? (3-6, Enter = %d): ",
                            DEFAULT_CIRCULAR_SLOTS),
                        3, 6, DEFAULT_CIRCULAR_SLOTS);
    runSingleMode(
        fmt("DEMO 6 - CIRCULAR BUFFERING (a ring of %d reusable slots)", slots),
        slots,
        fmt("The OS uses a ring of %d slots. 'in' points to the next slot the "
            "computer will fill; 'out' points to the next slot the printer will "
            "print. Each pointer moves forward with (ptr + 1) mod %d, so slots are "
            "reused in a circle. On quick text pages the computer runs ahead and "
            "stores several pages; on a slow diagram page the printer keeps going "
            "from the stored pages.", slots, slots),
        "Circular buffering absorbs bursts: the extra slots act as a reserve, "
        "so the printer is never starved after a slow page and the computer "
        "finishes and is freed earlier. Cost: more memory and pointer "
        "management (in/out, wrap-around, full/empty checks). Beyond a point "
        "extra slots stop helping, because the printer itself is the limit.");
}

void demoCompareBuffering() {
    banner("DEMO 7 - COMPARING ALL BUFFERING APPROACHES (whole exam-week workload)");
    vector<Page> pages = wholeWorkload();
    int n = (int)pages.size();
    wrap(fmt("Same rules, bigger workload: all five documents (A=10, B=20, C=15, "
             "D=5, E=25 pages) sent as one stream of %d pages. Minimum possible time "
             "is limited by the printer: %d pages x %d tu = %d tu of printing.",
             n, n, PRINT_TIME_PER_PAGE, n * PRINT_TIME_PER_PAGE));
    predict("Will 6 slots be much faster than 4 slots?");
    bool saved = SHOW_EVENT_LOG;
    SHOW_EVENT_LOG = false;
    struct Row { string name; int slots; BufferResult r; };
    vector<Row> rows = {
        {"No buffering (lock-step)", 0, simulateUnbufferedLockstep(pages)},
        {"Single buffering", 1, simulateBuffered(pages, 1)},
        {"Double buffering", 2, simulateBuffered(pages, 2)},
        {"Circular buffering", 4, simulateBuffered(pages, 4)},
        {"Circular buffering", 6, simulateBuffered(pages, 6)},
    };
    BufferResult lost = simulateUnbufferedOverrun(pages);
    SHOW_EVENT_LOG = saved;
    out();
    out(fmt("  %-25s %6s %7s %8s %8s %8s %7s",
            "Approach", "Slots", "Memory", "Finish", "Computer", "Printer", "Printer"));
    out(fmt("  %-25s %6s %7s %8s %8s %8s %7s", "", "", "(KB)", "(tu)", "blocked", "idle", "util."));
    out("  " + repeatChar('-', 74));
    for (const Row& row : rows) {
        out(fmt("  %-25s %6d %7d %8d %8d %8d %7s",
                row.name.c_str(), row.slots, row.slots * PAGE_SIZE_KB, row.r.totalTime,
                row.r.computerBlocked, row.r.printerIdle,
                pct(row.r.printerBusy, row.r.totalTime).c_str()));
    }
    out();
    out(fmt("  No buffering, no waiting (overrun): %d of %d pages LOST.", (int)lost.lostPages.size(), n));
    explainConcept(fmt("Trade-off: each extra slot costs %d KB of kernel memory. Going from "
                "0 to 1 to 2 slots gives big gains; a ring of 4 keeps the printer "
                "busy over 99%% of the time; going from 4 to 6 slots only trims the "
                "computer's waiting and does not finish any sooner, because the "
                "printer itself is now the bottleneck. Choose the smallest buffer "
                "that absorbs the bursts in your workload.", PAGE_SIZE_KB));
    pause();
}

// =============================================================================
// PART B - SPOOLING VERSUS DEVICE RESERVATION
// =============================================================================
int spoolWriteTime(int pages) { return (pages + PAGES_SPOOLED_PER_TU - 1) / PAGES_SPOOLED_PER_TU; }

struct SpoolJob {
    int jobId;
    int studentIndex;
    string fileName;
    int sizeKb;
    int readyAt;

    SpoolJob(int id, int index) : jobId(id), studentIndex(index) {
        const Student& s = STUDENTS[index];
        fileName = fmt("job%02d_%s.prn", id, s.letter.c_str());
        sizeKb = s.pages * PAGE_SIZE_KB;
        readyAt = s.submitTime + spoolWriteTime(s.pages);
    }
    const Student& student() const { return STUDENTS[studentIndex]; }
    string shortName() const {
        return fmt("job%02d %s %dp", jobId, student().letter.c_str(), student().pages);
    }
};

struct StudentStats {
    int studentIndex;
    int freeAt = 0;        // when the student can carry on with other work
    int startedAt = -1;    // when the first page started printing
    int finishedAt = -1;   // when the last page came out
    bool cancelled = false;
    explicit StudentStats(int i) : studentIndex(i) {}
    const Student& student() const { return STUDENTS[studentIndex]; }
};

vector<StudentStats> makeStats() {
    vector<StudentStats> stats;
    for (int i = 0; i < (int)STUDENTS.size(); ++i) stats.emplace_back(i);
    return stats;
}

void printStudentTable(const vector<StudentStats>& stats, const string& title) {
    out();
    out("  " + title);
    out(fmt("  %-14s %6s %8s %10s %9s %10s %9s",
            "Student", "Pages", "Clicked", "Free to", "Blocked", "Printing", "Done at"));
    out(fmt("  %-14s %6s %8s %10s %9s %10s %9s", "", "", "Print", "work at", "(tu)", "started", "(tu)"));
    out("  " + repeatChar('-', 74));
    for (const StudentStats& st : stats) {
        const Student& s = st.student();
        if (st.cancelled) {
            out(fmt("  %-14s %6d %8d %10d %9d %10s %9s", s.name.c_str(), s.pages, s.submitTime,
                    st.freeAt, st.freeAt - s.submitTime, "cancelled", "-"));
        } else {
            out(fmt("  %-14s %6d %8d %10d %9d %10d %9d", s.name.c_str(), s.pages, s.submitTime,
                    st.freeAt, st.freeAt - s.submitTime, st.startedAt, st.finishedAt));
        }
    }
}

struct SpoolResult {
    vector<StudentStats> stats;
    int totalTime = 0;
    int printerBusy = 0;
    int printerIdle = 0;
    int peakDiskKb = 0;
    vector<string> outputTray;
};

/*
 SPOOLING. Each student's output is intercepted by the OS and written to
 a separate spool file on DISK. When the file is complete the job joins
 the print queue and the student is free. A background spooler (daemon)
 sends one complete job at a time to the printer, in FIFO order.
*/
SpoolResult simulateSpooling(const string& cancelLetter = "") {
    SpoolResult r;
    r.stats = makeStats();
    vector<StudentStats>& stats = r.stats;
    vector<SpoolJob> jobs;
    for (int i = 0; i < (int)STUDENTS.size(); ++i) jobs.emplace_back(i + 1, i);
    deque<int> queue;            // the print queue (spool queue): FIFO of complete jobs
    int diskKb = 0;
    int current = -1;            // index of the job being printed
    int pageLeft = 0;
    int pagesDone = 0;
    int remainingJobs = (int)jobs.size();
    int t = 0;

    auto showQueue = [&]() {
        if (!SHOW_EVENT_LOG) return;
        vector<string> parts;
        for (int j : queue) parts.push_back("[" + jobs[j].shortName() + "]");
        out("         Print queue: " + (queue.empty() ? string("(empty)") : join(parts, "  ")));
    };

    while (remainingJobs > 0) {
        // 1. students click Print: output is redirected to a spool file
        for (const SpoolJob& j : jobs) {
            if (j.student().submitTime == t) {
                logEvent(t, fmt("%s clicks Print (%d pages) -> spooler writes %s to disk",
                                j.student().name.c_str(), j.student().pages, j.fileName.c_str()));
                diskKb += j.sizeKb;
                r.peakDiskKb = max(r.peakDiskKb, diskKb);
            }
        }
        // 2. spool files that are complete join the queue; students are free
        for (int i = 0; i < (int)jobs.size(); ++i) {
            const SpoolJob& j = jobs[i];
            if (j.readyAt == t) {
                stats[i].freeAt = t;
                queue.push_back(i);
                logEvent(t, fmt("%s complete (%d KB). %s is FREE to leave.",
                                j.fileName.c_str(), j.sizeKb, j.student().name.c_str()));
                showQueue();
                if (cancelLetter == j.student().letter) {
                    // Spooler control interface: the job is removed before printing
                    queue.erase(find(queue.begin(), queue.end(), i));
                    diskKb -= j.sizeKb;
                    stats[i].cancelled = true;
                    remainingJobs--;
                    logEvent(t, fmt("CANCEL request: %s removed from the queue (lprm).", j.fileName.c_str()));
                    showQueue();
                }
            }
        }
        // 3. the spooler feeds the printer one COMPLETE job at a time
        if (current == -1 && !queue.empty()) {
            current = queue.front();
            queue.pop_front();
            pageLeft = PRINT_TIME_PER_PAGE;
            pagesDone = 0;
            stats[current].startedAt = t;
            logEvent(t, fmt("Spooler sends %s (%s) to the printer",
                            jobs[current].fileName.c_str(), jobs[current].student().name.c_str()));
            showQueue();
        }
        // 4. one tu passes
        if (current != -1) r.printerBusy++;
        else r.printerIdle++;
        t++;
        if (current != -1) {
            pageLeft--;
            if (pageLeft == 0) {
                pagesDone++;
                const SpoolJob& j = jobs[current];
                r.outputTray.push_back(j.student().letter + to_string(pagesDone));
                if (pagesDone == j.student().pages) {
                    stats[current].finishedAt = t;
                    diskKb -= j.sizeKb;
                    logEvent(t, fmt("Printer finished %s (%d pages). Spool file deleted.",
                                    j.fileName.c_str(), pagesDone));
                    current = -1;
                    remainingJobs--;
                } else {
                    pageLeft = PRINT_TIME_PER_PAGE;
                }
            }
        }
    }
    r.totalTime = t;
    return r;
}

struct ReservationResult {
    vector<StudentStats> stats;
    int totalTime = 0;
    int printerBusy = 0;
    int printerIdleHeld = 0;   // printer reserved but not printing
    int printerIdleFree = 0;   // nobody wanted it
    bool starved = false;
};

enum OwnerPhase { PH_NONE, PH_PRINT, PH_PAUSE, PH_HOLD, PH_CRASHED };

/*
 DEVICE RESERVATION. A student's program must call allocate(printer)
 and gets EXCLUSIVE use of it until it calls release(printer). Others
 who call allocate() meanwhile are BLOCKED in a FIFO wait queue.
 The owner sends its pages straight to the printer, so it stays blocked
 for the whole print, and any pause it makes leaves the printer idle.
 If crashLetter is set, that student's program crashes before calling
 release(): nobody else can ever get the printer (STARVATION).
*/
ReservationResult simulateReservation(const string& crashLetter = "") {
    ReservationResult r;
    r.stats = makeStats();
    vector<StudentStats>& stats = r.stats;
    int owner = -1;
    deque<int> waitQueue;
    int pagesDone = 0;
    OwnerPhase phase = PH_NONE;
    int left = 0;
    int finished = 0;
    int t = 0;
    int n = (int)STUDENTS.size();

    auto showWait = [&]() {
        if (!SHOW_EVENT_LOG) return;
        vector<string> names;
        for (int i : waitQueue) names.push_back(STUDENTS[i].name);
        out("         Waiting for the printer: " + (waitQueue.empty() ? string("(none)") : join(names, ", ")));
    };

    auto grant = [&](int i) {
        owner = i;
        pagesDone = 0;
        phase = PH_PRINT;
        left = PRINT_TIME_PER_PAGE;
        stats[i].startedAt = t;
        logEvent(t, "allocate(printer) GRANTED to " + STUDENTS[i].name);
    };

    while (finished < n) {
        if (!crashLetter.empty() && t >= WATCHDOG_LIMIT) {
            r.starved = true;
            logEvent(t, "WATCHDOG: no progress for a long time - simulation stopped.");
            break;
        }
        // 1. students call allocate(printer)
        for (int i = 0; i < n; ++i) {
            const Student& s = STUDENTS[i];
            if (s.submitTime == t) {
                if (owner == -1 && waitQueue.empty()) {
                    logEvent(t, s.name + " calls allocate(printer)");
                    grant(i);
                } else {
                    waitQueue.push_back(i);
                    string holder = owner != -1 ? STUDENTS[owner].name : "-";
                    logEvent(t, fmt("%s calls allocate(printer) -> BUSY (held by %s): BLOCKED",
                                    s.name.c_str(), holder.c_str()));
                    showWait();
                }
            }
        }
        // 2. account for this tu
        if (owner == -1) r.printerIdleFree++;
        else if (phase == PH_PRINT) r.printerBusy++;
        else r.printerIdleHeld++;
        // 3. one tu passes
        t++;
        if (owner != -1 && phase != PH_CRASHED) {
            left--;
            if (left == 0) {
                const Student& s = STUDENTS[owner];
                bool release = false;
                if (phase == PH_PRINT) {
                    pagesDone++;
                    if (pagesDone == s.pages) {
                        stats[owner].finishedAt = t;
                        if (s.letter == crashLetter) {
                            phase = PH_CRASHED;
                            logEvent(t, s.name + "'s program CRASHES before calling release()!");
                            logEvent(t, "The printer stays reserved by a dead process.");
                        } else if (s.holdAfterFinish > 0) {
                            phase = PH_HOLD;
                            left = s.holdAfterFinish;
                            logEvent(t, s.name + " finished printing but keeps the printer reserved");
                            logEvent(t, fmt("  (reviewing pages for %d tu) -> printer IDLE but LOCKED",
                                            s.holdAfterFinish));
                        } else {
                            release = true;
                        }
                    } else if (pagesDone == s.pauseAfterPage) {
                        phase = PH_PAUSE;
                        left = s.pauseLength;
                        logEvent(t, fmt("%s pauses after page %d to check margins (%d tu)",
                                        s.name.c_str(), pagesDone, s.pauseLength));
                        logEvent(t, "  -> printer IDLE but still RESERVED; others keep waiting");
                    } else {
                        left = PRINT_TIME_PER_PAGE;
                    }
                } else if (phase == PH_PAUSE) {
                    phase = PH_PRINT;
                    left = PRINT_TIME_PER_PAGE;
                    logEvent(t, s.name + " resumes printing");
                } else if (phase == PH_HOLD) {
                    release = true;
                }
                if (release) {
                    stats[owner].freeAt = t;
                    logEvent(t, fmt("%s calls release(printer) after %d pages", s.name.c_str(), s.pages));
                    finished++;
                    owner = -1;
                    if (!waitQueue.empty()) {
                        int next = waitQueue.front();
                        waitQueue.pop_front();
                        grant(next);
                        showWait();
                    }
                }
            }
        }
    }
    r.totalTime = t;
    return r;
}

void demoNoCoordination() {
    banner("DEMO 8 - WHAT GOES WRONG WITHOUT SPOOLING OR RESERVATION");
    wrap("All five students print at the same moment and every program writes "
         "its pages straight to the shared printer. The printer is a "
         "NON-SHARABLE device: it simply prints pages in the order they arrive, "
         "taking one page from each program in turn.");
    vector<string> tray;
    int n = (int)STUDENTS.size();
    vector<int> remaining, printed(n, 0);
    int total = 0;
    for (const Student& s : STUDENTS) { remaining.push_back(s.pages); total += s.pages; }
    while (total > 0) {
        for (int i = 0; i < n; ++i) {
            if (remaining[i] > 0) {
                printed[i]++;
                remaining[i]--;
                total--;
                tray.push_back(STUDENTS[i].letter + to_string(printed[i]));
            }
        }
    }
    out();
    out(fmt("  Output tray, sheet by sheet (first 30 of %d sheets):", (int)tray.size()));
    for (int start = 0; start < 30; start += 10) {
        vector<string> cells;
        for (int k = start; k < start + 10 && k < (int)tray.size(); ++k) cells.push_back(fmt("%-3s", tray[k].c_str()));
        out(fmt("    sheets %2d-%2d: %s", start + 1, start + 10, join(cells, " ").c_str()));
    }
    out();
    for (const Student& s : STUDENTS) {
        int first = -1, last = -1;
        for (int i = 0; i < (int)tray.size(); ++i) {
            if (tray[i][0] == s.letter[0]) {
                if (first == -1) first = i + 1;
                last = i + 1;
            }
        }
        out(fmt("  %-12s %2d pages scattered between sheet %2d and sheet %2d",
                s.name.c_str(), s.pages, first, last));
    }
    explainConcept("Interleaved output. A printer cannot usefully mix the I/O streams "
            "of several programs. The OS must coordinate access - either by "
            "SPOOLING the output or by giving one process exclusive use of "
            "the device (DEVICE RESERVATION).");
    pause();
}

void demoReservation() {
    banner("DEMO 9 - DEVICE RESERVATION (allocate / release)");
    wrap("Now the OS gives the printer to ONE student at a time. A program "
         "must call allocate(printer) before printing and release(printer) "
         "afterwards. Students who ask while the printer is reserved are "
         "blocked in a wait queue. Student B pauses for 12 tu after page 10 "
         "to check the margins; Student D keeps the printer reserved for 8 tu "
         "to review the pages before releasing it.");
    predict("While Student B is checking margins, what is the printer doing?");
    out();
    ReservationResult r = simulateReservation();
    printStudentTable(r.stats, "RESULT - Device reservation");
    out();
    out(fmt("  All jobs finished at t = %d tu", r.totalTime));
    out(fmt("  Printer printing ............. %d tu (%s)", r.printerBusy, pct(r.printerBusy, r.totalTime).c_str()));
    out(fmt("  Printer IDLE but reserved .... %d tu  <- wasted: others were waiting", r.printerIdleHeld));
    int totalBlocked = 0;
    for (const StudentStats& st : r.stats) totalBlocked += st.freeAt - st.student().submitTime;
    out(fmt("  Total student time blocked ... %d tu", totalBlocked));
    explainConcept("Device reservation gives exclusive access, so output is never "
            "mixed. But each student is blocked while waiting AND while their "
            "own job prints, and the device is wasted whenever its owner pauses.");
    pause();

    section("9(b) What goes wrong: a program crashes before calling release()");
    r = simulateReservation("A");
    out();
    vector<string> waiting;
    for (const StudentStats& st : r.stats)
        if (st.finishedAt == -1) waiting.push_back(st.student().name);
    out(fmt("  After %d tu: %d student(s) still waiting, 0 of their pages printed:",
            r.totalTime, (int)waiting.size()));
    out("    " + join(waiting, ", "));
    explainConcept("STARVATION: a process that never releases a reserved device "
            "blocks everyone else until an administrator steps in. With "
            "spooling this cannot happen: once a job is on disk, a crashed "
            "student program does not affect the printer.");
    pause();

    section("9(c) What goes wrong: DEADLOCK with two reserved devices");
    wrap("The printing centre also has one scanner. Student A needs the "
         "printer AND the scanner (to scan a signed cover page). Student B "
         "needs the scanner AND the printer. Both use reservation.");
    out();
    map<string, string> owner = {{"printer", ""}, {"scanner", ""}};
    map<string, string> waitsFor;
    struct Step { int t; string who; string device; };
    vector<Step> steps = {{0, "A", "printer"}, {1, "B", "scanner"}, {2, "A", "scanner"}, {3, "B", "printer"}};
    map<string, string> names = {{"A", STUDENTS[0].name}, {"B", STUDENTS[1].name}};
    for (const Step& st : steps) {
        if (owner[st.device].empty()) {
            owner[st.device] = st.who;
            logEvent(st.t, fmt("%s allocate(%s) -> GRANTED", names[st.who].c_str(), st.device.c_str()));
        } else {
            waitsFor[st.who] = owner[st.device];
            logEvent(st.t, fmt("%s allocate(%s) -> BUSY (held by %s): BLOCKED",
                               names[st.who].c_str(), st.device.c_str(), names[owner[st.device]].c_str()));
        }
    }
    out();
    if (waitsFor["A"] == "B" && waitsFor["B"] == "A") {
        out(fmt("  Wait-for cycle: %s -> waits for -> %s -> waits for -> %s",
                names["A"].c_str(), names["B"].c_str(), names["A"].c_str()));
        out("  DEADLOCK: neither student can ever continue.");
    }
    explainConcept("Reserving several dedicated devices allows hold-and-wait, and "
            "with circular waiting the processes deadlock. If the printer is "
            "spooled instead, Student B's print request simply goes to the "
            "spool queue, B finishes and releases the scanner, and A continues.");
    pause();
}

void demoSpooling() {
    banner("DEMO 10 - SPOOLING WITH A SPOOLER AND PRINT QUEUE");
    wrap(fmt("Now the OS intercepts every student's printer output and writes it "
             "to a separate spool file on DISK (secondary storage). When a spool "
             "file is complete, the job joins the print queue and the student is "
             "free. A background spooler sends one complete job at a time to the "
             "printer. Writing to disk is fast: %d pages per tu.", PAGES_SPOOLED_PER_TU));
    out();
    out("  CLASS CHOICE: the spooler lets users manage the queue.");
    string choice = trim(readLine("  Cancel a job before it prints? Enter A-E, or press Enter for none: "));
    for (char& c : choice) c = (char)toupper((unsigned char)c);
    if (!(choice == "A" || choice == "B" || choice == "C" || choice == "D" || choice == "E")) choice = "";
    if (!choice.empty()) out("  -> The class cancels " + STUDENTS[choice[0] - 'A'].name + "'s job.");
    out();
    SpoolResult r = simulateSpooling(choice);
    printStudentTable(r.stats, "RESULT - Spooling");
    out();
    out(fmt("  All jobs finished at t = %d tu", r.totalTime));
    out(fmt("  Printer printing ............. %d tu (%s)", r.printerBusy, pct(r.printerBusy, r.totalTime).c_str()));
    out(fmt("  Printer idle ................. %d tu (only before the first job)", r.printerIdle));
    out(fmt("  Peak disk space in spool area  %d KB", r.peakDiskKb));
    const vector<string>& tray = r.outputTray;
    vector<string> head(tray.begin(), tray.begin() + min<size_t>(4, tray.size()));
    vector<string> tail(tray.end() - min<size_t>(3, tray.size()), tray.end());
    out(fmt("  Output tray: %s ... %s (each job printed as one complete block)",
            join(head, " ").c_str(), join(tail, " ").c_str()));
    explainConcept("Spooling: output for a device that cannot accept interleaved data "
            "is held in spool files on disk; the spooler prints them one at a "
            "time. Every student is free within a few tu, the printer runs back "
            "to back, and jobs can be listed, reordered or cancelled. Cost: disk "
            "space, a spooler process, and printing cannot start until a job's "
            "spool file is complete.");
    pause();
}

void demoCompareSpooling() {
    banner("DEMO 11 - SPOOLING vs DEVICE RESERVATION (same five students)");
    bool saved = SHOW_EVENT_LOG;
    SHOW_EVENT_LOG = false;
    SpoolResult spool = simulateSpooling("");
    ReservationResult reserve = simulateReservation();
    SHOW_EVENT_LOG = saved;
    out();
    out(fmt("  %-14s %22s %22s", "", "Time blocked (tu)", "Document done at (tu)"));
    out(fmt("  %-14s %11s %10s %11s %10s", "Student", "Reserve", "Spool", "Reserve", "Spool"));
    out("  " + repeatChar('-', 60));
    int rb = 0, sb = 0;
    for (size_t i = 0; i < reserve.stats.size(); ++i) {
        const StudentStats& a = reserve.stats[i];
        const StudentStats& b = spool.stats[i];
        const Student& s = a.student();
        out(fmt("  %-14s %11d %10d %11d %10d", s.name.c_str(), a.freeAt - s.submitTime,
                b.freeAt - s.submitTime, a.finishedAt, b.finishedAt));
        rb += a.freeAt - s.submitTime;
        sb += b.freeAt - s.submitTime;
    }
    out("  " + repeatChar('-', 60));
    out(fmt("  %-14s %11d %10d", "TOTAL blocked", rb, sb));
    out();
    out(fmt("  %-36s %12s %12s", "Measure", "Reservation", "Spooling"));
    out("  " + repeatChar('-', 62));
    out(fmt("  %-36s %12d %12d", "All jobs finished at (tu)", reserve.totalTime, spool.totalTime));
    out(fmt("  %-36s %12s %12s", "Printer utilisation", pct(reserve.printerBusy, reserve.totalTime).c_str(),
            pct(spool.printerBusy, spool.totalTime).c_str()));
    out(fmt("  %-36s %12d %12d", "Printer idle while reserved (tu)", reserve.printerIdleHeld, 0));
    out(fmt("  %-36s %12d %12d", "Disk space needed (peak KB)", 0, spool.peakDiskKb));
    out(fmt("  %-36s %12s %12s", "Can users cancel / reorder jobs?", "no", "yes"));
    out(fmt("  %-36s %12s %12s", "Starvation / deadlock risk", "yes", "no"));
    explainConcept("Both approaches stop output from being mixed. Spooling wins for a "
            "busy shared printer: students are freed almost at once and the "
            "printer never waits for a hesitating owner. It costs disk space "
            "and a spooler, and Student A's document finished 1 tu LATER because "
            "it had to be written to disk first. The printer still prints at the "
            "same speed - spooling removes waiting and idle gaps, it does not "
            "make the printer faster. Reservation suits devices that need an "
            "interactive, uninterrupted session (e.g. a tape drive or lab instrument).");
    pause();
}

// =============================================================================
// MENU
// =============================================================================
void showAssumptions() {
    banner("SIMULATION ASSUMPTIONS");
    out("  Time is simulated in time units (tu). Real devices differ by much");
    out("  larger ratios; these values are scaled so the class can follow.");
    out();
    out("  Workload (the same for every demo):");
    for (const Student& s : STUDENTS)
        out(fmt("    %-12s %2d pages, clicks Print at t=%d", s.name.c_str(), s.pages, s.submitTime));
    out();
    out("  Buffering:");
    out(fmt("    Prepare a text page (C) ............... %d tu", TEXT_PAGE_RENDER));
    out(fmt("    Prepare a diagram page (every 5th) .... %d tu", DIAGRAM_PAGE_RENDER));
    out(fmt("    Copy one page into a buffer slot (M) .. %d tu", COPY_TIME_PER_PAGE));
    out(fmt("    Printer prints one page (T) ........... %d tu", PRINT_TIME_PER_PAGE));
    out(fmt("    One page = one buffer slot ............ %d KB", PAGE_SIZE_KB));
    out(fmt("    Transfer-size demo: %d KB pieces, %d KB blocks, %d tu set-up per transfer",
            CHUNK_KB, PAGE_SIZE_KB, DEVICE_SETUP_TIME));
    out();
    out("  Spooling / device reservation:");
    out(fmt("    Writing a spool file to disk .......... %d pages per tu", PAGES_SPOOLED_PER_TU));
    out("    Spooler scheduling .................... FIFO, one complete job at a time");
    out("    Student B (reservation) ............... pauses 12 tu after page 10");
    out("    Student D (reservation) ............... holds printer 8 tu before release");
    pause();
}

void renameStudents() {
    banner("CLASS PARTICIPATION - NAME THE FIVE STUDENTS");
    out("  Enter a classmate's name for each student (max 12 characters).");
    out("  Press Enter to keep the current name.");
    for (Student& s : STUDENTS) {
        string text = trim(readLine(fmt("    %s (%d pages): ", s.name.c_str(), s.pages)));
        if (!text.empty()) s.name = text.substr(0, 12);
    }
    out();
    vector<string> names;
    for (const Student& s : STUDENTS) names.push_back(s.name);
    out("  Names saved: " + join(names, ", "));
}

void guidedDemo() {
    banner("GUIDED DEMONSTRATION - UNIVERSITY PRINTING CENTRE, EXAM WEEK");
    wrap("Five students submit documents almost at the same time. The "
         "computers produce print data much faster than the one shared printer "
         "can print it. We will first follow the DATA of one job (buffering), "
         "then follow the JOBS of all five students (spooling).");
    bool saved = SHOW_EVENT_LOG;
    SHOW_EVENT_LOG = false;            // timelines only, to fit in 10 minutes
    pause();
    demoNoBuffering();
    demoTransferSize();
    demoCopySemantics();
    demoSingle();
    demoDouble();
    demoCircular();
    demoCompareBuffering();
    SHOW_EVENT_LOG = true;             // event log on for the spooling story
    demoNoCoordination();
    demoReservation();
    demoSpooling();
    demoCompareSpooling();
    SHOW_EVENT_LOG = saved;
    banner("SUMMARY");
    wrap("BUFFERING manages the flow of DATA between two components that work at "
         "different speeds or sizes, using memory (RAM). SPOOLING manages complete "
         "JOBS from many processes for one non-sharable device, using a queue of "
         "files on disk. They solve different problems and are used together: the "
         "spooler itself uses buffers to move each job to the printer.");
}

int main() {
    banner("GROUP 8 - BUFFERING AND SPOOLING: UNIVERSITY PRINTING CENTRE");
    out("  Charbel Ngeti 221441 | Russel Muiga 220510 | Alvin Otanga 220267");
    out("  Handel Songoro 220337 | Jesse Kyongo 180904 | Abigael Wambui 183891");
    while (true) {
        out();
        out("  PART A - BUFFERING (moving data from one computer to the printer)");
        out("    1. Why buffer? Speed mismatch with NO buffering");
        out("    2. Why buffer? Transfer-size mismatch");
        out("    3. Why buffer? Copy semantics");
        out("    4. Single buffering");
        out("    5. Double buffering");
        out("    6. Circular buffering (class chooses the number of slots)");
        out("    7. Compare all buffering approaches (whole workload)");
        out("  PART B - SPOOLING (many students sharing one printer)");
        out("    8. No coordination: everyone prints directly at once");
        out("    9. Device reservation (allocate / release) and what goes wrong");
        out("   10. Spooling with a spooler and print queue");
        out("   11. Compare spooling vs device reservation");
        out("  OTHER");
        out("   12. Guided demonstration (presentation order, about 10 minutes)");
        out("   13. Show simulation assumptions");
        out("   14. Class participation: enter classmates' names for the students");
        out(fmt("   15. Toggle detailed event log (currently %s)", SHOW_EVENT_LOG ? "ON" : "OFF"));
        out("    0. Exit");
        int choice = readInt("\n  Choose an option: ", 0, 15, -1);
        switch (choice) {
            case -1: break;
            case 0: out("  Goodbye from Group 8!"); return 0;
            case 1: demoNoBuffering(); break;
            case 2: demoTransferSize(); break;
            case 3: demoCopySemantics(); break;
            case 4: demoSingle(); break;
            case 5: demoDouble(); break;
            case 6: demoCircular(); break;
            case 7: demoCompareBuffering(); break;
            case 8: demoNoCoordination(); break;
            case 9: demoReservation(); break;
            case 10: demoSpooling(); break;
            case 11: demoCompareSpooling(); break;
            case 12: guidedDemo(); break;
            case 13: showAssumptions(); break;
            case 14: renameStudents(); break;
            case 15:
                SHOW_EVENT_LOG = !SHOW_EVENT_LOG;
                out(fmt("  Detailed event log is now %s.", SHOW_EVENT_LOG ? "ON" : "OFF"));
                break;
        }
    }
}
