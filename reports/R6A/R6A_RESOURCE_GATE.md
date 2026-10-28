# Resource gate from actual builder

Actual arrays/capacities, not oneblock*12: gate capacity4810063872,coordinate capacity1021602816,nonmaster padded fields42568704,master padded2^28,child padded313398272. Shared lookup planner78748221440bytes. ACTUAL_RESOURCE_PHASES.json adds real val0 growth capacity, retained layers/int32matrices, dense subset scratch24*masterActual, cc.w+external master, allchildren, up to3 circuit capacities, existing mapped-input8master workspace, bounded thread stacks/slack and25% proving/verifying margin. Serialization adds4proof bounds. Existing legacy measured envelope is recorded separately and is not the sole budget.

| Phase | Conservative bytes | GiB |
|---|---:|---:|
| graph_bytes | 6368537600 | 5.931 |
| witness_bytes | 23112176384 | 21.525 |
| commit_bytes | 29065227264 | 27.069 |
| prove_bytes | 160083395840 | 149.089 |
| verify_bytes | 66339897920 | 61.784 |
| serialize_bytes | 160226833920 | 149.223 |
| artifact_disk_bytes | 210546944 | 0.196 |

Proposed separate run caps: Prover192GiB/28800s; independent Verifier96GiB/14400s; disk reserve64GiB; shared available memory reserve32GiB; CPU affinity4 (maximum permitted8). Existing GKR has a fixed32-worker phase; configured general workers4 does not imply only4 OS threads. Budget includes stack/slack; cgroup PIDroom must be>=64. No large proofs run concurrently.

HOST_RESOURCE_SNAPSHOT.json records current host/cgroup/limits/disk. Visible ancestor memory.max=max; PIDheadroom ample, host memory and roughly2.7TB free disk exceed these conservative budgets at the snapshot. No shared-server allocation is reserved by this observation. New gates dynamically intersect host/cgroup remaining memory and PIDroom and reject an inherited address-space hard cap below the requested signed permit budget. Current memory/disk/process conditions support the proposed caps; availability must be checked again immediately before execution.

Native RLIMIT_AS, CPU affinity and alarm enforce memory/CPU/wall boundaries. run_limited.py additionally watches process-group RSS, cgroup/host shared memory and disk every0.25s, kills the entire group on deadline/RSS/reserve failure and retains logs; allocation exceptions/shape/domain/hash/permit mismatch abort. No automatic retries, range widening, constraint removal or cleanup. Watchdog tests used only sleeping processes and an intentionally impossible disk reserve.

Proof upper bound35859520bytes and artifacts conservative210546944bytes exclude separately selected raw-log/archive reserve. No temporary full master/W spill file is required by this implementation. Full12 duration is uncalibrated: the8h/4h values are stop allowances, not predicted completion times. Oneblock runtime*12 is not used as a sufficient execution argument. Further remaining gates are explicit authorization/valid permits and admission of the real12-block numerical witness before proving; no security/paper certification is inferred.
