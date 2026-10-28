# Task0-C.1 independent interactive verifier design
User authorization: implement the supplied Task0-C.1 specification, only in this new research directory. Engineering choices below are agent decisions within that scope.
## Fixed constraints
Preserve old stages and all eight copied legacy mathematical modules byte-identically. N<=1024, degree 2/3, H-PC K<=4, GKR width<=4 (indicator table<=64), <=2 layers. Separate Linux verifier process, PR_SET_DUMPABLE=0 mandatory; trusted OS/loaded verifier code and inherited IPC endpoints. No disk replay authority, no pickle, no arbitrary remote code. CPU 120s, AS 512MiB, per-message 2MiB, aggregate input 16MiB, max events 2048, idle timeout 10s, total session 60s (trusted startup test configuration may shorten).
## Architecture
Verifier preimports unmodified legacy code before READY; it owns independent Entropy, all transcript objects, session ID, sequential protocol parser, prefix computations and receipt objects. Prover transport adapter intercepts legacy create_verifier/Transcript/VerifierCoins/Flow in PROVER process only; its proxy contains public challenges received from V, never authority. Simulator preselected coins and local verifier RNG injection raise errors in live adapter. Original legacy functions remain internal research helpers, not acceptance APIs.
V executes a fixed online grammar, reconstructing proof fields from actual incoming messages, issuing coins automatically at predetermined steps, checking each component's original verifier before next component. No generic challenge-request API. V reconstructs child statement from its own prior results before opening it. Final complete payload must equal V-reconstructed statement/proof canonically, then original whole verifier checks local receipts. Only V emits ACCEPT.
## Wire API (exact dictionaries, JSON lines UTF-8; no duplicate keys)
V startup: {status:READY,suite:TRANSPORT_SUITE,sid:<random 32-byte hex>,mode:<startup mode>,verifier_pid:int}.
TRANSPORT_SUITE = R7-C1-INDEPENDENT-INTERACTIVE-RESEARCH-v1. Cryptographic suite stays legacy Task0C suite: math/generators unchanged.
Every P request contains op,sid,seq (starts 0 and strictly increments).
- bind: + statement (fixed outer, session must equal V sid).
- open: + family (c or hpc), statement (must canonical-equal V expected).
- message: + label,value (exact next scheduled label).
- finish: no extra fields; allowed only at component end.
- complete: + statement,proof (entire expected canonical objects).
Anything else, state/receipt/seed/challenge keys, abort, EOF, timeout, malformed JSON => terminal rejection/failure; never ACCEPT.
V reply to each legal request: {status:CONTINUE,sid,seq,coins:[[label,fr_hex],...]}.
Challenge may arrive on open (aggregation and H-PC beta) or message ack (round/rho/c/chi/fold). Client challenge(label) only consumes queued public replies; it sends no challenge request. Client absorb sends message; client finish sends finish. V prefix absorbs identical verifier/label plus fr_hex.
Terminal reply {status:ACCEPT|REJECTED|FAILED|TIMED_OUT,sid,reason,...}; all failures close process nonzero. JSONL stderr audit records seq/event/label/prefix/coin/count/status (public transcript only).
## Modes and trusted server command
python3 -B research_only/verifier_server.py --mode relation|sumcheck|one|two|matrix --timeout 10
--rng-fail is trusted startup TEST fault flag, never a P message. Timeout override trusted tests only.
relation: bind legacy relation-test statement then one relation component.
sumcheck: bind make_statement(SUITE,sid,'sumcheck-opening', {pc,initial,degrees}); derived scst = make_sumcheck_statement('sc-'+digest(outer)[:40],initial,degrees,{'parent':digest(outer)}).
Then Sumcheck rounds + relation component; H-PC opening bridge binding=parent_binding(scst,scproof), one terminal handle and actual r-point. Final proof={suite:SUITE,sc_statement:scst,sumcheck:scproof,opening:bridgeproof}.
one/two/matrix: bind exact original _one_outer/_two_outer/_matrix_outer object, selected mode. Original prove_* creates this internally; client adapter wraps ONLY corresponding outer constructor to bind once at creation.
## Verifier reconstruction
Component open uses locally expected statement and issues initial coins if any. Sumcheck receives degree+1 validated coefficient handles per round, commits row, sends r, then terminal. finish closes round tape. Construct exact _relations and _relation_statement; verify relation batch from first,B,responses.
Relation: fixed validated spec/handles; first -> all rho; B -> c; responses -> finish; original verify_relations must pass.
H-PC: statement rebuilt via pc,points,handles,session. open -> beta; audit then each opening: AB -> c; response; computed IPA (R,t) statement must equal incoming -> chi; each LR -> fold; z_final; final finish and original hpc.verify.
GKR: use original helper functions for public polynomial coefficients, child statements, derived handle, parent digest. One/two/matrix build EXACT old proof shape. After L0 verified, aggregation open emits two independent lambda; finish; V derives lower initial before L1. Four relation c values for two layers.
Canonical JSON equality bridges tuple/list serialization only; mathematical verifier runs on V's reconstructed native objects. All byte identities, scalar identities and original equations remain intact.
## Tests and evidence
RED missing server/client integration test first; all 14 user attacks logged individually with attempted manipulation, actual terminal state/reason, verifier trace. Positive relation, d2/d3 SC+real opening (fresh vector), one/two GKR, matrix, bounded N1024, zero cases. Test PID isolation/proc memory denial; test live preselected simulator ban. Test duplicate-key/schema/EOF/timeout/final mutation. Do not run old single-process prove checks as evidence of repaired boundary.
## Security scope
No FS, malicious-V ZK, concurrent/reset ZK, overall GKR/Lasso knowledge, 893 openings or production R7 claim. Two-layer four c; linear compression bound conditional on pre-fixed nonzero errors; joint extractor remains PARTIAL. Task0-D execution only if this scoped boundary passes; never auto-start D.
