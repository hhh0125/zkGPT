# Task0-D exact indexed-memory protocol
Status: DESIGNED; equations LOCALLY_PROVEN; end-to-end soundness CONDITIONAL.
Research only. Production BLOCKED_CRYPTOGRAPHIC_DESIGN; may_enter_stage2=false.

## Instance and identities
q is the inherited prime scalar order. Table T has M unique public addresses 0..M-1, M a power of two <=64, and may contain duplicate values. The trusted verifier owns T and its digest, not a table supplied as verification authority by P.
N is the padded query length, a power of two <=512 in this implementation: packed master has length 2N<=1024. Actual query length n<=N is public. All N queries participate, including padding.
Packed master U[2i]=a_i, U[2i+1]=v_i, with low-bit-first MLE indexing. QueryAddressPC and QueryValuePC are typed views (selector 0 and 1) of exactly this same physical blinded PC, not independent commitments assumed equal. R[i]=read_i and F[u]=final_u have separate PCs. Each PC uses actual=padded, so padding is committed data. Registry fixes full PC bytes, layout, dimensions, suite, role, session and view selectors before gamma/tau.
For i>=n the master is opened at Boolean points [0,bits(i)], [1,bits(i)] and linked to public 0,T[0]. This authenticates the declared padding policy; read values are still unrestricted field elements. No timestamp range proof or temporal-order predicate.
Every PC is Pedersen row commitment A_j=sum_k w_jk G_k+rho_j H. HiddenScalar C_x=xG+eta_x H has full (suite,id,point) identity; labels alone never authorize replacement.
All mathematical primitives retain Task0-B/C equations. Outer transport/domain is Task0-D only; copied primitive suite is retained for byte-identical algebra and domain is bound through every child statement.

## Ordered messages
1. V READY: independent session identity. P bind exact registry (master,read,final PCs; n,N; table digest; query views). V validates its table, shape, roles and points. V then independently samples nonzero gamma,tau, stores prefix and sends them. No P coin/state/receipt input.
2. P fixes all three complete tree PC hierarchies and three root handles in one packet, after gamma/tau but before tree tests. Tree k has levels C[k,j], j=0..log2 L_k, lengths 2^j; L_write=L_read=N,L_final=M. Extra root-product handle Zrf is included. V fixes bytes in session transcript.
3. Open each level-0 PC at empty point to the exact root handle. Prove RootRead*RootFinal=Zrf and Initial*RootWrite-Zrf=0. Initial=product_u(gamma^2*u+gamma*T[u]-tau), computed by V. No division and no zero-root shortcut.
4. For each tree/level j<log2 L: V samples r in (Fr*)^j only after all tree PCs fixed. P sends hidden parent evaluation S and opens C[k,j] at r to that same S. Run degree-3 committed SC for
   S = sum_{b in {0,1}^j} eq(r,b) L(b) R(b),
   L(x)=MLE(C[k,j+1], [0]+x), R(x)=MLE(C[k,j+1], [1]+x).
   In each SC round commit all four coefficient handles before V's r_i. The relation batch proves
   2c_i0+sum_{t=1}^3 c_it = (i=0 ? S : sum_t c_(i-1)t r_(i-1)^t),
   terminal=sum_t c_last,t r_last^t.
   j=0 has no rounds and proves terminal=S.
   P commits X,Y,Z; prove Z=XY and terminal=eq(r,u)Z. Open child PC at [0]+u,[1]+u to exactly X,Y in one K=2 batch. No raw endpoints.
5. For each leaf level V samples independent p after tree PCs fixed. P supplies hidden leaf E, opens leaf PC at p to E.
   Write/read: open master at [0]+p,[1]+p to A,V and read PC at p to R. Prove
   E-gamma^2 A-gamma V-R + tau-delta=0,
   delta_write=1,delta_read=0.
   Final: open final PC at p to F and prove
   E-F-gamma^2 Addr(p)-gamma Table(p)+tau=0.
   Addr and Table MLE are evaluated by V from its fixed public table/index convention.
6. Padding openings/linear equations as above. V checks every component and exact final accumulated statement/proof bytes, then ACCEPT. Missing messages, failures and timeouts fail closed.

Every tree PC is fixed before its random evaluation tests. Each layer separately authenticates its parent and children: this is intentionally more costly than recursive aggregation and avoids introducing new adaptive aggregation in D. Every internal-node multiplication is covered by the layer polynomial; authentication is randomized, not an enumeration claim.

## Exact scalar Sigma checks
For each handle C_h and first A_h: z_h G+zeta_h H=A_h+c C_h.
For each fixed linear equation b+sum a_h x_h=0 let D=bG+sum a_h C_h. After all first messages V draws independent rho_e; P commits B, then V draws this batch's own c. Check B+c sum_e rho_e D_e=zDelta H.
For Z=XY, first alpha,beta,delta and responses z1..z5:
alpha+cX=z1G+z2H; beta+cY=z3G+z4H; delta+cZ=z3 X+z5 H.
All equations are checked, including zero values. Each batch has an independent c; no global c.

## Exact committed opening
For row-weighted commitment xi and scalar handle Y, public column weights a, first A,B then V c:
Rz=c xi+A-sH; cY+B=tG+uH.
Response IPA proves Rz=sum z_j G_j AND t=<a,z>, with fresh nonzero chi then nonzero fold challenges after each L/R. Its commitment is U=Rz+chi*tG=sum z_j(G_j+chi*a_j G). At fold f: U'=U+f^2 L+f^-2 R; bases'=f^-1 bases_L+f bases_R; final U=z_final*base_final. No raw z vector on wire.
Each opening batch repeats the row audit at fresh beta with xi=sum beta^j A_j and public weight zero/output identity; K<=4. D does not amortize audits. Full inherited verifier checks are reused byte-for-byte.

## Privacy and bounds
Public: dimensions, n, table identity/values, PC/handle group elements, independent V coins, masked Sigma responses/IPA responses, component structure. Secret: a,v,read,final, tree nodes/roots, all true MLE evaluations, coefficient scalars and blinding. No raw secret scalar is a protocol field.
This exact design is sufficient to implement a research toy, conditional on B/C local primitives. Joint knowledge and production FS remain unresolved. D does not alter numerical Profile, R6 or a real W anchor.

## Trusted zero-factor diagnostic exception (TOY_TESTED control only)
Only normal mode memory implements independent gamma,tau in Fr*. The trusted launcher's separate memory-zero mode sets tau=gamma^2+7gamma, conditioning on the public initial factor at address1 being zero. It preserves the equation checks but deliberately changes the challenge distribution. With a valid (1,7,0) read factor, Initial=RootRead=0 can make the root equation hold even with unrelated invalid queries. Its ACCEPT is DIAGNOSTIC_ONLY and excluded from every soundness bound and research protocol acceptance claim. It tests that zero values still execute all trees/openings. Test D_diagnostic_invalid_other intentionally demonstrates this acceptance exception; the same invalid lookup in normal mode rejects. P cannot select this mode through a packet; trusted launcher controls it. This is a known random-evaluation exceptional event, not a zero-root bypass or evidence against the formal identity lemma.
