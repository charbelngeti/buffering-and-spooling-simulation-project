#!/usr/bin/env python3
"""
===============================================================================
 OPERATING SYSTEMS - GROUP 8 - BUFFERING AND SPOOLING SIMULATION (Python 3)
 Scenario: University Printing Centre During Exam Week
===============================================================================
 Group members: Charbel Ngeti (221441), Russel Muiga (220510),
                Alvin Otanga (220267), Handel Songoro (220337),
                Jesse Kyongo (180904)

 This program is a line-for-line equivalent of buffering_spooling.cpp.
 Both programs use the same workload, the same rules and the same simulated
 clock, so they print the same results.

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
"""

import sys

# =============================================================================
# SIMULATION ASSUMPTIONS (identical in the C++ version)
# =============================================================================
PRINT_TIME_PER_PAGE = 4    # T: tu for the printer to print one page
COPY_TIME_PER_PAGE = 1     # M: tu to copy one page into a kernel buffer slot
TEXT_PAGE_RENDER = 1       # C: tu for the computer to prepare a text page
DIAGRAM_PAGE_RENDER = 8    # C: tu to prepare a page that contains a diagram
DIAGRAM_EVERY = 5          # every 5th page of a document contains a diagram
PAGE_SIZE_KB = 4           # one page of print data = one 4 KB block
DEFAULT_CIRCULAR_SLOTS = 4

# Transfer-size mismatch demonstration
CHUNK_KB = 1               # the application hands data to the OS in 1 KB pieces
DEVICE_SETUP_TIME = 2      # fixed cost of starting ONE device transfer
TIME_PER_KB = 1            # cost of moving 1 KB to the printer

# Spooling / device reservation demonstration
PAGES_SPOOLED_PER_TU = 10  # writing a spool file to disk: 10 pages per tu
WATCHDOG_LIMIT = 200       # starvation demo stops after this many tu

LINE_WIDTH = 78

# =============================================================================
# THE WORKLOAD: five students submit documents almost at the same time
# =============================================================================
class Student:
    def __init__(self, letter, name, pages, submit_time, pause_after_page,
                 pause_length, hold_after_finish):
        self.letter = letter              # short label used in page names (A1, B7..)
        self.name = name                  # display name (classmates can rename)
        self.pages = pages                # document length in pages
        self.submit_time = submit_time    # when the student clicks "Print"
        # Behaviour that only matters under DEVICE RESERVATION:
        self.pause_after_page = pause_after_page    # stops to check printout...
        self.pause_length = pause_length            # ...for this many tu
        self.hold_after_finish = hold_after_finish  # keeps printer before release()


def make_students():
    return [
        Student("A", "Student A", 10, 0, 0, 0, 0),
        Student("B", "Student B", 20, 1, 10, 12, 0),   # checks margins after page 10
        Student("C", "Student C", 15, 2, 0, 0, 0),
        Student("D", "Student D", 5, 3, 0, 0, 8),      # reviews pages before release
        Student("E", "Student E", 25, 4, 0, 0, 0),
    ]


STUDENTS = make_students()
SHOW_EVENT_LOG = True


def render_time(page_number):
    """Time the computer needs to prepare page n (1-based) of a document."""
    if page_number % DIAGRAM_EVERY == 0:
        return DIAGRAM_PAGE_RENDER
    return TEXT_PAGE_RENDER


def document_pages(student):
    """A document as a list of (label, render_time) e.g. ("A5", 8)."""
    return [(student.letter + str(p), render_time(p))
            for p in range(1, student.pages + 1)]


def whole_workload():
    """All five documents as one continuous stream of 75 pages."""
    pages = []
    for s in STUDENTS:
        pages.extend(document_pages(s))
    return pages


# =============================================================================
# CONSOLE HELPERS (kept identical to the C++ version so outputs match)
# =============================================================================
def out(text=""):
    print(text)


def wrap(text, indent=2):
    """Greedy word wrap (same algorithm in C++)."""
    words = text.split(" ")
    prefix = " " * indent
    line = ""
    for w in words:
        if w == "":
            continue
        if line == "":
            line = w
        elif len(prefix) + len(line) + 1 + len(w) <= LINE_WIDTH:
            line = line + " " + w
        else:
            out(prefix + line)
            line = w
    if line != "":
        out(prefix + line)


def banner(title):
    out()
    out("=" * LINE_WIDTH)
    out("  " + title)
    out("=" * LINE_WIDTH)


def section(title):
    out()
    out("-" * LINE_WIDTH)
    out("  " + title)
    out("-" * LINE_WIDTH)


def explain_concept(text):
    """Highlighted box linking what happens on screen to the OS concept."""
    out()
    out("  >> OS CONCEPT: ")
    wrap(text, 5)


def read_line(prompt):
    try:
        return input(prompt)
    except EOFError:
        out()
        out("(Input ended - exiting.)")
        sys.exit(0)


def pause():
    read_line("\n  [Press Enter to continue] ")


def predict(question):
    """Class participation: ask the audience to predict before revealing."""
    out()
    out("  ?? CLASS PREDICTION: " + question)
    read_line("  [Discuss, then press Enter to reveal] ")


def read_int(prompt, low, high, default):
    while True:
        text = read_line(prompt).strip()
        if text == "":
            return default
        if text.isdigit() and low <= int(text) <= high:
            return int(text)
        out("  Please enter a whole number from " + str(low) + " to " + str(high) + ".")


def log(t, message):
    if SHOW_EVENT_LOG:
        out("  t=%3d  %s" % (t, message))


def pct(part, whole):
    if whole == 0:
        return "0.0%"
    return "%.1f%%" % (100.0 * part / whole)


def draw_timeline(computer, printer):
    """Gantt-style chart: one character per tu."""
    length = len(computer)
    ruler = [" "] * length
    for i in range(0, length, 10):
        label = str(i)
        for j, ch in enumerate(label):
            if i + j < length:
                ruler[i + j] = ch
    out()
    out("  Time (tu) |" + "".join(ruler))
    out("  Computer  |" + "".join(computer))
    out("  Printer   |" + "".join(printer))
    out("  Key: R=preparing page  C=copying into buffer  W=WAITING (blocked)")
    out("       -=finished, free  #=printer printing     .=printer IDLE")


# =============================================================================
# PART A - BUFFERING
# =============================================================================
EMPTY, FILLING, FULL, PRINTING = 0, 1, 2, 3


