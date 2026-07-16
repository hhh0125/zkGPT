#include "range_sumcheck.hpp"

#include <stdexcept>

namespace {

bool failWith(std::string *error, const std::string &message) {
    if (error) *error=message;
    return false;
}

bool isPowerOfTwo(std::size_t value) {
    return value!=0 && (value&(value-1))==0;
}

std::size_t exactLog2(std::size_t value) {
    if (!isPowerOfTwo(value))
        throw std::invalid_argument("sumcheck vector size is not a power of two");
    std::size_t result=0;
    while (value>1) {
        value>>=1;
        ++result;
    }
    return result;
}

Fr degree3Evaluate(const Degree3SumcheckRound &round, const Fr &point) {
    return Fr(1)/Fr(6)*(
        (-round.value_at_0)*(point-1)*(point-2)*(point-3)+
        Fr(3)*round.value_at_1*point*(point-2)*(point-3)-
        Fr(3)*round.value_at_2*point*(point-1)*(point-3)+
        round.value_at_3*point*(point-1)*(point-2));
}

std::vector<Fr> eqWeights(const std::vector<Fr> &point) {
    std::vector<Fr> weights(static_cast<std::size_t>(1)<<point.size(), Fr(0));
    weights[0]=1;
    std::size_t active=1;
    for (const Fr &challenge : point) {
        for (std::size_t i=active;i>0;--i) {
            const Fr previous=weights[i-1];
            weights[i-1]=previous*(Fr(1)-challenge);
            weights[i-1+active]=previous*challenge;
        }
        active*=2;
    }
    return weights;
}

void appendDegree1Round(Transcript &transcript, const std::string &label,
                        const Degree1SumcheckRound &round) {
    transcript.appendFr(label+".value_at_0", round.value_at_0);
    transcript.appendFr(label+".value_at_1", round.value_at_1);
}

void appendDegree3Round(Transcript &transcript, const std::string &label,
                        const Degree3SumcheckRound &round) {
    transcript.appendFr(label+".value_at_0", round.value_at_0);
    transcript.appendFr(label+".value_at_1", round.value_at_1);
    transcript.appendFr(label+".value_at_2", round.value_at_2);
    transcript.appendFr(label+".value_at_3", round.value_at_3);
}

Fr equalityEvaluation(const std::vector<Fr> &statement_point,
                      const std::vector<Fr> &opening_point) {
    if (statement_point.size()!=opening_point.size())
        throw std::invalid_argument("equality point length mismatch");
    Fr result=1;
    for (std::size_t i=0;i<statement_point.size();++i)
        result*=statement_point[i]*opening_point[i]+
            (Fr(1)-statement_point[i])*(Fr(1)-opening_point[i]);
    return result;
}

}  // namespace

Degree1SumcheckProof proveDegree1Sumcheck(
    const std::vector<Fr> &values_input, const Fr &initial_claim,
    Transcript &transcript, const std::string &label,
    std::vector<Fr> *opening_point) {
    exactLog2(values_input.size());
    Fr actual_claim=0;
    for (const Fr &value : values_input) actual_claim+=value;
    if (actual_claim!=initial_claim)
        throw std::logic_error("degree-1 sumcheck witness does not match claim");

    std::vector<Fr> values=values_input;
    Degree1SumcheckProof proof;
    std::vector<Fr> point;
    Fr claim=initial_claim;
    while (values.size()>1) {
        Degree1SumcheckRound round;
        round.value_at_0=0;
        round.value_at_1=0;
        for (std::size_t i=0;i<values.size();i+=2) {
            round.value_at_0+=values[i];
            round.value_at_1+=values[i+1];
        }
        if (round.value_at_0+round.value_at_1!=claim)
            throw std::logic_error("degree-1 prover round consistency failed");
        proof.rounds.push_back(round);
        appendDegree1Round(transcript, label, round);
        const Fr challenge=transcript.challenge(label+".challenge");
        point.push_back(challenge);
        claim=round.value_at_0+
              challenge*(round.value_at_1-round.value_at_0);
        for (std::size_t i=0;i<values.size()/2;++i)
            values[i]=values[2*i]+
                challenge*(values[2*i+1]-values[2*i]);
        values.resize(values.size()/2);
    }
    proof.final_evaluation=values[0];
    if (proof.final_evaluation!=claim)
        throw std::logic_error("degree-1 prover final claim failed");
    transcript.appendFr(label+".final_evaluation", proof.final_evaluation);
    if (opening_point) *opening_point=std::move(point);
    return proof;
}

bool verifyDegree1Sumcheck(
    const Fr &initial_claim, const Degree1SumcheckProof &proof,
    Transcript &transcript, const std::string &label,
    std::vector<Fr> *opening_point, std::string *error) {
    try {
        Fr claim=initial_claim;
        std::vector<Fr> point;
        point.reserve(proof.rounds.size());
        for (const auto &round : proof.rounds) {
            appendDegree1Round(transcript, label, round);
            if (round.value_at_0+round.value_at_1!=claim)
                return failWith(error,
                                "degree-1 sumcheck round consistency failed");
            const Fr challenge=transcript.challenge(label+".challenge");
            point.push_back(challenge);
            claim=round.value_at_0+
                  challenge*(round.value_at_1-round.value_at_0);
        }
        transcript.appendFr(label+".final_evaluation", proof.final_evaluation);
        if (claim!=proof.final_evaluation)
            return failWith(error, "degree-1 sumcheck final claim failed");
        if (opening_point) *opening_point=std::move(point);
        return true;
    } catch (const std::exception &exception) {
        return failWith(error, exception.what());
    }
}

