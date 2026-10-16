#include "tests/hyrax_row_major_reference.hpp"
#include "hyrax.hpp"
#include "gkr_serialization.hpp"
#include <memory>
#include <limits>
#include <iostream>
#include <type_traits>
#include <utility>
static_assert(std::is_same<decltype(std::declval<HyraxFrWitnessView>()[0]),const Fr &>::value,"view must be read-only");
namespace {
void check(bool b,const char *message) { if (!b) throw std::runtime_error(message); }
std::vector<uint8_t> bytes(const MleOpeningProof &opening) {
    GKRProof proof{}; proof.output_evaluation=0;proof.lasso.input_evaluation=0;
    proof.lasso.mapping_evaluation=0;proof.transcript_binding=0;
    proof.lasso.input_opening=opening;return serializeGKRProof(proof);
}
Fr evaluate(std::vector<Fr> values,const std::vector<Fr> &point) {
    std::size_t active=values.size();
    for (const auto &r:point) {
        for (std::size_t i=0;i<active/2;++i) values[i]=values[2*i]+r*(values[2*i+1]-values[2*i]);
        active/=2;
    }
    return values[0];
}
void run(unsigned n,unsigned pattern) {
    const std::size_t count=std::size_t{1}<<n,rows=std::size_t{1}<<(n/2),columns=count/rows;
    std::vector<G1> generators(columns); const G1 u=gen_gi(generators.data(),columns);
    std::vector<Fr> point(n);for (unsigned i=0;i<n;++i) point[i]=Fr(i*7+3);
    std::vector<Fr> values(count,Fr(0));
    uint64_t seed=0x31415926;
    for (std::size_t i=0;i<count;++i) {
        seed=seed*6364136223846793005ULL+1;
        if (pattern==1) values[i]=Fr(static_cast<int>((seed>>32)%103)-51);
        if (pattern==2 && i%17==0) values[i]=Fr(i+1);
        if (pattern==3 && i<(count>2?count-3:count)) values[i]=Fr(i+2);
    }
    if (pattern==4) { // first/last row and column, including four corners
        for (std::size_t r=0;r<rows;++r) { values[r*columns]=Fr(r+1);values[r*columns+columns-1]=Fr(r+13); }
        for (std::size_t c=0;c<columns;++c) { values[c]=Fr(c+7);values[(rows-1)*columns+c]=Fr(c+19); }
    }
    const auto original=values;const Fr evaluation=evaluate(values,point);
    std::unique_ptr<G1[]> raw(prover_commit(values.data(),generators.data(),n,1));
    std::vector<G1> commitments(raw.get(),raw.get()+rows);
    ChallengeTraceRecorder reference_trace,view_trace,vector_trace;
    Transcript reference_transcript("hyrax-zero-copy-test",&reference_trace,"same");
    const auto reference=hyrax_reference::referenceOpening(values,commitments,point,generators,u,evaluation,reference_transcript,"opening");
    MleOpeningProof proof;
    Transcript view_transcript("hyrax-zero-copy-test",&view_trace,"same");
    {
        std::unique_ptr<Fr[]> owner(new Fr[count+2]);owner[0]=Fr(991);owner[count+1]=Fr(997);
        std::copy(values.begin(),values.end(),owner.get()+1);
        const HyraxFrWitnessView view(owner.get()+1,count);
        proof=hyraxMleOpenProveFrRowMajor(view,commitments,point,generators,u,evaluation,view_transcript,"opening");
        check(owner[0]==Fr(991)&&owner[count+1]==Fr(997),"view guard changed");
        for (std::size_t i=0;i<count;++i) check(view[i]==original[i],"borrowed input changed");
    } // owner gone; proof must have independent values and no witness address
    Transcript vector_transcript("hyrax-zero-copy-test",&vector_trace,"same");
    const auto wrapped=hyraxMleOpenProveFrRowMajor(values,commitments,point,generators,u,evaluation,vector_transcript,"opening");
    check(values==original,"vector input changed");
    check(bytes(reference)==bytes(proof)&&bytes(proof)==bytes(wrapped),"canonical opening bytes differ");
    std::string error;
    check(compareChallengeTraces(reference_trace.entries,view_trace.entries,&error),"reference/view challenges differ");
    check(compareChallengeTraces(reference_trace.entries,vector_trace.entries,&error),"reference/vector challenges differ");
    check(reference_transcript.stateDigest()==view_transcript.stateDigest(),"opening transcript digest changed");
    Transcript verifier("hyrax-zero-copy-test");
    check(hyraxMleOpenVerifyRowMajor(commitments,point,generators,u,evaluation,proof,verifier,"opening",&error),"verification after owner destruction failed");
    check(verifier.stateDigest()==view_transcript.stateDigest(),"verifier transcript mismatch");
    const char *hex="0123456789abcdef";
    std::cout << "OPENING_VECTOR " << n << ':' << pattern << " digest=";
    for (auto byte:view_transcript.stateDigest()) std::cout << hex[byte>>4] << hex[byte&15];
    std::cout << '\n';
    auto rejected=[&](HyraxFrWitnessView view,const std::vector<Fr> &bad_point,const std::vector<G1> &bad_rows,const std::vector<G1> &bad_generators) {
        Transcript t("hyrax-zero-copy-test");const auto before=t.stateDigest();bool threw=false;
        try { (void)hyraxMleOpenProveFrRowMajor(view,bad_rows,bad_point,bad_generators,u,evaluation,t,"opening"); }
        catch (const std::invalid_argument &) { threw=true; }
        check(threw,"invalid geometry accepted");check(before==t.stateDigest(),"invalid geometry modified transcript");
    };
    rejected(HyraxFrWitnessView(values.data(),count-1),point,commitments,generators);
    rejected(HyraxFrWitnessView(values.data(),count+1),point,commitments,generators);
    rejected(HyraxFrWitnessView(nullptr,0),point,commitments,generators);
    rejected(HyraxFrWitnessView(values.data(),std::numeric_limits<std::size_t>::max()),point,commitments,generators);
    rejected(HyraxFrWitnessView(values.data(),count),point,{},generators);
    rejected(HyraxFrWitnessView(values.data(),count),point,commitments,{});
    if (!n) {
        rejected(HyraxFrWitnessView(values.data(),count),std::vector<Fr>(31,Fr(0)),commitments,generators);
        rejected(HyraxFrWitnessView(values.data(),count),std::vector<Fr>(std::numeric_limits<std::size_t>::digits,Fr(0)),commitments,generators);
    }
}
}
int main() {
    try {
        initPairing(mcl::BN254);
        bool threw=false;try { HyraxFrWitnessView invalid(nullptr,1); } catch (const std::invalid_argument &) { threw=true; }
        check(threw,"null nonempty view accepted");
        for (unsigned n=0;n<=13;++n) for (unsigned pattern=0;pattern<5;++pattern) run(n,pattern);
        std::cout << "Hyrax row-major zero-copy: 70 reference/vector/view cases passed; canonical bytes, challenges, input guards, owner destruction, invalid geometry passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