class RingBuffer:
    """
    A set of N buffer slots in KERNEL memory, used as a ring.
      N = 1 -> single buffering
      N = 2 -> double buffering (one slot fills while the other drains)
      N > 2 -> circular buffering
    The producer always fills the slot at 'in_ptr'; the consumer (printer)
    always drains the slot at 'out_ptr'. Both pointers wrap around with
    modulo arithmetic: ptr = (ptr + 1) % N.
    (Because every slot keeps its own state, all N slots can be used. A ring
    that only stores the two pointers must leave one slot empty to tell
    'full' apart from 'empty'.)
    """

    def __init__(self, num_slots):
        self.n = num_slots
        self.page = [""] * num_slots
        self.state = [EMPTY] * num_slots
        self.in_ptr = 0
        self.out_ptr = 0

    # --- producer side -------------------------------------------------------
    def can_fill(self):
        return self.state[self.in_ptr] == EMPTY

    def start_fill(self, label):
        self.page[self.in_ptr] = label
        self.state[self.in_ptr] = FILLING
        return self.in_ptr

    def finish_fill(self):
        slot = self.in_ptr
        self.state[slot] = FULL
        self.in_ptr = (self.in_ptr + 1) % self.n
        return slot

    # --- consumer side -------------------------------------------------------
    def can_drain(self):
        return self.state[self.out_ptr] == FULL

    def start_drain(self):
        slot = self.out_ptr
        self.state[slot] = PRINTING
        return slot

    def finish_drain(self):
        slot = self.out_ptr
        self.state[slot] = EMPTY
        self.page[slot] = ""
        self.out_ptr = (self.out_ptr + 1) % self.n
        return slot

    def describe(self):
        text = ""
        for i in range(self.n):
            if self.state[i] == EMPTY:
                text += "[....]"
            else:
                flag = {FILLING: "+", FULL: " ", PRINTING: "*"}[self.state[i]]
                text += "[%-3s%s]" % (self.page[i], flag)
        if self.n >= 3:
            text += " in:%d out:%d" % (self.in_ptr, self.out_ptr)
        return text


class BufferResult:
    def __init__(self):
        self.total_time = 0
        self.computer_free_at = 0
        self.computer_blocked = 0
        self.printer_idle = 0
        self.printer_busy = 0
        self.pages_printed = 0
        self.lost_pages = []
        self.computer_line = []
        self.printer_line = []


def simulate_buffered(pages, num_slots):
    """
    Producer-consumer with N kernel buffer slots.
    Producer (computer): prepare page (C) -> wait for an EMPTY slot ->
                         copy page into slot (M) -> next page.
    Consumer (printer):  take the oldest FULL slot -> print it (T) -> slot
                         becomes EMPTY again.
    """
    buf = RingBuffer(num_slots)
    r = BufferResult()
    n = len(pages)
    t = 0
    index = 0                        # page the computer is working on
    prod_state = "RENDER"
    prod_left = pages[0][1]
    printer_page = ""
    printer_left = 0
    blocked_reported = False
    idle_reported = False

    def show(message):
        log(t, "%-36s %s" % (message, buf.describe()))

    while r.pages_printed < n:
        # ---- 1. decisions at the start of tick t ---------------------------
        if prod_state == "READY":
            if buf.can_fill():
                slot = buf.start_fill(pages[index][0])
                prod_state = "COPY"
                prod_left = COPY_TIME_PER_PAGE
                blocked_reported = False
            elif not blocked_reported:
                show("Computer BLOCKED: no free slot")
                blocked_reported = True
        if printer_page == "":
            if buf.can_drain():
                slot = buf.start_drain()
                printer_page = buf.page[slot]
                printer_left = PRINT_TIME_PER_PAGE
                idle_reported = False
                show("Printer starts %s (slot B%d)" % (printer_page, slot))
            elif not idle_reported:
                show("Printer IDLE: buffer is empty")
                idle_reported = True

        # ---- 2. record what each component does during this tick ----------
        r.computer_line.append({"RENDER": "R", "COPY": "C", "READY": "W",
                                "DONE": "-"}[prod_state])
        if prod_state == "READY":
            r.computer_blocked += 1
        if printer_page != "":
            r.printer_line.append("#")
            r.printer_busy += 1
        else:
            r.printer_line.append(".")
            r.printer_idle += 1

        # ---- 3. one tu passes; apply completions at time t+1 --------------
        t += 1
        if prod_state in ("RENDER", "COPY"):
            prod_left -= 1
        if printer_page != "":
            printer_left -= 1

        if prod_state == "RENDER" and prod_left == 0:
            prod_state = "READY"
        elif prod_state == "COPY" and prod_left == 0:
            slot = buf.finish_fill()
            show("%s copied into slot B%d" % (pages[index][0], slot))
            index += 1
            if index < n:
                prod_state = "RENDER"
                prod_left = pages[index][1]
            else:
                prod_state = "DONE"
                r.computer_free_at = t
                show("Computer done: free for other work")
        if printer_page != "" and printer_left == 0:
            buf.finish_drain()
            r.pages_printed += 1
            printer_page = ""

    r.total_time = t
    return r


def simulate_unbuffered_lockstep(pages):
    """
    NO BUFFERING (with a handshake): the printer reads each page straight
    from the application's own memory, so the program must stop and wait
    until the slow printer has finished with that page.
    """
    r = BufferResult()
    n = len(pages)
    t = 0
    index = 0
    prod_state = "RENDER"
    prod_left = pages[0][1]
    printer_left = 0
    while r.pages_printed < n:
        if prod_state == "READY":
            prod_state = "BLOCKED"
            printer_left = PRINT_TIME_PER_PAGE
            log(t, "Printer prints %s straight from program memory -> computer BLOCKED"
                % pages[index][0])
        busy = prod_state == "BLOCKED"
        r.computer_line.append({"RENDER": "R", "BLOCKED": "W", "DONE": "-"}[prod_state])
        r.printer_line.append("#" if busy else ".")
        if busy:
            r.computer_blocked += 1
            r.printer_busy += 1
        else:
            r.printer_idle += 1
        t += 1
        if prod_state == "RENDER":
            prod_left -= 1
            if prod_left == 0:
                prod_state = "READY"
        elif prod_state == "BLOCKED":
            printer_left -= 1
            if printer_left == 0:
                r.pages_printed += 1
                index += 1
                if index < n:
                    prod_state = "RENDER"
                    prod_left = pages[index][1]
                else:
                    prod_state = "DONE"
                    r.computer_free_at = t
    r.total_time = t
    return r


