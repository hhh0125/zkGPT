#include "wire.hpp"
#include <fstream>
#include <algorithm>
namespace zkgpt::r1 {
namespace {
struct Writer {
    Bytes b;
    void u(std::uint64_t x) {
        for (unsigned j = 0; j < 8; ++j)
            b.push_back(std::uint8_t(x >> (8 * j)));
    }
    void text(const std::string &x) {
        u(x.size());
        b.insert(b.end(), x.begin(), x.end());
    }
    void f(const Fr &x) {
        auto raw = encode(x);
        b.insert(b.end(), raw.begin(), raw.end());
    }
    void g(const G1 &x) {
        auto raw = encode(x);
        b.insert(b.end(), raw.begin(), raw.end());
    }
    void fields(const std::vector<Fr> &v) {
        u(v.size());
        for (auto &x : v)
            f(x);
    }
    void spec(const PCSpec &s) {
        text(s.role);
        u(s.actual);
        u(s.padded);
        u(s.rows);
        u(s.columns);
        u(unsigned(s.layout));
    }
    void pc(const Commitment &c) {
        spec(c.spec);
        u(c.rows.size());
        for (auto &x : c.rows)
            g(x);
    }
    void opening(const OpenProof &p) {
        f(p.value);
        u(p.rounds.size());
        for (auto &q : p.rounds) {
            g(q.left);
            g(q.right);
        }
        f(p.final);
    }
    void sc(const SumcheckProof &p) {
        u(p.rounds.size());
        for (auto &q : p.rounds)
            fields(q);
    }
    void tree(const ProductProof &p) {
        u(p.layers.size());
        for (auto &q : p.layers) {
            sc(q.sumcheck);
            f(q.left);
            f(q.right);
        }
    }
    void memory(const MemoryProof &p) {
        pc(p.read);
        pc(p.final);
        for (auto &x : p.roots)
            f(x);
        for (auto &q : p.products) {
            tree(q.tree);
            u(q.leaf.size());
            for (auto &op : q.leaf)
                opening(op);
        }
    }
    void range(const RangeProof &p) {
        u(p.digits.size());
        for (auto &c : p.digits)
            pc(c);
        opening(p.query);
        u(p.digit_values.size());
        for (auto &op : p.digit_values)
            opening(op);
        u(p.memories.size());
        for (auto &m : p.memories)
            memory(m);
        sc(p.tail.sumcheck);
        opening(p.tail.endpoint);
    }
};
struct Reader {
    const Bytes &b;
    std::size_t at = 0;
    explicit Reader(const Bytes &v) : b(v) {
        require(b.size() <= kMaxWireBytes, "wire byte limit");
    }
    void room(std::size_t n) { require(n <= b.size() - at, "truncated artifact"); }
    std::uint64_t u() {
        room(8);
        std::uint64_t x = 0;
        for (unsigned j = 0; j < 8; ++j)
            x |= std::uint64_t(b[at++]) << (8 * j);
        return x;
    }
    void count(std::size_t expected) { require(u() == expected, "wire exact count"); }
    std::size_t bounded(std::size_t limit, std::size_t bytes) {
        auto n = u();
        require(n <= limit, "wire count limit");
        require(n <= (b.size() - at) / bytes, "truncated counted vector");
        return std::size_t(n);
    }
    std::string text(std::size_t limit) {
        auto n = bounded(limit, 1);
        std::string s(b.begin() + at, b.begin() + at + n);
        at += n;
        return s;
    }
    Bytes raw(std::size_t n) {
        room(n);
        Bytes v(b.begin() + at, b.begin() + at + n);
        at += n;
        return v;
    }
    Fr f() { return decodeFr(raw(32)); }
    G1 g() { return decodeG1(raw(G1::getSerializedByteSize())); }
    std::vector<Fr> fields(std::size_t limit) {
        auto n = bounded(limit, 32);
        std::vector<Fr> out;
        out.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            out.push_back(f());
        return out;
    }
    PCSpec spec() {
        auto role = text(128);
        auto actual = u(), n = u(), rows = u(), columns = u(), layout = u();
        require(layout <= 1, "wire PC layout");
        auto s = PCSpec::make(role, actual, n, Layout(layout));
        require(rows == s.rows && columns == s.columns, "wire PC shape");
        return s;
    }
    Commitment pc(const PCSpec &expected) {
        auto s = spec();
        require(s == expected, "wire expected PC identity");
        count(s.rows);
        room(s.rows * G1::getSerializedByteSize());
        Commitment c{s, {}};
        for (std::size_t i = 0; i < s.rows; ++i)
            c.rows.push_back(g());
        return c;
    }
    OpenProof opening(const PCSpec &s) {
        OpenProof p;
        p.value = f();
        auto n = log2exact(s.columns);
        count(n);
        room(n * 2 * G1::getSerializedByteSize() + 32);
        for (unsigned j = 0; j < n; ++j)
            p.rounds.push_back({g(), g()});
        p.final = f();
        return p;
    }
    SumcheckProof sc(unsigned d, unsigned degree) {
        count(d);
        SumcheckProof p;
        for (unsigned j = 0; j < d; ++j) {
            count(degree + 1);
            std::vector<Fr> v;
            for (unsigned k = 0; k <= degree; ++k)
                v.push_back(f());
            p.rounds.push_back(std::move(v));
        }
        return p;
    }
    ProductProof tree(std::size_t n) {
        auto d = log2exact(n);
        count(d);
        ProductProof p;
        for (unsigned j = 0; j < d; ++j) {
            ProductLayer q;
            q.sumcheck = sc(j, 3);
            q.left = f();
            q.right = f();
            p.layers.push_back(std::move(q));
        }
        return p;
    }
    MemoryProof memory(const MemorySpec &s) {
        MemoryProof p;
        p.read = pc(readSpec(s));
        p.final = pc(finalSpec(s));
        for (auto &x : p.roots)
            x = f();
        for (unsigned kind = 0; kind < 3; ++kind) {
            p.products[kind].tree = tree(kind == 2 ? s.table.size() : s.address.padded);
            count(kind == 2 ? 1 : 3);
            auto &ops = p.products[kind].leaf;
            if (kind == 2)
                ops.push_back(opening(finalSpec(s)));
            else {
                ops.push_back(opening(s.address));
                ops.push_back(opening(s.value));
                ops.push_back(opening(readSpec(s)));
            }
        }
        return p;
    }
    RangeProof range(const RangeSpec &s) {
        RangeProof p;
        auto digits = (s.bits + 8) / 9;
        count(digits);
        for (unsigned h = 0; h < digits; ++h)
            p.digits.push_back(pc(digitSpec(s, h)));
        p.query = opening(s.query);
        count(digits);
        for (unsigned h = 0; h < digits; ++h)
            p.digit_values.push_back(opening(digitSpec(s, h)));
        count(digits);
        for (unsigned h = 0; h < digits; ++h)
            p.memories.push_back(memory(digitMemory(s, h)));
        p.tail.sumcheck = sc(log2exact(s.query.padded), 2);
        p.tail.endpoint = opening(s.query);
        return p;
    }
    void done() { require(at == b.size(), "trailing artifact bytes"); }
};
void header(Writer &w, unsigned kind) {
    w.text("ZKGPTR1");
    w.u(1);
    w.u(kind);
}
void header(Reader &r, unsigned kind) {
    require(r.text(16) == "ZKGPTR1" && r.u() == 1 && r.u() == kind, "artifact magic/version/kind");
}
} // namespace
Bytes serialize(const BundleStatement &s) {
    s.validate();
    Writer w;
    header(w, 1);
    w.text(s.input.context);
    w.fields(s.input.x);
    for (unsigned b = 0; b < 3; ++b) {
        w.spec(s.input.spec[b]);
        w.pc(s.input.commitment[b]);
    }
    w.text(s.gkr.context);
    w.u(s.gkr.input_size);
    w.u(s.gkr.layers.size());
    for (auto &layer : s.gkr.layers) {
        w.u(layer.size());
        for (auto &g : layer) {
            w.u(unsigned(g.kind));
            w.u(g.left);
            w.u(g.right);
            w.f(g.coefficient);
        }
    }
    w.fields(s.gkr.output);
    w.text(s.range.id);
    w.spec(s.range.query);
    w.u(s.range.bits);
    w.text(s.indexed.id);
    w.spec(s.indexed.address);
    w.spec(s.indexed.value);
    w.fields(s.indexed.table);
    require(w.b.size() <= kMaxWireBytes, "wire size");
    return w.b;
}
BundleStatement parseStatement(const Bytes &b) {
    Reader r(b);
    header(r, 1);
    BundleStatement s;
    s.input.context = r.text(4096);
    s.input.x = r.fields(1 << 20);
    log2exact(s.input.x.size());
    for (unsigned j = 0; j < 3; ++j) {
        s.input.spec[j] = r.spec();
        s.input.commitment[j] = r.pc(s.input.spec[j]);
    }
    s.gkr.context = r.text(4096);
    s.gkr.input_size = r.u();
    auto layers = r.bounded(4096, 8);
    std::size_t total = 0;
    for (std::size_t j = 0; j < layers; ++j) {
        auto n = r.bounded(1 << 20, 56);
        require(total + n <= (1 << 20), "wire total gate bound");
        total += n;
        std::vector<Gate> layer;
        for (std::size_t i = 0; i < n; ++i) {
            auto kind = r.u(), left = r.u(), right = r.u();
            require(kind <= 1, "wire gate kind");
            layer.push_back({GateKind(kind), std::size_t(left), std::size_t(right), r.f()});
        }
        s.gkr.layers.push_back(std::move(layer));
    }
    s.gkr.output = r.fields(1 << 20);
    s.range.id = r.text(55);
    s.range.query = r.spec();
    auto bits = r.u();
    require(bits <= 126, "wire Range bits");
    s.range.bits = unsigned(bits);
    s.indexed.id = r.text(80);
    s.indexed.address = r.spec();
    s.indexed.value = r.spec();
    s.indexed.table = r.fields(1 << 20);
    r.done();
    s.validate();
    return s;
}
Bytes serialize(const BundleStatement &s, const BundleProof &p) {
    Writer w;
    header(w, 2);
    w.range(p.range);
    w.opening(p.indexed_value);
    w.memory(p.memory);
    w.u(p.gkr.layers.size());
    for (auto &q : p.gkr.layers) {
        w.sc(q.sumcheck);
        w.f(q.left);
        w.f(q.right);
    }
    w.sc(p.input.local);
    w.sc(p.input.selectors);
    for (auto &q : p.input.opening)
        w.opening(q);
    for (auto &q : p.input.padding) {
        w.sc(q.sumcheck);
        w.opening(q.endpoint);
    }
    require(w.b.size() <= kMaxWireBytes, "wire size");
    (void)s;
    return w.b;
}
BundleProof parseProof(const BundleStatement &s, const Bytes &b) {
    s.validate();
    Reader r(b);
    header(r, 2);
    BundleProof p;
    p.range = r.range(s.range);
    p.indexed_value = r.opening(s.input.spec[2]);
    p.memory = r.memory(s.indexed);
    r.count(s.gkr.layers.size());
    for (std::size_t i = s.gkr.layers.size(); i > 0; --i) {
        GkrLayerProof q;
        q.sumcheck = r.sc(2 * log2exact(i == 1 ? s.gkr.input_size : s.gkr.layers[i - 2].size()), 2);
        q.left = r.f();
        q.right = r.f();
        p.gkr.layers.push_back(std::move(q));
    }
    p.input.local = r.sc(log2exact(s.input.x.size()), 2);
    p.input.selectors = r.sc(2, 2);
    for (unsigned j = 0; j < 3; ++j)
        p.input.opening[j] = r.opening(s.input.spec[j]);
    for (unsigned j = 0; j < 3; ++j) {
        p.input.padding[j].sumcheck = r.sc(log2exact(s.input.x.size()), 2);
        p.input.padding[j].endpoint = r.opening(s.input.spec[j]);
    }
    r.done();
    return p;
}
void ExpStatement::validate() const {
    spec.validate();
    require(index.spec == spec.index && value.spec == spec.value, "Exp artifact source identity");
    require(spec.index.padded <= (1U << 20), "Exp artifact deployment size limit");
}
Bytes serializeExp(const ExpStatement &s) {
    s.validate();
    require(s.spec.profile==ExpProfile::Unclamped,"legacy standalone Exp codec requires unclamped profile; use LookupBatch for explicit profiles");
    Writer w;
    header(w, 3);
    w.text(s.spec.id);
    w.spec(s.spec.index);
    w.spec(s.spec.value);
    w.pc(s.index);
    w.pc(s.value);
    return w.b;
}
ExpStatement parseExpStatement(const Bytes &b) {
    Reader r(b);
    header(r, 3);
    ExpStatement s;
    s.spec.id = r.text(35);
    s.spec.index = r.spec();
    s.spec.value = r.spec();
    s.index = r.pc(s.spec.index);
    s.value = r.pc(s.spec.value);
    r.done();
    s.validate();
    return s;
}
Bytes serializeExp(const ExpStatement &s, const ExpProof &p) {
    s.validate();
    require(s.spec.profile==ExpProfile::Unclamped,"legacy standalone Exp codec requires unclamped profile; use LookupBatch for explicit profiles");
    Writer w;
    header(w, 4);
    for (auto &c : p.helper)
        w.pc(c);
    for (auto &range : p.range)
        w.range(range);
    for (unsigned k = 0; k < 2; ++k) {
        w.sc(p.constraints[k]);
        for (auto &op : p.endpoint[k])
            w.opening(op);
    }
    for (auto &op : p.value_binding)
        w.opening(op);
    w.memory(p.memory);
    require(w.b.size() <= kMaxWireBytes, "Exp wire size");
    return w.b;
}
ExpProof parseExpProof(const ExpStatement &s, const Bytes &b) {
    s.validate();
    require(s.spec.profile==ExpProfile::Unclamped,"legacy standalone Exp codec requires unclamped profile");
    Reader r(b);
    header(r, 4);
    ExpProof p;
    for (unsigned k = 0; k < 4; ++k)
        p.helper[k] = r.pc(expHelperSpec(s.spec, k));
    for (unsigned k = 0; k < 4; ++k)
        p.range[k] = r.range(expRangeSpec(s.spec, k));
    for (unsigned k = 0; k < 2; ++k) {
        p.constraints[k] = r.sc(log2exact(s.spec.index.padded), 3);
        p.endpoint[k][0] = r.opening(s.spec.index);
        p.endpoint[k][1] = r.opening(expHelperSpec(s.spec, 1));
        p.endpoint[k][2] = r.opening(expHelperSpec(s.spec, k == 0 ? 0 : 2));
    }
    p.value_binding[0] = r.opening(s.spec.value);
    p.value_binding[1] = r.opening(expHelperSpec(s.spec, 3));
    p.memory = r.memory(expMemorySpec(s.spec));
    r.done();
    return p;
}
void DivisionStatement::validate() const {
    spec.validate();
    require(numerator.spec == spec.numerator && denominator.spec == spec.denominator &&
                quotient.spec == spec.quotient,
            "division artifact source identity");
    require(spec.numerator.padded <= (1U << 20), "division artifact deployment size limit");
}
Bytes serializeDivision(const DivisionStatement &s) {
    s.validate();
    Writer w;
    header(w, 5);
    w.text(s.spec.id);
    w.spec(s.spec.numerator);
    w.spec(s.spec.denominator);
    w.spec(s.spec.quotient);
    w.u(s.spec.numerator_bits);
    w.u(s.spec.quotient_bits);
    w.u(s.spec.denominator_bits);
    w.pc(s.numerator);
    w.pc(s.denominator);
    w.pc(s.quotient);
    return w.b;
}
DivisionStatement parseDivisionStatement(const Bytes &b) {
    Reader r(b);
    header(r, 5);
    DivisionStatement s;
    s.spec.id = r.text(32);
    s.spec.numerator = r.spec();
    s.spec.denominator = r.spec();
    s.spec.quotient = r.spec();
    auto ab = r.u(), qb = r.u(), bb = r.u();
    require(ab <= 64 && qb <= 64 && bb <= 62, "wire division bits");
    s.spec.numerator_bits = unsigned(ab);
    s.spec.quotient_bits = unsigned(qb);
    s.spec.denominator_bits = unsigned(bb);
    s.numerator = r.pc(s.spec.numerator);
    s.denominator = r.pc(s.spec.denominator);
    s.quotient = r.pc(s.spec.quotient);
    r.done();
    s.validate();
    return s;
}
Bytes serializeDivision(const DivisionStatement &s, const DivisionProof &p) {
    s.validate();
    Writer w;
    header(w, 6);
    for (auto &c : p.helper)
        w.pc(c);
    for (auto &q : p.range)
        w.range(q);
    for (auto &q : p.signed_binding) {
        w.opening(q.source);
        w.opening(q.encoded);
    }
    for (auto &q : p.positive_binding)
        w.opening(q);
    for (unsigned k = 0; k < 2; ++k) {
        w.sc(p.constraints[k]);
        for (auto &q : p.endpoint[k])
            w.opening(q);
    }
    require(w.b.size() <= kMaxWireBytes, "division wire size");
    return w.b;
}
DivisionProof parseDivisionProof(const DivisionStatement &s, const Bytes &b) {
    s.validate();
    Reader r(b);
    header(r, 6);
    DivisionProof p;
    for (unsigned k = 0; k < 5; ++k)
        p.helper[k] = r.pc(divisionHelperRange(s.spec, k).query);
    for (unsigned k = 0; k < 5; ++k)
        p.range[k] = r.range(divisionHelperRange(s.spec, k));
    p.signed_binding[0].source = r.opening(s.spec.numerator);
    p.signed_binding[0].encoded = r.opening(divisionHelperRange(s.spec, 0).query);
    p.signed_binding[1].source = r.opening(s.spec.quotient);
    p.signed_binding[1].encoded = r.opening(divisionHelperRange(s.spec, 1).query);
    p.positive_binding[0] = r.opening(s.spec.denominator);
    p.positive_binding[1] = r.opening(divisionHelperRange(s.spec, 2).query);
    for (unsigned k = 0; k < 2; ++k) {
        p.constraints[k] = r.sc(log2exact(s.spec.numerator.padded), 3);
        p.endpoint[k][0] = r.opening(s.spec.numerator);
        p.endpoint[k][1] = r.opening(s.spec.denominator);
        p.endpoint[k][2] = r.opening(s.spec.quotient);
        p.endpoint[k][3] = r.opening(divisionHelperRange(s.spec, 3).query);
        p.endpoint[k][4] = r.opening(divisionHelperRange(s.spec, 4).query);
    }
    r.done();
    return p;
}
void PositiveSqrtStatement::validate() const {
    spec.validate();
    require(input.spec == spec.input && root.spec == spec.root, "sqrt artifact source identity");
    require(spec.input.padded <= (1U << 20), "sqrt artifact deployment size limit");
}
Bytes serializePositiveSqrt(const PositiveSqrtStatement &s) {
    s.validate();
    Writer w;
    header(w, 7);
    w.text(s.spec.id);
    w.spec(s.spec.input);
    w.spec(s.spec.root);
    w.u(s.spec.input_bits);
    w.u(s.spec.root_bits);
    w.pc(s.input);
    w.pc(s.root);
    return w.b;
}
PositiveSqrtStatement parsePositiveSqrtStatement(const Bytes &b) {
    Reader r(b);
    header(r, 7);
    PositiveSqrtStatement s;
    s.spec.id = r.text(32);
    s.spec.input = r.spec();
    s.spec.root = r.spec();
    auto xb = r.u(), yb = r.u();
    require(xb <= 64 && yb <= 63, "wire sqrt bits");
    s.spec.input_bits = unsigned(xb);
    s.spec.root_bits = unsigned(yb);
    s.input = r.pc(s.spec.input);
    s.root = r.pc(s.spec.root);
    r.done();
    s.validate();
    return s;
}
Bytes serializePositiveSqrt(const PositiveSqrtStatement &s, const PositiveSqrtProof &p) {
    s.validate();
    Writer w;
    header(w, 8);
    for (auto &c : p.helper)
        w.pc(c);
    for (auto &q : p.range)
        w.range(q);
    for (auto &q : p.positive_binding)
        w.opening(q);
    for (unsigned k = 0; k < 2; ++k) {
        w.sc(p.constraints[k]);
        for (auto &q : p.endpoint[k])
            w.opening(q);
    }
    require(w.b.size() <= kMaxWireBytes, "sqrt wire size");
    return w.b;
}
PositiveSqrtProof parsePositiveSqrtProof(const PositiveSqrtStatement &s, const Bytes &b) {
    s.validate();
    Reader r(b);
    header(r, 8);
    PositiveSqrtProof p;
    for (unsigned k = 0; k < 3; ++k)
        p.helper[k] = r.pc(positiveSqrtHelperSpec(s.spec, k));
    for (unsigned k = 0; k < 4; ++k)
        p.range[k] = r.range(positiveSqrtRangeSpec(s.spec, k));
    p.positive_binding[0] = r.opening(s.spec.root);
    p.positive_binding[1] = r.opening(positiveSqrtHelperSpec(s.spec, 0));
    for (unsigned k = 0; k < 2; ++k) {
        p.constraints[k] = r.sc(log2exact(s.spec.input.padded), 3);
        p.endpoint[k][0] = r.opening(s.spec.input);
        p.endpoint[k][1] = r.opening(s.spec.root);
        p.endpoint[k][2] = r.opening(positiveSqrtHelperSpec(s.spec, k + 1));
    }
    r.done();
    return p;
}
Bytes readFile(const std::string &path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    require(bool(f), "open artifact");
    auto n = f.tellg();
    require(n >= 0 && std::uint64_t(n) <= kMaxWireBytes, "artifact file size");
    Bytes b(static_cast<std::size_t>(n));
    f.seekg(0);
    if (!b.empty())
        f.read(reinterpret_cast<char *>(b.data()), b.size());
    require(bool(f), "read artifact");
    return b;
}
void writeFile(const std::string &path, const Bytes &b) {
    require(b.size() <= kMaxWireBytes, "wire size");
    std::ofstream f(path, std::ios::binary);
    require(bool(f), "write artifact open");
    f.write(reinterpret_cast<const char *>(b.data()), b.size());
    require(bool(f), "write artifact");
}
} // namespace zkgpt::r1

