#
# Copyright 2011-2012, 2018, 2020 Free Software Foundation, Inc.
# Copyright 2021 Josh Morman
#
# This file is part of GNU Radio
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
#



import numpy
import ctypes

from . import runtime_python as gr

########################################################################
# Magic to turn pointers into numpy arrays
# http://docs.scipy.org/doc/numpy/reference/arrays.interface.html
########################################################################

def pointer_to_ndarray(addr, typestr, dims, nitems):
    shape = dims
    if len(shape) > 0 and shape[-1] == 1:
        shape = shape[:-1]
    shape = (nitems,) + tuple(shape)
    class array_like(object):
        __array_interface__ = {
            'data': (int(addr), False),
            'typestr': typestr,
            'descr': None,
            'shape': shape,
            'strides': None,
            'version': 3
        }
    return numpy.asarray(array_like()).view()


def get_input_array(self, work_input, index):
    ctypes.pythonapi.PyCapsule_GetPointer.restype = ctypes.c_void_p
    ctypes.pythonapi.PyCapsule_GetPointer.argtypes = [
        ctypes.py_object, ctypes.c_char_p]

    port = self.get_port(index, gr.STREAM, gr.INPUT)
    return pointer_to_ndarray(
            ctypes.pythonapi.PyCapsule_GetPointer(work_input[index].raw_items(), None),
            port.format_descriptor(),
            port.dims(),
            work_input[index].n_items)

def get_output_array(self, work_output, index):
    ctypes.pythonapi.PyCapsule_GetPointer.restype = ctypes.c_void_p
    ctypes.pythonapi.PyCapsule_GetPointer.argtypes = [
        ctypes.py_object, ctypes.c_char_p]

    port = self.get_port(index, gr.STREAM, gr.OUTPUT)

    return pointer_to_ndarray(
            ctypes.pythonapi.PyCapsule_GetPointer(work_output[index].raw_items(), None),
            port.format_descriptor(),
            port.dims(),
            work_output[index].n_items)