def simulate_unbuffered_overrun(pages):
    """
    NO BUFFERING and NO waiting: the fast computer pushes each page the
    moment it is ready. The printer can only hold the page it is printing,
    so any page that arrives while it is busy is LOST (data overrun).
    """
    r = BufferResult()
    n = len(pages)
    t = 0
    printer_left = 0
    for label, c in pages:
        for _ in range(c):                 # computer prepares the page
            r.computer_line.append("R")
            r.printer_line.append("#" if printer_left > 0 else ".")
            if printer_left > 0:
                printer_left -= 1
            t += 1
        if printer_left == 0:
            log(t, "%s sent -> printer free, accepts it" % label)
            printer_left = PRINT_TIME_PER_PAGE
            r.pages_printed += 1
        else:
            log(t, "%s sent -> printer BUSY: %s is LOST!" % (label, label))
            r.lost_pages.append(label)
    while printer_left > 0:
        r.computer_line.append("-")
        r.printer_line.append("#")
        printer_left -= 1
        t += 1
    r.computer_free_at = sum(c for _, c in pages)
    r.total_time = t
    return r


def print_buffer_result(title, r, n_pages, memory_kb):
    out()
    out("  RESULT - " + title)
    out("    Kernel buffer memory used ........ %d KB" % memory_kb)
    out("    All %2d pages printed at .......... t = %d tu" % (n_pages, r.total_time))
    out("    Computer free for other work at .. t = %d tu" % r.computer_free_at)
    out("    Computer blocked (waiting) ....... %d tu" % r.computer_blocked)
    out("    Printer idle (starved of data) ... %d tu  -> printer utilisation %s"
        % (r.printer_idle, pct(r.printer_busy, r.total_time)))


def describe_document(student):
    pages = document_pages(student)
    out("  Document: %s's revision notes, %d pages." % (student.name, student.pages))
    out("  Preparation time per page: " +
        " ".join("%s=%d" % (label, c) for label, c in pages))
    out("  (Every 5th page has a diagram, so it takes %d tu to prepare instead of %d.)"
        % (DIAGRAM_PAGE_RENDER, TEXT_PAGE_RENDER))
    out("  The printer needs %d tu per page. Copying a page into a buffer takes %d tu."
        % (PRINT_TIME_PER_PAGE, COPY_TIME_PER_PAGE))


def buffer_key():
    out("  Buffer key: [A3  ]=full, waiting to print  [A3 *]=being printed")
    out("              [A3 +]=being filled             [....]=empty slot")


# ---------------------------------------------------------------------------
def demo_no_buffering():
    banner("DEMO 1 - WHY BUFFER? SPEED MISMATCH WITH NO BUFFERING")
    wrap("Student A presses Print. The computer can prepare most pages in 1 tu, "
         "but the printer needs 4 tu to print each page. There is NO buffer "
         "between them. Two things can happen, and both are bad.")
    student = STUDENTS[0]
    pages = document_pages(student)
    out()
    describe_document(student)

    section("1(a) Computer pushes pages without waiting -> DATA OVERRUN")
    predict("With no buffer and no waiting, how many of the 10 pages will print?")
    r = simulate_unbuffered_overrun(pages)
    draw_timeline(r.computer_line, r.printer_line)
    out()
    out("  Pages printed: %d of %d" % (r.pages_printed, len(pages)))
    out("  Pages LOST   : " + ", ".join(r.lost_pages))
    explain_concept("Speed mismatch. The producer (computer) is faster than the "
            "consumer (printer). With nowhere to hold the extra data, pages "
            "that arrive while the printer is busy are lost.")
    pause()

    section("1(b) Computer waits for the printer (lock-step) -> NOTHING LOST, BUT SLOW")
    r = simulate_unbuffered_lockstep(pages)
    draw_timeline(r.computer_line, r.printer_line)
    print_buffer_result("No buffering (lock-step)", r, len(pages), 0)
    explain_concept("Without a buffer the fast computer is forced to run at the speed "
            "of the slow printer: it is blocked for every page it sends, and "
            "the printer sits idle while the next page is being prepared. A "
            "buffer in memory decouples the producer from the consumer.")
    pause()


def demo_transfer_size():
    banner("DEMO 2 - WHY BUFFER? TRANSFER-SIZE MISMATCH")
    student = STUDENTS[3]
    total_kb = student.pages * PAGE_SIZE_KB
    chunks = total_kb // CHUNK_KB
    wrap("%s prints a %d-page document (%d KB). The word processor hands the "
         "data to the OS in small %d KB pieces, but the printer interface moves "
         "data in %d KB blocks (one page). Every device transfer has a fixed "
         "set-up cost of %d tu (command + interrupt) plus %d tu per KB."
         % (student.name, student.pages, total_kb, CHUNK_KB, PAGE_SIZE_KB,
            DEVICE_SETUP_TIME, TIME_PER_KB))

    section("2(a) No buffer: every small piece becomes its own device transfer")
    unbuffered_ops = 0
    unbuffered_time = 0
    for piece in range(chunks):
        unbuffered_ops += 1                  # one device operation per piece
        unbuffered_time += DEVICE_SETUP_TIME + CHUNK_KB * TIME_PER_KB
    out("  %d pieces x (%d tu set-up + %d tu data) = %d tu, %d device transfers"
        % (chunks, DEVICE_SETUP_TIME, CHUNK_KB * TIME_PER_KB, unbuffered_time, unbuffered_ops))
    out("  The printer also receives %d partial blocks instead of whole pages."
        % chunks)

    section("2(b) Kernel buffer collects pieces until one full 4 KB block is ready")
    fill = 0          # KB currently held in the kernel buffer
    ops = 0
    t = 0
    for piece in range(1, chunks + 1):
        fill += CHUNK_KB
        bar = "#" * fill + "-" * (PAGE_SIZE_KB - fill)
        if fill == PAGE_SIZE_KB:
            # a whole block is assembled: ONE device transfer moves it
            ops += 1
            t += DEVICE_SETUP_TIME + PAGE_SIZE_KB * TIME_PER_KB
            if SHOW_EVENT_LOG or piece <= PAGE_SIZE_KB:
                out("    piece %2d -> buffer [%s] %d/%d KB FULL -> transfer %d sends page %d"
                    % (piece, bar, fill, PAGE_SIZE_KB, ops, ops))
            fill = 0
        elif SHOW_EVENT_LOG or piece <= PAGE_SIZE_KB:
            out("    piece %2d -> buffer [%s] %d/%d KB, keep collecting"
                % (piece, bar, fill, PAGE_SIZE_KB))
    if not SHOW_EVENT_LOG:
        out("    ... the same pattern repeats for pages 2 to %d" % student.pages)
    out()
    out("  %-30s %18s %12s" % ("Approach", "Device transfers", "Time (tu)"))
    out("  %-30s %18d %12d" % ("No buffer (1 KB pieces)", unbuffered_ops, unbuffered_time))
    out("  %-30s %18d %12d" % ("Buffered (4 KB blocks)", ops, t))
    explain_concept("Transfer-size mismatch. The two sides use different data sizes. "
            "The buffer reassembles small pieces into the block size the "
            "device expects, so there are fewer, larger transfers and fewer "
            "interrupts. (Networking does the reverse too: it fragments large "
            "messages into small packets and reassembles them at the receiver.)")
    pause()


