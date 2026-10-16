#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>

namespace zkgpt {

// Preserve the original expressions and branch order. Output must be disjoint
// from both inputs; parallel callers own disjoint output index ranges.
template <typename Field>
void lassoFoldEntry(Field *read1, Field *read2, Field *write1, Field *write2,
                    std::size_t index, const Field &challenge) {
    Field &c=read1[2*index+1], &d=read1[2*index];
    if (c==d) write1[index]=d;
    else write1[index]=(c-d)*challenge+d;
    Field &a=read2[2*index+1], &b=read2[2*index];
    if (a==b) write2[index]=b;
    else write2[index]=(a-b)*challenge+b;
}

// Two generations, each containing coefficient and witness folds. Borrowed
// initial sources are never owned here. Call begin/finish only between worker
// pools, after every reader/writer has joined. No arithmetic is performed here.
template <typename Field>
class LassoFoldingWorkspace {
public:
    explicit LassoFoldingWorkspace(int bits) : bits_(bits) {
        if (bits < 0 || bits > 30)
            throw std::invalid_argument("invalid Lasso workspace bit length");
    }
    void beginRound(int round) {
        if (round != completed_+1 || round > bits_ || output1_)
            throw std::logic_error("invalid Lasso workspace round transition");
        output_length_=std::size_t{1} << (bits_-round);
        output1_.reset(new Field[output_length_]);
        output2_.reset(new Field[output_length_]);
    }
    void zeroOutputSuffix(std::size_t written) {
        if (!output1_ || written > output_length_)
            throw std::out_of_range("invalid Lasso written range");
        for (std::size_t i=written; i<output_length_; ++i) {
            output1_[i].clear();
            output2_[i].clear();
        }
    }
    void finishRound() {
        if (!output1_ || !output2_)
            throw std::logic_error("Lasso output is unavailable");
        input1_=std::move(output1_);
        input2_=std::move(output2_);
        input_length_=output_length_;
        output_length_=0;
        ++completed_;
    }
    Field *input1() const { return input1_.get(); }
    Field *input2() const { return input2_.get(); }
    Field *output1() const { return output1_.get(); }
    Field *output2() const { return output2_.get(); }
    std::size_t inputLength() const {
        return input_length_ ? input_length_ : std::size_t{1} << bits_;
    }
    std::size_t outputLength() const { return output_length_; }
    std::size_t capacity() const { return 2*(input_length_+output_length_); }
    void release() {
        input1_.reset(); input2_.reset();
        output1_.reset(); output2_.reset();
        input_length_=output_length_=0;
    }
private:
    int bits_, completed_=0;
    std::size_t input_length_=0, output_length_=0;
    std::unique_ptr<Field[]> input1_, input2_, output1_, output2_;
};

} // namespace zkgpt