Degree3SumcheckProof proveDegree3Sumcheck(
    const std::vector<Fr> &equality_point,
    const std::vector<Fr> &f_input,
    const std::vector<Fr> &g_input, const Fr &initial_claim,
    Transcript &transcript, const std::string &label,
    std::vector<Fr> *opening_point) {
    if (f_input.size()!=g_input.size() ||
        equality_point.size()!=exactLog2(f_input.size()))
        throw std::invalid_argument("invalid degree-3 sumcheck shape");
    std::vector<Fr> equality=eqWeights(equality_point);
    std::vector<Fr> f=f_input;
    std::vector<Fr> g=g_input;
    Fr actual_claim=0;
    for (std::size_t i=0;i<f.size();++i)
        actual_claim+=equality[i]*f[i]*g[i];
    if (actual_claim!=initial_claim)
        throw std::logic_error("degree-3 sumcheck witness does not match claim");

    Degree3SumcheckProof proof;
    std::vector<Fr> point;
    Fr claim=initial_claim;
    while (f.size()>1) {
        Degree3SumcheckRound round;
        round.value_at_0=0;
        round.value_at_1=0;
        round.value_at_2=0;
        round.value_at_3=0;
        for (std::size_t i=0;i<f.size();i+=2) {
            const Fr e0=equality[i];
            const Fr e1=equality[i+1];
            const Fr f0=f[i];
            const Fr f1=f[i+1];
            const Fr g0=g[i];
            const Fr g1=g[i+1];
            round.value_at_0+=e0*f0*g0;
            round.value_at_1+=e1*f1*g1;
            round.value_at_2+=(Fr(2)*e1-e0)*(Fr(2)*f1-f0)*
                              (Fr(2)*g1-g0);
            round.value_at_3+=(Fr(3)*e1-Fr(2)*e0)*
                              (Fr(3)*f1-Fr(2)*f0)*
                              (Fr(3)*g1-Fr(2)*g0);
        }
        if (round.value_at_0+round.value_at_1!=claim)
            throw std::logic_error("degree-3 prover round consistency failed");
        proof.rounds.push_back(round);
        appendDegree3Round(transcript, label, round);
        const Fr challenge=transcript.challenge(label+".challenge");
        point.push_back(challenge);
        claim=degree3Evaluate(round, challenge);
        for (std::size_t i=0;i<f.size()/2;++i) {
            equality[i]=equality[2*i]+
                challenge*(equality[2*i+1]-equality[2*i]);
            f[i]=f[2*i]+challenge*(f[2*i+1]-f[2*i]);
            g[i]=g[2*i]+challenge*(g[2*i+1]-g[2*i]);
        }
        equality.resize(equality.size()/2);
        f.resize(f.size()/2);
        g.resize(g.size()/2);
    }
    proof.final_f_evaluation=f[0];
    proof.final_g_evaluation=g[0];
    if (claim!=equality[0]*f[0]*g[0])
        throw std::logic_error("degree-3 prover final claim failed");
    transcript.appendFr(label+".final_f", proof.final_f_evaluation);
    transcript.appendFr(label+".final_g", proof.final_g_evaluation);
    if (opening_point) *opening_point=std::move(point);
    return proof;
}

bool verifyDegree3Sumcheck(
    const std::vector<Fr> &equality_point, const Fr &initial_claim,
    const Degree3SumcheckProof &proof, Transcript &transcript,
    const std::string &label, std::vector<Fr> *opening_point,
    std::string *error) {
    try {
        if (proof.rounds.size()!=equality_point.size())
            return failWith(error, "degree-3 sumcheck round count mismatch");
        Fr claim=initial_claim;
        std::vector<Fr> point;
        point.reserve(proof.rounds.size());
        for (const auto &round : proof.rounds) {
            appendDegree3Round(transcript, label, round);
            if (round.value_at_0+round.value_at_1!=claim)
                return failWith(error,
                                "degree-3 sumcheck round consistency failed");
            const Fr challenge=transcript.challenge(label+".challenge");
            point.push_back(challenge);
            claim=degree3Evaluate(round, challenge);
        }
        transcript.appendFr(label+".final_f", proof.final_f_evaluation);
        transcript.appendFr(label+".final_g", proof.final_g_evaluation);
        const Fr equality=equalityEvaluation(equality_point, point);
        if (claim!=equality*proof.final_f_evaluation*
                      proof.final_g_evaluation)
            return failWith(error, "degree-3 sumcheck final claim failed");
        if (opening_point) *opening_point=std::move(point);
        return true;
    } catch (const std::exception &exception) {
        return failWith(error, exception.what());
    }
}