def demo_copy_semantics():
    banner("DEMO 3 - WHY BUFFER? COPY SEMANTICS")
    student = STUDENTS[2]
    original = ["Q1: F = m x a (Newton's 2nd law)",
                "Q2: Answer = 42 m/s",
                "Q3: Answer = 17 J"]
    wrap("%s prints a 3-page answer sheet and the write() call returns at once "
         "(asynchronous I/O). Straight after, the student keeps editing the "
         "SAME document in the editor: 1 tu later page 2 is cleared to be "
         "rewritten, and 3 tu later page 3 is changed. The printer reads each "
         "page when it starts printing it (every %d tu)." % (student.name, PRINT_TIME_PER_PAGE))
    out()
    out("  What the student printed (version at the moment of write()):")
    for line in original:
        out("    | " + line)

    def run(copy_semantics):
        user_memory = list(original)          # the application's own buffer
        if copy_semantics:
            # write() copies the data into a KERNEL buffer before returning.
            kernel_buffer = list(user_memory)  # a real copy, not a reference
            returns_at = len(original) * COPY_TIME_PER_PAGE
            source = kernel_buffer
        else:
            # The printer is pointed straight at the application's memory.
            returns_at = 0
            source = user_memory
        edits = {returns_at + 1: (1, "Q2: [deleted - rewriting answer]"),
                 returns_at + 3: (2, "Q3: Answer = ??? (draft)")}
        printed = []
        start = returns_at
        for t in range(start, start + len(original) * PRINT_TIME_PER_PAGE + 1):
            if t in edits:                    # the student edits first ...
                page, text = edits[t]
                user_memory[page] = text
                log(t, "student edits page %d in the editor" % (page + 1))
            offset = t - start                # ... then the printer reads
            if offset % PRINT_TIME_PER_PAGE == 0 and offset // PRINT_TIME_PER_PAGE < len(original):
                page = offset // PRINT_TIME_PER_PAGE
                printed.append(source[page])
                log(t, "printer reads page %d: \"%s\"" % (page + 1, source[page]))
        return returns_at, printed

    section("3(a) WITHOUT copy semantics: printer reads the application's memory")
    returns_at, printed = run(False)
    out()
    out("  write() returned at t=%d. Printed output:" % returns_at)
    wrong = 0
    for i, line in enumerate(printed):
        mark = "OK" if line == original[i] else "WRONG"
        if line != original[i]:
            wrong += 1
        out("    %-5s | %s" % (mark, line))
    out("  -> %d of %d pages do NOT match what the student printed." % (wrong, len(original)))
    pause()

    section("3(b) WITH copy semantics: write() copies the data into a kernel buffer")
    returns_at, printed = run(True)
    out()
    out("  write() returned at t=%d (it spent %d tu copying %d KB). Printed output:"
        % (returns_at, returns_at, len(original) * PAGE_SIZE_KB))
    for i, line in enumerate(printed):
        mark = "OK" if line == original[i] else "WRONG"
        out("    %-5s | %s" % (mark, line))
    explain_concept("Copy semantics: the data written to the device must be the "
            "version that existed when write() was called, even if the "
            "application changes its buffer afterwards. The OS guarantees "
            "this by copying the data into a kernel buffer before write() "
            "returns. Trade-off: the copy costs time and kernel memory.")
    pause()


def run_single_mode(title, slots, explanation_before, explanation_after):
    student = STUDENTS[0]
    pages = document_pages(student)
    banner(title)
    wrap(explanation_before)
    out()
    describe_document(student)
    if SHOW_EVENT_LOG:
        out()
        buffer_key()
        out()
    r = simulate_buffered(pages, slots)
    draw_timeline(r.computer_line, r.printer_line)
    name = {1: "Single buffering (1 slot)", 2: "Double buffering (2 slots)"}.get(
        slots, "Circular buffering (%d slots)" % slots)
    print_buffer_result(name, r, len(pages), slots * PAGE_SIZE_KB)
    explain_concept(explanation_after)
    pause()
    return r


def demo_single():
    run_single_mode(
        "DEMO 4 - SINGLE BUFFERING (one kernel buffer slot)",
        1,
        "The OS places ONE buffer slot in kernel memory between the computer "
        "and the printer. The computer copies a page into the slot and is then "
        "free to prepare the next page while the printer prints. But it cannot "
        "copy the next page in until the printer has emptied the slot.",
        "Single buffering lets preparing (C) overlap with printing (T), so it "
        "beats no buffering. But the slot cannot be filled and emptied at the "
        "same time: each page costs about max(C, T) + M. Look for the W "
        "(blocked) and . (idle) gaps in the timeline.")


def demo_double():
    run_single_mode(
        "DEMO 5 - DOUBLE BUFFERING (two kernel buffer slots)",
        2,
        "The OS now uses TWO slots, B0 and B1. While the printer prints from "
        "one slot, the computer copies the next page into the other. Then they "
        "swap roles.",
        "Double buffering overlaps filling and emptying, so for steady work "
        "each page costs about max(C + M, T). The computer still blocks and the "
        "printer still starves after a slow diagram page, because only one page "
        "can be stored ahead. Cost: twice the buffer memory.")


def demo_circular():
    global_slots = read_int(
        "\n  CLASS CHOICE: how many buffer slots should the ring have? (3-6, Enter = %d): "
        % DEFAULT_CIRCULAR_SLOTS, 3, 6, DEFAULT_CIRCULAR_SLOTS)
    run_single_mode(
        "DEMO 6 - CIRCULAR BUFFERING (a ring of %d reusable slots)" % global_slots,
        global_slots,
        "The OS uses a ring of %d slots. 'in' points to the next slot the "
        "computer will fill; 'out' points to the next slot the printer will "
        "print. Each pointer moves forward with (ptr + 1) mod %d, so slots are "
        "reused in a circle. On quick text pages the computer runs ahead and "
        "stores several pages; on a slow diagram page the printer keeps going "
        "from the stored pages." % (global_slots, global_slots),
        "Circular buffering absorbs bursts: the extra slots act as a reserve, "
        "so the printer is never starved after a slow page and the computer "
        "finishes and is freed earlier. Cost: more memory and pointer "
        "management (in/out, wrap-around, full/empty checks). Beyond a point "
        "extra slots stop helping, because the printer itself is the limit.")


