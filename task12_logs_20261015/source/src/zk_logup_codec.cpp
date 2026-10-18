#include "range_logup.hpp"
#include <algorithm>
namespace zkgpt::zk {
namespace {
constexpr uint64_t statementMagic=0x31534b5a554c46ULL,proofMagic=0x31504b5a554c46ULL;
constexpr size_t maxStatement=64ULL<<20,maxProof=32ULL<<20;
void stringWrite(Writer& w,const std::string& s){w.u64(s.size());w.bytes(Bytes(s.begin(),s.end()));}
std::string stringRead(Reader& r){auto n=r.u64();if(n>kMaxContextBytes)throw std::invalid_argument("LogUp string bound");auto b=r.bytes(n);return {b.begin(),b.end()};}
void matrixWrite(Writer& w,const MatrixCommitment& c){w.u64(c.size);w.u64(static_cast<uint64_t>(c.layout));w.u64(c.rows.size());for(const auto& row:c.rows)w.point(row);}
MatrixCommitment matrixRead(Reader& r,size_t expected=0){
    const auto n=r.u64(),layout=r.u64(),rows=r.u64();
    if(!n || n>(1ULL<<30) || (expected&&n!=expected) || layout!=1 || rows!=matrixShape(n).rows)
        throw std::invalid_argument("LogUp matrix decode shape");
    MatrixCommitment c{n,Layout::ColumnMajor,{}};c.rows.reserve(rows);
    for(size_t i=0;i<rows;++i)c.rows.push_back(r.point());return c;
}
size_t d1(size_t L){return 136+maskedSumcheckEncodedSize(L,1);}
size_t d3(size_t L){return 456+maskedSumcheckEncodedSize(L,3);}
size_t lin(size_t c){return 136+32*c;}
size_t proofSize(const FieldLogUpZkStatement& s){
    const auto q=matrixShape(s.query.size),t=matrixShape(s.table.size());
    const size_t result=24+3*24+32*(q.rows+2*t.rows)+64+
        d3(q.log_size)+d3(t.log_size)+d1(q.log_size)+d1(t.log_size)+64+
        3*lin(q.columns)+3*lin(t.columns)+32;
    if(result>maxProof)throw std::invalid_argument("LogUp proof bound");return result;
}
}
Bytes encodeFieldLogUpZkStatement(const FieldLogUpZkStatement& s){
    validateFieldLogUpZkStatement(s);Writer w(maxStatement);w.u64(statementMagic);
    w.u64(s.query_index);w.u64(s.actual_query_size);matrixWrite(w,s.query);
    stringWrite(w,s.generator_domain.domain);w.u64(s.generator_domain.count);
    w.u64(s.table.size());for(const auto& x:s.table)w.fr(x);
    stringWrite(w,s.table_id);stringWrite(w,s.context);
    w.bytes(Bytes(s.circuit_id.begin(),s.circuit_id.end()));w.u64(s.layer_id);w.fr(s.padding_value);
    return w.finish();
}
FieldLogUpZkStatement decodeFieldLogUpZkStatement(const Bytes& bytes){
    if(bytes.size()>maxStatement)throw std::invalid_argument("LogUp statement bound");
    Reader r(bytes,bytes.size());if(r.u64()!=statementMagic)throw std::invalid_argument("LogUp statement version");
    FieldLogUpZkStatement s;s.query_index=r.u64();s.actual_query_size=r.u64();s.query=matrixRead(r);
    s.generator_domain.domain=stringRead(r);const auto columns=r.u64();
    if(columns!=matrixShape(s.query.size).columns)throw std::invalid_argument("LogUp generator count");
    s.generator_domain.count=static_cast<uint32_t>(columns);const auto m=r.u64();
    if(!m || m>(1ULL<<20) || (m&(m-1)))throw std::invalid_argument("LogUp public table bound");
    s.table.reserve(m);for(size_t i=0;i<m;++i)s.table.push_back(r.fr());
    s.table_id=stringRead(r);s.context=stringRead(r);auto circuit=r.bytes(32);
    std::copy(circuit.begin(),circuit.end(),s.circuit_id.begin());s.layer_id=r.u64();s.padding_value=r.fr();
    r.finish();validateFieldLogUpZkStatement(s);return s;
}
Bytes encodeFieldLogUpZkProof(const FieldLogUpZkStatement& s,const FieldLogUpZkProof& p){
    validateFieldLogUpZkStatement(s);const auto q=matrixShape(s.query.size),t=matrixShape(s.table.size());
    Writer w(proofSize(s));w.u64(proofMagic);w.u64(q.size);w.u64(t.size);
    matrixWrite(w,p.Cm);matrixWrite(w,p.Ca);matrixWrite(w,p.Cb);w.point(p.D_M);w.point(p.D_S);
    w.bytes(encodeDegree3Zk(p.table_identity,t.log_size));w.bytes(encodeDegree3Zk(p.query_identity,q.log_size));
    w.bytes(encodeDegree1Zk(p.query_sum,q.log_size));w.bytes(encodeDegree1Zk(p.table_sum,t.log_size));
    w.bytes(encode(p.public_table_factor));for(const auto& o:p.openings)w.bytes(encode(o));w.fr(p.binding);
    auto bytes=w.finish();if(bytes.size()!=proofSize(s))throw std::invalid_argument("LogUp proof encoded shape");return bytes;
}
FieldLogUpZkProof decodeFieldLogUpZkProof(const FieldLogUpZkStatement& s,const Bytes& bytes){
    validateFieldLogUpZkStatement(s);const auto q=matrixShape(s.query.size),t=matrixShape(s.table.size());
    Reader r(bytes,proofSize(s));
    if(r.u64()!=proofMagic || r.u64()!=q.size || r.u64()!=t.size)throw std::invalid_argument("LogUp proof version/dimensions");
    FieldLogUpZkProof p;p.Cm=matrixRead(r,t.size);p.Ca=matrixRead(r,q.size);p.Cb=matrixRead(r,t.size);
    p.D_M=r.point();p.D_S=r.point();p.table_identity=decodeDegree3Zk(r.bytes(d3(t.log_size)),t.log_size);
    p.query_identity=decodeDegree3Zk(r.bytes(d3(q.log_size)),q.log_size);
    p.query_sum=decodeDegree1Zk(r.bytes(d1(q.log_size)),q.log_size);
    p.table_sum=decodeDegree1Zk(r.bytes(d1(t.log_size)),t.log_size);
    p.public_table_factor=decodeEquality(r.bytes(64));
    for(size_t i=0;i<6;++i){auto c=(i==0||i==1||i==5)?t.columns:q.columns;p.openings[i]=decodeLinear(r.bytes(lin(c)),c);}
    p.binding=r.fr();r.finish();return p;
}
}
