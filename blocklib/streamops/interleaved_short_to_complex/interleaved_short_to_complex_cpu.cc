/* -*- c++ -*- */
/*
 * Copyright 2004,2009,2010,2012,2018 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "interleaved_short_to_complex_cpu.hh"
#include "interleaved_short_to_complex_cpu_gen.hh"
#include <volk/volk.h>

namespace gr {
namespace streamops {

interleaved_short_to_complex_cpu::interleaved_short_to_complex_cpu(const block_args& args)
    : sync_block("interleaved_short_to_complex"), interleaved_short_to_complex(args), d_scalar(args.scale_factor), d_swap(args.swap)
{
}

void interleaved_short_to_complex_cpu::set_swap(bool swap) { d_swap = swap; }
void interleaved_short_to_complex_cpu::set_scale_factor(float new_value)
{
    d_scalar = new_value;
}

work_return_code_t
interleaved_short_to_complex_cpu::work(std::vector<block_work_input_sptr>& work_input,
                                       std::vector<block_work_output_sptr>& work_output)
{
    auto in = work_input[0]->items<short>();
    auto out = work_output[0]->items<float>();

    auto noutput_items = work_output[0]->n_items;

    // This calculates in[] * 1.0 / d_scalar
    volk_16i_s32f_convert_32f(out, in, d_scalar, 2 * noutput_items);

    if (d_swap) {
        for (int i = 0; i < noutput_items; ++i) {
            float f = out[2 * i + 1];
            out[2 * i + 1] = out[2 * i];
            out[2 * i] = f;
        }
    }

    work_output[0]->n_produced = noutput_items;
    return work_return_code_t::WORK_OK;
}


} /* namespace streamops */
} /* namespace gr */