def demo_compare_buffering():
    banner("DEMO 7 - COMPARING ALL BUFFERING APPROACHES (whole exam-week workload)")
    pages = whole_workload()
    wrap("Same rules, bigger workload: all five documents (A=10, B=20, C=15, "
         "D=5, E=25 pages) sent as one stream of %d pages. Minimum possible time "
         "is limited by the printer: %d pages x %d tu = %d tu of printing."
         % (len(pages), len(pages), PRINT_TIME_PER_PAGE, len(pages) * PRINT_TIME_PER_PAGE))
    predict("Will 6 slots be much faster than 4 slots?")
    global SHOW_EVENT_LOG
    saved = SHOW_EVENT_LOG
    SHOW_EVENT_LOG = False
    rows = [("No buffering (lock-step)", 0, simulate_unbuffered_lockstep(pages)),
            ("Single buffering", 1, simulate_buffered(pages, 1)),
            ("Double buffering", 2, simulate_buffered(pages, 2)),
            ("Circular buffering", 4, simulate_buffered(pages, 4)),
            ("Circular buffering", 6, simulate_buffered(pages, 6))]
    lost = simulate_unbuffered_overrun(pages)
    SHOW_EVENT_LOG = saved
    out()
    out("  %-25s %6s %7s %8s %8s %8s %7s" %
        ("Approach", "Slots", "Memory", "Finish", "Computer", "Printer", "Printer"))
    out("  %-25s %6s %7s %8s %8s %8s %7s" %
        ("", "", "(KB)", "(tu)", "blocked", "idle", "util."))
    out("  " + "-" * 74)
    for name, slots, r in rows:
        out("  %-25s %6d %7d %8d %8d %8d %7s" %
            (name, slots, slots * PAGE_SIZE_KB, r.total_time, r.computer_blocked,
             r.printer_idle, pct(r.printer_busy, r.total_time)))
    out()
    out("  No buffering, no waiting (overrun): %d of %d pages LOST."
        % (len(lost.lost_pages), len(pages)))
    explain_concept("Trade-off: each extra slot costs %d KB of kernel memory. Going from "
            "0 to 1 to 2 slots gives big gains; a ring of 4 keeps the printer "
            "busy over 99%% of the time; going from 4 to 6 slots only trims the "
            "computer's waiting and does not finish any sooner, because the "
            "printer itself is now the bottleneck. Choose the smallest buffer "
            "that absorbs the bursts in your workload." % PAGE_SIZE_KB)
    pause()


# =============================================================================
# PART B - SPOOLING VERSUS DEVICE RESERVATION
# =============================================================================
class SpoolJob:
    def __init__(self, job_id, student):
        self.job_id = job_id
        self.student = student
        self.file_name = "job%02d_%s.prn" % (job_id, student.letter)
        self.size_kb = student.pages * PAGE_SIZE_KB
        self.ready_at = student.submit_time + spool_write_time(student.pages)

    def short(self):
        return "job%02d %s %dp" % (self.job_id, self.student.letter, self.student.pages)


def spool_write_time(pages):
    return (pages + PAGES_SPOOLED_PER_TU - 1) // PAGES_SPOOLED_PER_TU


class StudentStats:
    def __init__(self, student):
        self.student = student
        self.free_at = 0          # when the student can carry on with other work
        self.started_at = -1      # when the first page started printing
        self.finished_at = -1     # when the last page came out
        self.cancelled = False


def print_student_table(stats, title):
    out()
    out("  " + title)
    out("  %-14s %6s %8s %10s %9s %10s %9s" %
        ("Student", "Pages", "Clicked", "Free to", "Blocked", "Printing", "Done at"))
    out("  %-14s %6s %8s %10s %9s %10s %9s" %
        ("", "", "Print", "work at", "(tu)", "started", "(tu)"))
    out("  " + "-" * 74)
    for st in stats:
        s = st.student
        if st.cancelled:
            out("  %-14s %6d %8d %10d %9d %10s %9s" %
                (s.name, s.pages, s.submit_time, st.free_at,
                 st.free_at - s.submit_time, "cancelled", "-"))
        else:
            out("  %-14s %6d %8d %10d %9d %10d %9d" %
                (s.name, s.pages, s.submit_time, st.free_at,
                 st.free_at - s.submit_time, st.started_at, st.finished_at))


class SpoolResult:
    def __init__(self):
        self.stats = []
        self.total_time = 0
        self.printer_busy = 0
        self.printer_idle = 0
        self.peak_disk_kb = 0
        self.output_tray = []


def simulate_spooling(cancel_letter=""):
    """
    SPOOLING. Each student's output is intercepted by the OS and written to
    a separate spool file on DISK. When the file is complete the job joins
    the print queue and the student is free. A background spooler (daemon)
    sends one complete job at a time to the printer, in FIFO order.
    """
    r = SpoolResult()
    stats = [StudentStats(s) for s in STUDENTS]
    r.stats = stats
    jobs = [SpoolJob(i + 1, s) for i, s in enumerate(STUDENTS)]
    queue = []              # the print queue (spool queue): FIFO of complete jobs
    disk_kb = 0
    current = None          # job being printed
    page_left = 0
    pages_done = 0
    remaining_jobs = len(jobs)
    t = 0

    def show_queue():
        if SHOW_EVENT_LOG:
            text = "  ".join("[" + j.short() + "]" for j in queue) if queue else "(empty)"
            out("         Print queue: " + text)

    while remaining_jobs > 0:
        # 1. students click Print: output is redirected to a spool file
        for j in jobs:
            if j.student.submit_time == t:
                log(t, "%s clicks Print (%d pages) -> spooler writes %s to disk"
                    % (j.student.name, j.student.pages, j.file_name))
                disk_kb += j.size_kb
                r.peak_disk_kb = max(r.peak_disk_kb, disk_kb)
        # 2. spool files that are complete join the queue; students are free
        for i, j in enumerate(jobs):
            if j.ready_at == t:
                stats[i].free_at = t
                queue.append(j)
                log(t, "%s complete (%d KB). %s is FREE to leave."
                    % (j.file_name, j.size_kb, j.student.name))
                show_queue()
                if cancel_letter == j.student.letter:
                    # Spooler control interface: the job is removed before printing
                    queue.remove(j)
                    disk_kb -= j.size_kb
                    stats[i].cancelled = True
                    remaining_jobs -= 1
                    log(t, "CANCEL request: %s removed from the queue (lprm)." % j.file_name)
                    show_queue()
        # 3. the spooler feeds the printer one COMPLETE job at a time
        if current is None and queue:
            current = queue.pop(0)
            page_left = PRINT_TIME_PER_PAGE
            pages_done = 0
            idx = jobs.index(current)
            stats[idx].started_at = t
            log(t, "Spooler sends %s (%s) to the printer" % (current.file_name, current.student.name))
            show_queue()
        # 4. one tu passes
        if current is not None:
            r.printer_busy += 1
        else:
            r.printer_idle += 1
        t += 1
        if current is not None:
            page_left -= 1
            if page_left == 0:
                pages_done += 1
                r.output_tray.append(current.student.letter + str(pages_done))
                if pages_done == current.student.pages:
                    idx = jobs.index(current)
                    stats[idx].finished_at = t
                    disk_kb -= current.size_kb
                    log(t, "Printer finished %s (%d pages). Spool file deleted."
                        % (current.file_name, pages_done))
                    current = None
                    remaining_jobs -= 1
                else:
                    page_left = PRINT_TIME_PER_PAGE
    r.total_time = t
    return r


