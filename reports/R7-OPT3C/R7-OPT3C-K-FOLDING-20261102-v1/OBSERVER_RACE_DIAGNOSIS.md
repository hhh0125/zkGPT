# Native measurement observer exit race

At 2026-11-02 05:47 UTC the original native scheduler stopped after its third
reference worker raised PermissionError while reading /proc/3045765/io.
The third C++ benchmark log contains all1485 kernel calls,32 round timings,
the terminal check and complete OPT3C_BENCH/OPT3A_AUTH/OPT2_AUTH/OPT2_MSM footer.
Its exact coefficient/challenge/endpoint digest equals both completed reference
repeats. Runtime source401 files and the benchmark binary remain unchanged.

Root cause reproduced with /bin/true: wait until its process stat is Z, then
read /proc/PID/io before wait4. It raises PermissionError; catching the optional
sample failure allows wait4 to collect the real exit0. A normal100-process
sample loop also collected100 exit0 results. No cryptographic code was changed.

The observer now catches FileNotFoundError/PermissionError/ProcessLookupError,
records an incomplete sample and returns to wait4. The failed Python worker,
parent traceback, all raw C++ output and resource samples are retained.

The third arithmetic timing is retained as a directly measured completed
calculation, not an extrapolation. Its final wait4 process exit and exact OS
storage/cpu counters were lost when the observer exited and are NULL. Last
observed process IO is labelled as such. C++ self-timed CPU, full arithmetic wall,
peak RSS, kernel metrics,32 coefficient times and exact digest remain available.
Reference K file IO is zero; process OS IO includes mapping/log work and is
optional supplemental accounting. Do not claim9 observed zero process exits:
the final set has9 complete arithmetic self-reports and8 wait4 zero exits.

Resume only unrun file/hybrid batches (three each) in the same original output
directory and same total four-CPU affinity. Preserve the two original validated
wait4 records and the recovered third self-report separately. No proof has run.
