#include "tests/gkr_test_fixture.hpp"
#include <iostream>
#include <set>
#include <tuple>
int main() {
    try {
        initPairing(mcl::BN254);
        gkr_test::SmallFconnFixture f(true);
        std::vector<GKRInputTerminalClaim> claims;
        std::string error;
        if (!verifyGKRWithClaims(f.circuit(), f.statement(), f.proof(), claims, &error))
            throw std::runtime_error(error);
        std::set<std::tuple<unsigned,unsigned,unsigned>> seen;
        unsigned matrix_claims=0;
        for (const auto &q:claims) {
            if (!seen.emplace(q.layer_index,q.port,q.matrix_point).second)
                throw std::runtime_error("duplicate terminal");
            const auto &l=f.circuit().circuit.at(q.layer_index);
            const auto &map=q.port ? l.ori_id_v : l.ori_id_u;
            const auto count=q.port ? l.size_v[0] : l.size_u[0];
            if (q.actual_size!=count || q.padded_size!=(std::size_t(1)<<q.point.size()))
                throw std::runtime_error("terminal metadata");
            std::vector<Fr> values(q.padded_size,Fr(0));
            for (std::size_t i=0;i<count;++i) values[i]=f.proverInstance().val[0].at(map.at(i));
            for (const auto &r:q.point) {
                for (std::size_t i=0;i<values.size()/2;++i)
                    values[i]=values[2*i]+r*(values[2*i+1]-values[2*i]);
                values.resize(values.size()/2);
            }
            if (values.at(0)!=q.value) throw std::runtime_error("wrong terminal value/coordinates");
            if (l.ty==layerType::FCONN) ++matrix_claims;
        }
        if (matrix_claims!=16) throw std::runtime_error("both points of all four FCONN layers required");
        std::size_t expected=0;
        for (std::size_t i=1;i<f.circuit().size;++i) {
            const auto &l=f.circuit().circuit[i];
            expected+=(l.bit_length_u[0]>=0)+(l.bit_length_v[0]>=0);
            if (l.ty==layerType::FCONN) expected+=2;
        }
        if (expected!=20) throw std::runtime_error("normal gates plus FCONN endpoints required");
        if (claims.size()!=expected) throw std::runtime_error("incomplete terminal inventory");
        auto bad=f.proof(); bad.lasso.input_evaluation+=Fr(1);
        if (verifyGKRWithClaims(f.circuit(),f.statement(),bad,claims,&error) || !claims.empty())
            throw std::runtime_error("failed final opening exported trusted claims");
        bad=f.proof(); bad.layers.at(f.matrixLayerProofIndex()).matrix_v_final_claim_u0+=Fr(1);
        if (verifyGKRWithClaims(f.circuit(),f.statement(),bad,claims,&error) || !claims.empty())
            throw std::runtime_error("failed second matrix point exported trusted claims");
        std::cout<<"PASS authenticated terminals="<<expected<<" FCONN=16; failures clear output\n";
        return 0;
    } catch (const std::exception &e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
