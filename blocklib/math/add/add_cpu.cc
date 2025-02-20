/* -*- c++ -*- */
/*
 * Copyright 2004,2009,2010,2012,2018 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "add_cpu.hh"
#include "add_cpu_gen.hh"
#include <volk/volk.h>

namespace gr {
namespace math {

namespace {
template <typename T>
inline void volk_add(T* out, const T* add, unsigned int num)
{
    for (unsigned int elem = 0; elem < num; elem++) {
        out[elem] += add[elem];
    }
}

template <>
inline void volk_add<float>(float* out, const float* add, unsigned int num)
{
    volk_32f_x2_add_32f(out, out, add, num);
}

template <>
inline void volk_add<gr_complex>(gr_complex* out, const gr_complex* add, unsigned int num)
{
    volk_32fc_x2_add_32fc(out, out, add, num);
}
} // namespace

template <class T>
add_cpu<T>::add_cpu(const typename add<T>::block_args& args)
    : sync_block("add"), add<T>(args), d_vlen(args.vlen), d_nports(args.nports)
{
    // set_alignment(std::max(1, int(volk_get_alignment() / sizeof(T))));
    // this->set_output_multiple(std::max(1, int(volk_get_alignment() / sizeof(T))));
}



template <class T>
work_return_code_t
add_cpu<T>::work(std::vector<block_work_input_sptr>& work_input,
                            std::vector<block_work_output_sptr>& work_output)
{
    auto out = work_output[0]->items<T>();
    auto noutput_items = work_output[0]->n_items;
    int noi = d_vlen * noutput_items;

    memcpy(out, work_input[0]->items<T>(), noi * sizeof(T));
    for (size_t i = 1; i < work_input.size(); i++) {
        volk_add(out, work_input[i]->items<T>(), noi);
    }

    this->produce_each(noutput_items, work_output);
    this->consume_each(noutput_items, work_input);
    return work_return_code_t::WORK_OK;

}

} /* namespace math */
} /* namespace gr */
