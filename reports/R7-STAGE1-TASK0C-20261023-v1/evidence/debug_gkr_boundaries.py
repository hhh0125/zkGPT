import hpc_protocol as hpc
import gkr
from transcript import SUITE

LOWER = [
    {"op": "add", "left": 0, "right": 1},
    {"op": "mul", "left": 1, "right": 2},
    {"op": "add", "left": 2, "right": 3},
    {"op": "mul", "left": 3, "right": 0},
]
UPPER = [
    {"op": "mul", "left": 0, "right": 1},
    {"op": "add", "left": 1, "right": 2},
    {"op": "mul", "left": 2, "right": 3},
    {"op": "add", "left": 3, "right": 0},
]

values = [2, 3, 5, 7]
pc, witness = hpc.commit(values, suite=SUITE)
statement, proof, receipts = gkr.prove_one_layer(pc, witness, values, LOWER, [11, 13], "gkr-one")
package = {"sumcheck": proof["sumcheck"], "endpoints": proof["endpoints"], "endpoint_relation": proof["endpoint_relation"]}
print("one.outer", statement == gkr._one_outer(pc, LOWER, [11, 13], statement["public"]["initial"], statement["session"]))
print("one.sumcheck", gkr.verify_sumcheck(proof["sc_statement"], proof["sumcheck"], receipts.layer.sumcheck))
ch = gkr._challenges(receipts.layer.sumcheck, 6)
gate, left, right = ch[:2], ch[2:4], ch[4:]
add_table, mul_table = gkr._indicator_tables(LOWER, 2)
sel = gkr._selector([(1, [11, 13])], gate)
coeff = (-sel * gkr._mle(add_table, ch) % gkr.Q, -sel * gkr._mle(add_table, ch) % gkr.Q, -sel * gkr._mle(mul_table, ch) % gkr.Q)
print("one.coefficients", coeff)
print("one.endpoint", gkr._verify_endpoints(proof["sc_statement"], proof["sumcheck"], proof["endpoints"], proof["endpoint_relation"], receipts.layer.endpoint, coeff))
state = gkr._verify_layer(gkr.digest(statement), "L0", statement["public"]["initial"], 4, LOWER, [(1, [11, 13])], proof["sc_statement"], package, receipts.layer)
print("one.layer", state)
if state:
    partial = {"sc_statement": proof["sc_statement"], **package}
    binding = gkr.digest([statement, partial])
    print("one.opening", gkr.verify_opening_bridge(pc, [state["left"], state["right"]], proof["endpoints"][:2], proof["opening"], receipts.opening, binding))

statement2, proof2, receipts2 = gkr.prove_two_layer(pc, witness, values, LOWER, UPPER, [17, 19], "gkr-two")
layer0 = proof2["layers"][0]
package0 = {key: layer0[key] for key in ("sumcheck", "endpoints", "endpoint_relation")}
print("two.layer0.sumcheck", gkr.verify_sumcheck(layer0["sc_statement"], layer0["sumcheck"], receipts2.layer0.sumcheck))
state0 = gkr._verify_layer(gkr.digest(statement2), "L0", statement2["public"]["initial"], 4, UPPER, [(1, [17, 19])], layer0["sc_statement"], package0, receipts2.layer0)
print("two.layer0", state0)
if state0:
    l0, l1, _ = gkr._aggregation(statement2, layer0, replay=receipts2.aggregation)
    expected = gkr._derived_handle(layer0["endpoints"], (l0, l1))
    print("two.derived", expected == proof2["derived_initial"])
    layer1 = proof2["layers"][1]
    package1 = {key: layer1[key] for key in ("sumcheck", "endpoints", "endpoint_relation")}
    selectors = [(l0, state0["left"]), (l1, state0["right"])]
    state1 = gkr._verify_layer(gkr.digest([statement2, layer0, expected]), "L1", expected, 4, LOWER, selectors, layer1["sc_statement"], package1, receipts2.layer1)
    print("two.layer1", state1)
