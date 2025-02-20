#pragma once

#include <gnuradio/blocks/null_sink.hh>

namespace gr {
namespace blocks {

class null_sink_cpu : public null_sink
{
public:
    null_sink_cpu(block_args args) : sync_block("null_sink"), null_sink(args), d_itemsize(args.itemsize) {}
    virtual work_return_code_t work(std::vector<block_work_input_sptr>& work_input,
                                    std::vector<block_work_output_sptr>& work_output) override;

protected:
    size_t d_itemsize;
    size_t d_nports;
};

} // namespace blocks
} // namespace gr