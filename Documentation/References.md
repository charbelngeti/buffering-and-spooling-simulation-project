# References – Group 8: Buffering and Spooling

## Primary course reference

1. Silberschatz, A., Galvin, P. B. and Gagne, G. (2018). *Operating System Concepts*, 10th ed. Hoboken, NJ: Wiley.
   - **Chapter 12, I/O Systems.** Section 12.4, *Kernel I/O Subsystem*, covers buffering (12.4.2), caching (12.4.3), and spooling and device reservation (12.4.4). Our terminology comes from here: speed mismatch, transfer-size mismatch, copy semantics, double buffering, spool, spooler, device reservation, and the buffer vs cache distinction.
   - **Producer–consumer and bounded-buffer problems:** Chapters 6–7 (synchronisation).
   - **Deadlock conditions** (hold-and-wait, circular wait): Chapter 8. These are needed to explain the deadlock risk of device reservation.

## Supplementary references

2. Stallings, W. (2018). *Operating Systems: Internals and Design Principles*, 9th ed. Harlow: Pearson. Chapter 11, *I/O Management and Disk Scheduling*, Section 11.4, *I/O Buffering*. This book gives the **single, double and circular buffering** classification and the timing estimates we use: single buffering ≈ max(C, T) + M per block, and double buffering ≈ max(C, T). Silberschatz describes double buffering but does not present this three-way classification, so we mark it as supplementary material in our notes.

3. Tanenbaum, A. S. and Bos, H. (2015). *Modern Operating Systems*, 4th ed. Boston: Pearson. Section 5.2 (Principles of I/O Software) and Section 5.3 (I/O Software Layers), covering buffering, spooling directories and the printer daemon.

4. Arpaci-Dusseau, R. H. and Arpaci-Dusseau, A. C. (2018). *Operating Systems: Three Easy Pieces*. Chapter 30, *Condition Variables*, covers the bounded-buffer (producer/consumer) problem. Freely available at https://pages.cs.wisc.edu/~remzi/OSTEP/

5. OpenPrinting. *CUPS Documentation*. https://openprinting.github.io/cups/ — a real-world example of a print spooler: the `cupsd` scheduler, the spool directory `/var/spool/cups`, and the `lp`, `lpq`/`lpstat` and `lprm`/`cancel` commands.

## Notes on accuracy

- The study guide given to the group attributes the `max(C, T) + M` formula to Silberschatz. It actually comes from **Stallings** (reference 2). Our notes and slides credit Stallings.
- Silberschatz notes that a spool *is* a kind of buffer: it holds output for a device, such as a printer, that cannot accept interleaved data streams. The real distinction is **what** is held and **where**. Buffering holds data in transit, usually in memory. Spooling holds complete jobs as files on secondary storage, managed by a spooler.
- The simulation's time values are scaled for teaching. They are not measurements of real hardware.
