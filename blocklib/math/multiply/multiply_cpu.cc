/* -*- c++ -*- */
/*
 * Copyright 2004,2009,2010,2012,2018 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "multiply_cpu.hh"
#include "multiply_cpu_gen.hh"
#include <volk/volk.h>

namespace gr {
namespace math {

template <class T>
multiply_cpu<T>::multiply_cpu(const typename multiply<T>::block_args& args)
    : sync_block("multiply"), multiply<T>(args), d_num_inputs(args.num_inputs), d_vlen(args.vlen)
{
}

template <>
multiply_cpu<float>::multiply_cpu(const typename multiply<float>::block_args& args)
    : sync_block("multiply_ff"), multiply<float>(args), d_num_inputs(args.num_inputs), d_vlen(args.vlen)
{
    // const int alignment_multiple = volk_get_alignment() / sizeof(float);
    // set_output_multiple(std::max(1, alignment_multiple));
}

template <>
multiply_cpu<gr_complex>::multiply_cpu(const typename multiply<gr_complex>::block_args& args)
    : sync_block("multiply_cc"), multiply<gr_complex>(args), d_num_inputs(args.num_inputs), d_vlen(args.vlen)
{
    // const int alignment_multiple = volk_get_alignment() / sizeof(gr_complex);
    // set_output_multiple(std::max(1, alignment_multiple));
}

template <>
work_return_code_t
multiply_cpu<float>::work(std::vector<block_work_input_sptr>& work_input,
                                std::vector<block_work_output_sptr>& work_output)
{
    auto out = work_output[0]->items<float>();
    auto noutput_items = work_output[0]->n_items;
    int noi = d_vlen * noutput_items;

    memcpy(out, work_input[0]->items<float>(), noi * sizeof(float));
    for (size_t i = 1; i < d_num_inputs; i++) {
        volk_32f_x2_multiply_32f(out, out, work_input[i]->items<float>(), noi);
    }

    work_output[0]->n_produced = work_output[0]->n_items;
    return work_return_code_t::WORK_OK;
}

template <>
work_return_code_t
multiply_cpu<gr_complex>::work(std::vector<block_work_input_sptr>& work_input,
                                     std::vector<block_work_output_sptr>& work_output)
{
    auto out = work_output[0]->items<gr_complex>();
    auto noutput_items = work_output[0]->n_items;
    int noi = d_vlen * noutput_items;

    memcpy(out, work_input[0]->items<gr_complex>(), noi * sizeof(gr_complex));
    for (size_t i = 1; i < d_num_inputs; i++) {
        volk_32fc_x2_multiply_32fc(out, out, work_input[i]->items<gr_complex>(), noi);
    }

    work_output[0]->n_produced = work_output[0]->n_items;
    return work_return_code_t::WORK_OK;
}

template <class T>
work_return_code_t
multiply_cpu<T>::work(std::vector<block_work_input_sptr>& work_input,
                            std::vector<block_work_output_sptr>& work_output)
{
    auto optr = work_output[0]->items<T>();
    auto noutput_items = work_output[0]->n_items;

    for (size_t i = 0; i < noutput_items * d_vlen; i++) {
        T acc = (work_input[0]->items<T>())[i];
        for (size_t j = 1; j < d_num_inputs; j++) {
            acc *= (work_input[j]->items<T>())[i];
        }
        *optr++ = static_cast<T>(acc);
    }

    work_output[0]->n_produced = work_output[0]->n_items;
    work_input[0]->n_consumed = work_input[0]->n_items;
    return work_return_code_t::WORK_OK;
}

} /* namespace math */
} /* namespace gr */