class ReservationResult:
    def __init__(self):
        self.stats = []
        self.total_time = 0
        self.printer_busy = 0
        self.printer_idle_held = 0    # printer reserved but not printing
        self.printer_idle_free = 0    # nobody wanted it
        self.starved = False


def simulate_reservation(crash_letter=""):
    """
    DEVICE RESERVATION. A student's program must call allocate(printer)
    and gets EXCLUSIVE use of it until it calls release(printer). Others
    who call allocate() meanwhile are BLOCKED in a FIFO wait queue.
    The owner sends its pages straight to the printer, so it stays blocked
    for the whole print, and any pause it makes leaves the printer idle.
    If crash_letter is set, that student's program crashes before calling
    release(): nobody else can ever get the printer (STARVATION).
    """
    r = ReservationResult()
    stats = [StudentStats(s) for s in STUDENTS]
    r.stats = stats
    owner = -1
    wait_queue = []
    pages_done = 0
    phase = ""          # "PRINT", "PAUSE", "HOLD", "CRASHED"
    left = 0
    finished = 0
    t = 0
    n = len(STUDENTS)

    def show_wait():
        if SHOW_EVENT_LOG:
            names = ", ".join(STUDENTS[i].name for i in wait_queue) if wait_queue else "(none)"
            out("         Waiting for the printer: " + names)

    def grant(i):
        nonlocal owner, pages_done, phase, left
        owner = i
        pages_done = 0
        phase = "PRINT"
        left = PRINT_TIME_PER_PAGE
        stats[i].started_at = t
        log(t, "allocate(printer) GRANTED to %s" % STUDENTS[i].name)

    while finished < n:
        if crash_letter != "" and t >= WATCHDOG_LIMIT:
            r.starved = True
            log(t, "WATCHDOG: no progress for a long time - simulation stopped.")
            break
        # 1. students call allocate(printer)
        for i, s in enumerate(STUDENTS):
            if s.submit_time == t:
                if owner == -1 and not wait_queue:
                    log(t, "%s calls allocate(printer)" % s.name)
                    grant(i)
                else:
                    wait_queue.append(i)
                    holder = STUDENTS[owner].name if owner != -1 else "-"
                    log(t, "%s calls allocate(printer) -> BUSY (held by %s): BLOCKED"
                        % (s.name, holder))
                    show_wait()
        # 2. account for this tu
        if owner == -1:
            r.printer_idle_free += 1
        elif phase == "PRINT":
            r.printer_busy += 1
        else:
            r.printer_idle_held += 1
        # 3. one tu passes
        t += 1
        if owner != -1 and phase != "CRASHED":
            left -= 1
            if left == 0:
                s = STUDENTS[owner]
                release = False
                if phase == "PRINT":
                    pages_done += 1
                    if pages_done == s.pages:
                        stats[owner].finished_at = t
                        if s.letter == crash_letter:
                            phase = "CRASHED"
                            log(t, "%s's program CRASHES before calling release()!" % s.name)
                            log(t, "The printer stays reserved by a dead process.")
                        elif s.hold_after_finish > 0:
                            phase = "HOLD"
                            left = s.hold_after_finish
                            log(t, "%s finished printing but keeps the printer reserved"
                                % s.name)
                            log(t, "  (reviewing pages for %d tu) -> printer IDLE but LOCKED"
                                % s.hold_after_finish)
                        else:
                            release = True
                    elif pages_done == s.pause_after_page:
                        phase = "PAUSE"
                        left = s.pause_length
                        log(t, "%s pauses after page %d to check margins (%d tu)"
                            % (s.name, pages_done, s.pause_length))
                        log(t, "  -> printer IDLE but still RESERVED; others keep waiting")
                    else:
                        left = PRINT_TIME_PER_PAGE
                elif phase == "PAUSE":
                    phase = "PRINT"
                    left = PRINT_TIME_PER_PAGE
                    log(t, "%s resumes printing" % s.name)
                elif phase == "HOLD":
                    release = True
                if release:
                    stats[owner].free_at = t
                    log(t, "%s calls release(printer) after %d pages" % (s.name, s.pages))
                    finished += 1
                    owner = -1
                    if wait_queue:
                        grant(wait_queue.pop(0))
                        show_wait()
    r.total_time = t
    return r


def demo_no_coordination():
    banner("DEMO 8 - WHAT GOES WRONG WITHOUT SPOOLING OR RESERVATION")
    wrap("All five students print at the same moment and every program writes "
         "its pages straight to the shared printer. The printer is a "
         "NON-SHARABLE device: it simply prints pages in the order they arrive, "
         "taking one page from each program in turn.")
    tray = []
    remaining = [s.pages for s in STUDENTS]
    printed = [0] * len(STUDENTS)
    while sum(remaining) > 0:
        for i, s in enumerate(STUDENTS):
            if remaining[i] > 0:
                printed[i] += 1
                remaining[i] -= 1
                tray.append(s.letter + str(printed[i]))
    out()
    out("  Output tray, sheet by sheet (first 30 of %d sheets):" % len(tray))
    for start in range(0, 30, 10):
        out("    sheets %2d-%2d: %s" % (start + 1, start + 10,
                                      " ".join("%-3s" % x for x in tray[start:start + 10])))
    out()
    for s in STUDENTS:
        positions = [i + 1 for i, x in enumerate(tray) if x[0] == s.letter]
        out("  %-12s %2d pages scattered between sheet %2d and sheet %2d"
            % (s.name, s.pages, positions[0], positions[-1]))
    explain_concept("Interleaved output. A printer cannot usefully mix the I/O streams "
            "of several programs. The OS must coordinate access - either by "
            "SPOOLING the output or by giving one process exclusive use of "
            "the device (DEVICE RESERVATION).")
    pause()


