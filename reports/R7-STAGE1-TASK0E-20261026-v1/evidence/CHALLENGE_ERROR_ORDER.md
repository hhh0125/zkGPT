# E challenge/error order audit
Status: CONDITIONAL / LOCALLY_PROVEN algebraic bounds. Semantic prebinding still requires the joint extraction premise.

| challenge | already fixed error data | missing global obligation |
|---|---|---|
|memory gamma/tau|all registry PCs: Cm, query U/read/final, remaining children|one semantic vector per PC across forks|
|tree points/SC rounds|all3 tree PC hierarchies; each degree3 coefficient row before its round coin|same nodes/leaf/base vector extraction|
|GKR layer SC|fixed input/child PCs; each degree2 coefficient row|cross-layer representation compatibility|
|between-layer lambdas|both complete upper endpoint handles and checked layer|pre-lambda extracted value consistency|
|matrix parent points|matrix child and Cm output slice fixed in registry|same source vectors across both parents|
|internal theta|exact8 computation handles/points, including4 matrix values|common master attribution is proved by this reduction|
|mapped child points|all child PCs and Cm fixed|same vector in D/GKR and mapped opening|
|mapped lambda/theta/public nu|all child evaluation handles already authenticated;8 fixed computation handles; public rows fixed|their errors versus m fixed under conditional premise|
|tail point t|Cm fixed before all coins; tail value is computed after t|tail scalar is not pretended to precede t|
|tail omega|t and fixed Cm tail determine e_t before omega|same m versus later endpoint extraction|
|Sigma rho/c|all full handles/product first messages before rho; B before c|post-rho extraction cannot assert a fixed error vector without reduction|

The implementation draws lambda/theta, public nu, then t, then omega. A single fictitious error vector existing before ALL these messages is unnecessary and is not assumed:
- If the fixed master tail vector is nonzero, e_t=sum_tail w_t(i)m_i is nonzero except probability<=6/(q-1). On that event omega is independent, drawn AFTER e_t is determined, so it cancels the already-fixed prior weighted error with probability<=1/(q-1).
- If the fixed master tail vector is identically zero, its contribution is always zero. Any remaining nonzero child/checked/public error is fixed before its own compression weights; choose one nonzero coefficient's variable to obtain the1/(q-1) bound. Later independent t has no effect.
Thus the conservative tail-plus-compression bound used in SECURITY is valid by cases; it does not silently move an adaptive witness before a challenge. If extracted witnesses change after these coins, this argument no longer applies: joint status PARTIAL.
