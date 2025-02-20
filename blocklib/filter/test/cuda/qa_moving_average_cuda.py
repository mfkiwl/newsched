#!/usr/bin/env python3
#
# Copyright 2013,2017 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#


from newsched import gr, gr_unittest, blocks, filter
import numpy as np

import math, random

def make_random_complex_tuple(L, scale=1):
    result = []
    for x in range(L):
        result.append(scale*complex(2*random.random()-1,
                                    2*random.random()-1))
    return tuple(result)

def make_random_float_tuple(L, scale=1):
    result = []
    for x in range(L):
        result.append(scale*(2*random.random()-1))
    return tuple(result)

class test_moving_average(gr_unittest.TestCase):

    def setUp(self):
        random.seed(0)
        self.tb = gr.flowgraph()

    def tearDown(self):
        self.tb = None

    # These tests will always pass and are therefore useless. 100 random numbers [-1,1) are
    # getting summed up and scaled with 0.001. Then, an assertion verifies a result near 0,
    # which is the case even if the block is malfunctioning.

    def test_01(self):
        tb = self.tb

        N = 10000
        data = make_random_float_tuple(N, 1)
        expected_result = N*[0,]

        src = blocks.vector_source_f(data, False)
        op  = filter.moving_average_ff(100, 0.001, impl=filter.moving_average_ff.cuda)
        dst = blocks.vector_sink_f()

        tb.connect(src, op).set_custom_buffer(gr.buffer_cuda_properties.make(gr.buffer_cuda_type.H2D))
        tb.connect(op, dst).set_custom_buffer(gr.buffer_cuda_properties.make(gr.buffer_cuda_type.D2H))
        tb.run()

        dst_data = dst.data()

        # make sure result is close to zero
        self.assertFloatTuplesAlmostEqual(expected_result, dst_data, 1)

    def test_02(self):
        tb = self.tb

        N = 10000
        data = make_random_complex_tuple(N, 1)
        expected_result = N*[0,]

        src = blocks.vector_source_c(data, False)
        op  = filter.moving_average_cc(100, 0.001, impl=filter.moving_average_cc.cuda)
        dst = blocks.vector_sink_c()

        tb.connect(src, op).set_custom_buffer(gr.buffer_cuda_properties.make(gr.buffer_cuda_type.H2D))
        tb.connect(op, dst).set_custom_buffer(gr.buffer_cuda_properties.make(gr.buffer_cuda_type.D2H))
        tb.run()

        dst_data = dst.data()

        # make sure result is close to zero
        self.assertComplexTuplesAlmostEqual(expected_result, dst_data, 1)

    def test_moving_sum(self):
        tb = self.tb

        N = 10000
        filt_len = 14
        data = [1.0] * N
        expected_result = N*[filt_len,]
        expected_result[0:filt_len-1] = list(range(1, filt_len))

        src = blocks.vector_source_f(data, False)
        op  = filter.moving_average_ff(filt_len, 1.0, impl=filter.moving_average_ff.cuda)
        dst = blocks.vector_sink_f()

        tb.connect(src, op).set_custom_buffer(gr.buffer_cuda_properties.make(gr.buffer_cuda_type.H2D))
        tb.connect(op, dst).set_custom_buffer(gr.buffer_cuda_properties.make(gr.buffer_cuda_type.D2H))
        tb.run()

        dst_data = dst.data()

        self.assertFloatTuplesAlmostEqual(expected_result, dst_data, 4)

    def test_moving_sum2(self):
        tb = self.tb

        
        filt_len = 3
        data = list(range(1,11))
        expected_result = []
        for ii in range(10):
            sum = 0.0
            for jj in range(filt_len):
                if (ii - jj ) >= 0:
                    sum += data[ii-jj]
            expected_result.append(sum)

        src = blocks.vector_source_f(data, False)
        op  = filter.moving_average_ff(filt_len, 1.0, 4, impl=filter.moving_average_ff.cuda)
        dst = blocks.vector_sink_f()

        tb.connect(src, op).set_custom_buffer(gr.buffer_cuda_properties.make(gr.buffer_cuda_type.H2D))
        tb.connect(op, dst).set_custom_buffer(gr.buffer_cuda_properties.make(gr.buffer_cuda_type.D2H))
        tb.run()

        dst_data = dst.data()

        self.assertFloatTuplesAlmostEqual(expected_result, dst_data, 4)

    def test_moving_avg3(self):
        tb = self.tb

        N = 100000
        filt_len = 64
        data = list(make_random_float_tuple(N, scale=1))
        expected_result = []
        for ii in range(N):
            sum = 0.0
            for jj in range(filt_len):
                if (ii - jj ) >= 0:
                    sum += data[ii-jj]
            expected_result.append(float(sum / N))

        src = blocks.vector_source_f(data, False)
        op  = filter.moving_average_ff(filt_len, 1.0 / N, 4000, impl=filter.moving_average_ff.cuda)
        dst = blocks.vector_sink_f()

        tb.connect(src, op).set_custom_buffer(gr.buffer_cuda_properties.make(gr.buffer_cuda_type.H2D))
        tb.connect(op, dst).set_custom_buffer(gr.buffer_cuda_properties.make(gr.buffer_cuda_type.D2H))
        tb.run()

        dst_data = dst.data()

        self.assertFloatTuplesAlmostEqual(expected_result, dst_data, 7)

    # This tests implement own moving average to verify correct behaviour of the block

    # def test_03(self):
    #     tb = self.tb

    #     vlen = 5
    #     N = 10*vlen
    #     data = make_random_float_tuple(N, 2**10)
    #     data = [int(d*1000) for d in data]
    #     src = blocks.vector_source_i(data, False)
    #     one_to_many = blocks.stream_to_streams(gr.sizeof_int, vlen)
    #     one_to_vector = blocks.stream_to_vector(gr.sizeof_int, vlen)
    #     many_to_vector = blocks.streams_to_vector(gr.sizeof_int, vlen)
    #     isolated  = [ filter.moving_average_ii(100, 1) for i in range(vlen)]
    #     dut = filter.moving_average_ii(100, 1, vlen=vlen)
    #     dut_dst = blocks.vector_sink_i(vlen=vlen)
    #     ref_dst = blocks.vector_sink_i(vlen=vlen)

    #     tb.connect(src, one_to_many)
    #     tb.connect(src, one_to_vector) #, dut, dut_dst)
    #     tb.connect(one_to_vector, dut)
    #     tb.connect(dut, dut_dst)
    #     tb.connect(many_to_vector, ref_dst)
    #     for idx, single in enumerate(isolated):
    #         tb.connect((one_to_many,idx), single, (many_to_vector,idx))

    #     tb.run()

    #     dut_data = dut_dst.data()
    #     ref_data = ref_dst.data()

    #     # make sure result is close to zero
    #     self.assertTupleEqual(dut_data, ref_data)

    # def test_04(self):
    #     tb = self.tb

    #     N = 10000  # number of samples
    #     history = 100  # num of samples to average
    #     data = make_random_complex_tuple(N, 1)  # generate random data

    #     #  pythonic MA filter
    #     data_padded = (history-1)*[0.0+1j*0.0]+list(data)  # history  
    #     expected_result = []
    #     moving_sum = sum(data_padded[:history-1])
    #     for i in range(N):
    #         moving_sum += data_padded[i+history-1]
    #         expected_result.append(moving_sum)
    #         moving_sum -= data_padded[i]

    #     src = blocks.vector_source_c(data, False)
    #     op  = filter.moving_average_cc(history, 1)
    #     dst = blocks.vector_sink_c()
        
    #     tb.connect(src, op)
    #     tb.connect(op, dst)
    #     tb.run()
    
    #     dst_data = dst.data()

    #     # make sure result is close to zero
    #     self.assertComplexTuplesAlmostEqual(expected_result, dst_data, 4)

if __name__ == '__main__':
    gr_unittest.run(test_moving_average)
