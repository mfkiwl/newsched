/* -*- c++ -*- */
/*
 * Copyright 2010,2013 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "annotator_cpu.hh"
#include "annotator_cpu_gen.hh"
#include <pmtf/string.hpp>
#include <pmtf/scalar.hpp>
#include <string.h>
#include <iomanip>
#include <iostream>

namespace gr {
namespace blocks {

annotator_cpu::annotator_cpu(const block_args& args)
    : sync_block("annotator"),
      annotator(args),
      d_when(args.when),
      d_num_inputs(args.num_inputs),
      d_num_outputs(args.num_outputs),
      d_tpp(args.tpp)
{

    set_tag_propagation_policy(args.tpp);

    d_tag_counter = 0;
    // set_relative_rate(1, 1);
}

work_return_code_t annotator_cpu::work(std::vector<block_work_input_sptr>& work_input,
                                   std::vector<block_work_output_sptr>& work_output)
{
    auto noutput_items = work_output[0]->n_items;

    uint64_t abs_N = 0;

    for (unsigned i = 0; i < d_num_inputs; i++) {
        abs_N = work_input[i]->nitems_read();

        auto tags = work_input[i]->buffer->tags_in_window(0,noutput_items);
        d_stored_tags.insert(
            d_stored_tags.end(), tags.begin(), tags.end());
    }

    // Storing the current noutput_items as the value to the "noutput_items" key
    auto srcid = pmtf::string(alias());
    auto key = pmtf::string("seq");

    // Work does nothing to the data stream; just copy all inputs to outputs
    // Adds a new tag when the number of items read is a multiple of d_when
    abs_N = work_output[0]->buffer->total_written();

    for (int j = 0; j < noutput_items; j++) {
        // the min() is a hack to make sure this doesn't segfault if
        // there are a different number of ins and outs. This is
        // specifically designed to test the 1-to-1 propagation policy.
        //for (unsigned i = 0; i < std::min(d_num_outputs, d_num_inputs); i++) {
        for (unsigned i = 0; i < d_num_outputs; i++) {
            if (abs_N % d_when == 0) {
                auto value = pmtf::scalar<uint64_t>(d_tag_counter++);
                work_output[i]->buffer->add_tag(abs_N, key, value, srcid);
            }

            // We don't really care about the data here
            
            // in = (const float*)work_input[i].items();
            // out = (float*)work_output[i].items();
            // out[j] = in[j];
        }
        abs_N++;
    }
    for (unsigned i = 0; i < d_num_outputs; i++) {
        work_output[i]->n_produced = noutput_items;
    }

    return work_return_code_t::WORK_OK;
}

} /* namespace blocks */
} /* namespace gr */
