/* -*- c++ -*- */
/*
 * Copyright 2004,2009,2010,2012,2018 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "source_cpu.hh"
#include "source_cpu_gen.hh"
#include <SoapySDR/Formats.h>
#include <SoapySDR/Errors.hpp>
#include <volk/volk.h>

namespace gr {
namespace soapy {

template <>
source_cpu<gr_complex>::source_cpu(const typename source<gr_complex>::block_args& args)
    : gr::block("soapy_source"),
      source<gr_complex>(args),
      block_impl(SOAPY_SDR_RX,
                 args.device,
                 SOAPY_SDR_CF32,
                 args.nchan,
                 args.dev_args,
                 args.stream_args,
                 args.tune_args,
                 args.other_settings)
{
}

template <class T>
work_return_code_t source_cpu<T>::work(std::vector<block_work_input_sptr>& work_input,
                                       std::vector<block_work_output_sptr>& work_output)
{
    auto noutput_items = work_output[0]->n_items;
    /* This limits each work invocation to MTU transfers */
    if (d_mtu > 0) {
        noutput_items = std::min(noutput_items, (int)d_mtu);
    } else {
        noutput_items = std::min(noutput_items, 1024);
    }

    long long int time_ns = 0;
    int flags = 0;
    const long timeout_us = 500000; // 0.5 sec
    int nout = 0;

    auto output_items = block_work_output::all_items(work_output);
    for (;;) {

        // No command handlers while reading
        int result;
        {
            std::lock_guard<std::mutex> l(d_device_mutex);
            result =
                d_device->readStream(d_stream,
                                     output_items.data(),
                                     noutput_items,
                                     flags,
                                     time_ns,
                                     timeout_us);
            
        }

        if (result >= 0) {
            nout = result;
            break;
        }

        switch (result) {

        // Retry on overflow. Data has been lost.
        case SOAPY_SDR_OVERFLOW:
            std::cerr << "sO" << std::flush;
            continue;

        // Yield back to scheduler on timeout.
        case SOAPY_SDR_TIMEOUT:
            break;

        // Report and yield back to scheduler on other errors.
        default:
            GR_LOG_WARN(logger(), "Soapy source error: {}", SoapySDR::errToStr(result));
            break;
        }

        break;
    };

    produce_each(nout, work_output);
    return work_return_code_t::WORK_OK;
}


} /* namespace soapy */
} /* namespace gr */