def demo_reservation():
    banner("DEMO 9 - DEVICE RESERVATION (allocate / release)")
    wrap("Now the OS gives the printer to ONE student at a time. A program "
         "must call allocate(printer) before printing and release(printer) "
         "afterwards. Students who ask while the printer is reserved are "
         "blocked in a wait queue. Student B pauses for 12 tu after page 10 "
         "to check the margins; Student D keeps the printer reserved for 8 tu "
         "to review the pages before releasing it.")
    predict("While Student B is checking margins, what is the printer doing?")
    out()
    r = simulate_reservation()
    print_student_table(r.stats, "RESULT - Device reservation")
    out()
    out("  All jobs finished at t = %d tu" % r.total_time)
    out("  Printer printing ............. %d tu (%s)"
        % (r.printer_busy, pct(r.printer_busy, r.total_time)))
    out("  Printer IDLE but reserved .... %d tu  <- wasted: others were waiting"
        % r.printer_idle_held)
    total_blocked = sum(st.free_at - st.student.submit_time for st in r.stats)
    out("  Total student time blocked ... %d tu" % total_blocked)
    explain_concept("Device reservation gives exclusive access, so output is never "
            "mixed. But each student is blocked while waiting AND while their "
            "own job prints, and the device is wasted whenever its owner pauses.")
    pause()

    section("9(b) What goes wrong: a program crashes before calling release()")
    r = simulate_reservation(crash_letter="A")
    out()
    waiting = [st.student.name for st in r.stats if st.finished_at == -1]
    out("  After %d tu: %d student(s) still waiting, 0 of their pages printed:"
        % (r.total_time, len(waiting)))
    out("    " + ", ".join(waiting))
    explain_concept("STARVATION: a process that never releases a reserved device "
            "blocks everyone else until an administrator steps in. With "
            "spooling this cannot happen: once a job is on disk, a crashed "
            "student program does not affect the printer.")
    pause()

    section("9(c) What goes wrong: DEADLOCK with two reserved devices")
    wrap("The printing centre also has one scanner. Student A needs the "
         "printer AND the scanner (to scan a signed cover page). Student B "
         "needs the scanner AND the printer. Both use reservation.")
    out()
    owner = {"printer": "", "scanner": ""}
    waits_for = {}
    steps = [(0, "A", "printer"), (1, "B", "scanner"),
             (2, "A", "scanner"), (3, "B", "printer")]
    names = {"A": STUDENTS[0].name, "B": STUDENTS[1].name}
    for t, who, device in steps:
        if owner[device] == "":
            owner[device] = who
            log(t, "%s allocate(%s) -> GRANTED" % (names[who], device))
        else:
            waits_for[who] = owner[device]
            log(t, "%s allocate(%s) -> BUSY (held by %s): BLOCKED"
                % (names[who], device, names[owner[device]]))
    out()
    if waits_for.get("A") == "B" and waits_for.get("B") == "A":
        out("  Wait-for cycle: %s -> waits for -> %s -> waits for -> %s"
            % (names["A"], names["B"], names["A"]))
        out("  DEADLOCK: neither student can ever continue.")
    explain_concept("Reserving several dedicated devices allows hold-and-wait, and "
            "with circular waiting the processes deadlock. If the printer is "
            "spooled instead, Student B's print request simply goes to the "
            "spool queue, B finishes and releases the scanner, and A continues.")
    pause()


def demo_spooling():
    banner("DEMO 10 - SPOOLING WITH A SPOOLER AND PRINT QUEUE")
    wrap("Now the OS intercepts every student's printer output and writes it "
         "to a separate spool file on DISK (secondary storage). When a spool "
         "file is complete, the job joins the print queue and the student is "
         "free. A background spooler sends one complete job at a time to the "
         "printer. Writing to disk is fast: %d pages per tu." % PAGES_SPOOLED_PER_TU)
    out()
    out("  CLASS CHOICE: the spooler lets users manage the queue.")
    choice = read_line("  Cancel a job before it prints? Enter A-E, or press Enter for none: ")
    choice = choice.strip().upper()
    if choice not in ("A", "B", "C", "D", "E"):
        choice = ""
    if choice != "":
        out("  -> The class cancels %s's job." % STUDENTS[ord(choice) - ord("A")].name)
    out()
    r = simulate_spooling(choice)
    print_student_table(r.stats, "RESULT - Spooling")
    out()
    out("  All jobs finished at t = %d tu" % r.total_time)
    out("  Printer printing ............. %d tu (%s)"
        % (r.printer_busy, pct(r.printer_busy, r.total_time)))
    out("  Printer idle ................. %d tu (only before the first job)"
        % r.printer_idle)
    out("  Peak disk space in spool area  %d KB" % r.peak_disk_kb)
    tray = r.output_tray
    out("  Output tray: %s ... %s (each job printed as one complete block)"
        % (" ".join(tray[:4]), " ".join(tray[-3:])))
    explain_concept("Spooling: output for a device that cannot accept interleaved data "
            "is held in spool files on disk; the spooler prints them one at a "
            "time. Every student is free within a few tu, the printer runs back "
            "to back, and jobs can be listed, reordered or cancelled. Cost: disk "
            "space, a spooler process, and printing cannot start until a job's "
            "spool file is complete.")
    pause()


def demo_compare_spooling():
    banner("DEMO 11 - SPOOLING vs DEVICE RESERVATION (same five students)")
    global SHOW_EVENT_LOG
    saved = SHOW_EVENT_LOG
    SHOW_EVENT_LOG = False
    spool = simulate_spooling("")
    reserve = simulate_reservation()
    SHOW_EVENT_LOG = saved
    out()
    out("  %-14s %22s %22s" % ("", "Time blocked (tu)", "Document done at (tu)"))
    out("  %-14s %11s %10s %11s %10s" % ("Student", "Reserve", "Spool", "Reserve", "Spool"))
    out("  " + "-" * 60)
    for a, b in zip(reserve.stats, spool.stats):
        s = a.student
        out("  %-14s %11d %10d %11d %10d" %
            (s.name, a.free_at - s.submit_time, b.free_at - s.submit_time,
             a.finished_at, b.finished_at))
    rb = sum(st.free_at - st.student.submit_time for st in reserve.stats)
    sb = sum(st.free_at - st.student.submit_time for st in spool.stats)
    out("  " + "-" * 60)
    out("  %-14s %11d %10d" % ("TOTAL blocked", rb, sb))
    out()
    out("  %-36s %12s %12s" % ("Measure", "Reservation", "Spooling"))
    out("  " + "-" * 62)
    out("  %-36s %12d %12d" % ("All jobs finished at (tu)", reserve.total_time, spool.total_time))
    out("  %-36s %12s %12s" % ("Printer utilisation",
                               pct(reserve.printer_busy, reserve.total_time),
                               pct(spool.printer_busy, spool.total_time)))
    out("  %-36s %12d %12d" % ("Printer idle while reserved (tu)", reserve.printer_idle_held, 0))
    out("  %-36s %12d %12d" % ("Disk space needed (peak KB)", 0, spool.peak_disk_kb))
    out("  %-36s %12s %12s" % ("Can users cancel / reorder jobs?", "no", "yes"))
    out("  %-36s %12s %12s" % ("Starvation / deadlock risk", "yes", "no"))
    explain_concept("Both approaches stop output from being mixed. Spooling wins for a "
            "busy shared printer: students are freed almost at once and the "
            "printer never waits for a hesitating owner. It costs disk space "
            "and a spooler, and Student A's document finished 1 tu LATER because "
            "it had to be written to disk first. The printer still prints at the "
            "same speed - spooling removes waiting and idle gaps, it does not "
            "make the printer faster. Reservation suits devices that need an interactive, "
            "uninterrupted session (e.g. a tape drive or lab instrument).")
    pause()