namespace zkgpt::r1 {
Bytes serializeMappedInput(const LookupBatchSpec &s,const MappedInputProof &p) {
    s.validate();Writer w;header(w,14);auto digest=hash(serializeLookup(s));
    w.b.insert(w.b.end(),digest.begin(),digest.end());
    w.u(p.children.size());for(const auto &op:p.children)w.opening(op);
    w.sc(p.sumcheck);w.opening(p.master);
    require(w.b.size()<=kMaxWireBytes,"mapped source wire size");return w.b;
}
MappedInputProof parseMappedInput(const LookupBatchSpec &s,const Bytes &b) {
    s.validate();Reader r(b);header(r,14);auto digest=hash(serializeLookup(s));
    require(r.raw(32)==Bytes(digest.begin(),digest.end()),"mapped source statement binding");
    MappedInputProof p;r.count(s.input.children.size());
    for(const auto &c:s.input.children)p.children.push_back(r.opening(c.commitment.spec));
    p.sumcheck=r.sc(log2exact(s.input.master.spec.padded),2);p.master=r.opening(s.input.master.spec);
    r.done();return p;
}
}

namespace zkgpt::r1 {
Bytes serializeLookup(const LookupBatchSpec &s) {
    s.validate();Writer w;header(w,12);w.text(s.input.context);w.pc(s.input.master);
    w.u(s.input.children.size());
    for (const auto &c:s.input.children) {
        w.pc(c.commitment);w.u(c.spans.size());
        for (const auto &v:c.spans) {w.u(v.offset);w.u(v.count);w.f(v.bias);}
    }
    w.u(s.input.public_values.size());
    for (const auto &v:s.input.public_values) {w.u(v.index);w.f(v.value);}
    w.u(s.ranges.size());for (const auto &r:s.ranges) {w.u(r.child);w.text(r.id);w.u(r.bits);}
    w.u(s.exps.size());for (const auto &e:s.exps) {w.u(e.index_child);w.u(e.value_child);w.text(e.id);w.u(unsigned(e.profile));}
    require(w.b.size()<=kMaxWireBytes,"lookup statement size");return w.b;
}
LookupBatchSpec parseLookupSpec(const Bytes &b) {
    Reader r(b);header(r,12);LookupBatchSpec s;s.input.context=r.text(4096);
    auto pc=[&]() {auto at=r.at;auto spec=r.spec();r.at=at;return r.pc(spec);};
    s.input.master=pc();auto n=r.bounded(65536,64);
    for (std::size_t i=0;i<n;++i) {
        MappedVector c;c.commitment=pc();auto spans=r.bounded(kMaxVector,48);
        for (std::size_t j=0;j<spans;++j) {auto offset=r.u(),count=r.u();auto bias=r.f();c.spans.push_back({offset,count,bias});}
        s.input.children.push_back(std::move(c));
    }
    n=r.bounded(kMaxVector,40);
    for (std::size_t i=0;i<n;++i) {auto index=r.u();auto value=r.f();s.input.public_values.push_back({index,value});}
    n=r.bounded(65536,24);
    for (std::size_t i=0;i<n;++i) {auto child=r.u();auto id=r.text(55);auto bits=r.u();require(bits<=126,"lookup Range bits");s.ranges.push_back({child,id,unsigned(bits)});}
    n=r.bounded(65536,24);
    for (std::size_t i=0;i<n;++i) {auto a=r.u(),e=r.u();auto id=r.text(35);auto profile=r.u();require(profile<=1,"lookup Exp profile");s.exps.push_back({a,e,id,static_cast<ExpProfile>(profile)});}
    r.done();s.validate();return s;
}
Bytes serializeLookup(const LookupBatchSpec &s,const LookupBatchProof &p) {
    s.validate();Writer w;header(w,13);auto digest=hash(serializeLookup(s));w.b.insert(w.b.end(),digest.begin(),digest.end());
    require(p.ranges.size()==s.ranges.size() && p.exps.size()==s.exps.size(),"lookup wire obligation count");
    w.u(p.ranges.size());for (const auto &q:p.ranges) w.range(q);
    w.u(p.exps.size());for (const auto &q:p.exps) {
        for (const auto &c:q.helper) w.pc(c);
        for (const auto &range:q.range) w.range(range);
        for (unsigned k=0;k<2;++k) {w.sc(q.constraints[k]);for (const auto &op:q.endpoint[k]) w.opening(op);}
        for (const auto &op:q.value_binding) w.opening(op);
        w.memory(q.memory);
    }
    w.u(p.input.children.size());for (const auto &op:p.input.children) w.opening(op);
    w.sc(p.input.sumcheck);w.opening(p.input.master);
    require(w.b.size()<=kMaxWireBytes,"lookup proof size");return w.b;
}
LookupBatchProof parseLookupProof(const LookupBatchSpec &s,const Bytes &b) {
    s.validate();Reader r(b);header(r,13);auto digest=hash(serializeLookup(s));
    require(r.raw(32)==Bytes(digest.begin(),digest.end()),"lookup statement binding");
    LookupBatchProof p;r.count(s.ranges.size());
    for (std::size_t j=0;j<s.ranges.size();++j) p.ranges.push_back(r.range(rangeSpec(s,j)));
    r.count(s.exps.size());
    for (std::size_t j=0;j<s.exps.size();++j) {
        auto es=expSpec(s,j);ExpProof q;
        for (unsigned k=0;k<4;++k) q.helper[k]=r.pc(expHelperSpec(es,k));
        for (unsigned k=0;k<4;++k) q.range[k]=r.range(expRangeSpec(es,k));
        for (unsigned k=0;k<2;++k) {
            q.constraints[k]=r.sc(log2exact(es.index.padded),3);
            q.endpoint[k][0]=r.opening(es.index);q.endpoint[k][1]=r.opening(expHelperSpec(es,1));
            q.endpoint[k][2]=r.opening(expHelperSpec(es,k==0?0:2));
        }
        q.value_binding[0]=r.opening(es.value);q.value_binding[1]=r.opening(expHelperSpec(es,3));
        q.memory=r.memory(expMemorySpec(es));p.exps.push_back(std::move(q));
    }
    r.count(s.input.children.size());
    for (const auto &c:s.input.children) p.input.children.push_back(r.opening(c.commitment.spec));
    p.input.sumcheck=r.sc(log2exact(s.input.master.spec.padded),2);p.input.master=r.opening(s.input.master.spec);
    r.done();return p;
}
} // namespace zkgpt::r1