# =============================================================================
# MENU
# =============================================================================
def show_assumptions():
    banner("SIMULATION ASSUMPTIONS")
    out("  Time is simulated in time units (tu). Real devices differ by much")
    out("  larger ratios; these values are scaled so the class can follow.")
    out()
    out("  Workload (the same for every demo):")
    for s in STUDENTS:
        out("    %-12s %2d pages, clicks Print at t=%d" % (s.name, s.pages, s.submit_time))
    out()
    out("  Buffering:")
    out("    Prepare a text page (C) ............... %d tu" % TEXT_PAGE_RENDER)
    out("    Prepare a diagram page (every 5th) .... %d tu" % DIAGRAM_PAGE_RENDER)
    out("    Copy one page into a buffer slot (M) .. %d tu" % COPY_TIME_PER_PAGE)
    out("    Printer prints one page (T) ........... %d tu" % PRINT_TIME_PER_PAGE)
    out("    One page = one buffer slot ............ %d KB" % PAGE_SIZE_KB)
    out("    Transfer-size demo: %d KB pieces, %d KB blocks, %d tu set-up per transfer"
        % (CHUNK_KB, PAGE_SIZE_KB, DEVICE_SETUP_TIME))
    out()
    out("  Spooling / device reservation:")
    out("    Writing a spool file to disk .......... %d pages per tu" % PAGES_SPOOLED_PER_TU)
    out("    Spooler scheduling .................... FIFO, one complete job at a time")
    out("    Student B (reservation) ............... pauses 12 tu after page 10")
    out("    Student D (reservation) ............... holds printer 8 tu before release")
    pause()


def rename_students():
    banner("CLASS PARTICIPATION - NAME THE FIVE STUDENTS")
    out("  Enter a classmate's name for each student (max 12 characters).")
    out("  Press Enter to keep the current name.")
    for s in STUDENTS:
        text = read_line("    %s (%d pages): " % (s.name, s.pages)).strip()
        if text != "":
            s.name = text[:12]
    out()
    out("  Names saved: " + ", ".join(s.name for s in STUDENTS))


def guided_demo():
    banner("GUIDED DEMONSTRATION - UNIVERSITY PRINTING CENTRE, EXAM WEEK")
    wrap("Five students submit documents almost at the same time. The "
         "computers produce print data much faster than the one shared printer "
         "can print it. We will first follow the DATA of one job (buffering), "
         "then follow the JOBS of all five students (spooling).")
    global SHOW_EVENT_LOG
    saved = SHOW_EVENT_LOG
    SHOW_EVENT_LOG = False            # timelines only, to fit in 10 minutes
    pause()
    demo_no_buffering()
    demo_transfer_size()
    demo_copy_semantics()
    demo_single()
    demo_double()
    demo_circular()
    demo_compare_buffering()
    SHOW_EVENT_LOG = True             # event log on for the spooling story
    demo_no_coordination()
    demo_reservation()
    demo_spooling()
    demo_compare_spooling()
    SHOW_EVENT_LOG = saved
    banner("SUMMARY")
    wrap("BUFFERING manages the flow of DATA between two components that work at "
         "different speeds or sizes, using memory (RAM). SPOOLING manages complete "
         "JOBS from many processes for one non-sharable device, using a queue of "
         "files on disk. They solve different problems and are used together: the "
         "spooler itself uses buffers to move each job to the printer.")


def main():
    global SHOW_EVENT_LOG
    banner("GROUP 8 - BUFFERING AND SPOOLING: UNIVERSITY PRINTING CENTRE")
    out("  Charbel Ngeti 221441 | Russel Muiga 220510 | Alvin Otanga 220267")
    out("  Handel Songoro 220337 | Jesse Kyongo 180904")
    while True:
        out()
        out("  PART A - BUFFERING (moving data from one computer to the printer)")
        out("    1. Why buffer? Speed mismatch with NO buffering")
        out("    2. Why buffer? Transfer-size mismatch")
        out("    3. Why buffer? Copy semantics")
        out("    4. Single buffering")
        out("    5. Double buffering")
        out("    6. Circular buffering (class chooses the number of slots)")
        out("    7. Compare all buffering approaches (whole workload)")
        out("  PART B - SPOOLING (many students sharing one printer)")
        out("    8. No coordination: everyone prints directly at once")
        out("    9. Device reservation (allocate / release) and what goes wrong")
        out("   10. Spooling with a spooler and print queue")
        out("   11. Compare spooling vs device reservation")
        out("  OTHER")
        out("   12. Guided demonstration (presentation order, about 10 minutes)")
        out("   13. Show simulation assumptions")
        out("   14. Class participation: enter classmates' names for the students")
        out("   15. Toggle detailed event log (currently %s)" % ("ON" if SHOW_EVENT_LOG else "OFF"))
        out("    0. Exit")
        choice = read_int("\n  Choose an option: ", 0, 15, -1)
        if choice == -1:
            continue
        if choice == 0:
            out("  Goodbye from Group 8!")
            return
        actions = {1: demo_no_buffering, 2: demo_transfer_size, 3: demo_copy_semantics,
                   4: demo_single, 5: demo_double, 6: demo_circular,
                   7: demo_compare_buffering, 8: demo_no_coordination,
                   9: demo_reservation, 10: demo_spooling, 11: demo_compare_spooling,
                   12: guided_demo, 13: show_assumptions, 14: rename_students}
        if choice == 15:
            SHOW_EVENT_LOG = not SHOW_EVENT_LOG
            out("  Detailed event log is now %s." % ("ON" if SHOW_EVENT_LOG else "OFF"))
        else:
            actions[choice]()


if __name__ == "__main__":
    main()
