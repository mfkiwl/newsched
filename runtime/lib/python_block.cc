/* -*- c++ -*- */
/*
 * Copyright 2013,2020 Free Software Foundation, Inc.
 * Copyright 2021 Josh Morman
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#include <gnuradio/python_block.hh>
#include <pybind11/embed.h>
#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


#include <gnuradio/buffer_cpu_vmcirc.hh>

namespace gr {

/**************************************************************************
 *   Python Block
 **************************************************************************/
python_block::sptr python_block::make(const py::object& p,
                                        const std::string& name)
{
    return std::make_shared<python_block>(p, name);
}

python_block::python_block(const py::handle& p,
                                       const std::string& name)
    : block(name)
{
    d_py_handle = p;
}

work_return_code_t python_block::work(std::vector<block_work_input_sptr>& work_input,
                                    std::vector<block_work_output_sptr>& work_output)
{
    py::gil_scoped_acquire acquire;

    py::object ret = d_py_handle.attr("handle_work")(
        work_input, work_output);

    return ret.cast<work_return_code_t>();
}

bool python_block::start(void)
{
    py::gil_scoped_acquire acquire;

    py::object ret = d_py_handle.attr("start")();
    return ret.cast<bool>();
}

bool python_block::stop(void)
{
    py::gil_scoped_acquire acquire;

    py::object ret = d_py_handle.attr("stop")();
    return ret.cast<bool>();
}

/**************************************************************************
 *   Python Sync Block
 **************************************************************************/
python_sync_block::sptr python_sync_block::make(const py::object& p,
                                        const std::string& name)
{
    return std::make_shared<python_sync_block>(p, name);
}

python_sync_block::python_sync_block(const py::handle& p,
                                       const std::string& name)
    : sync_block(name)
{   
    d_py_handle = p;
}


work_return_code_t python_sync_block::work(std::vector<block_work_input_sptr>& work_input,
                                    std::vector<block_work_output_sptr>& work_output)
{
    py::gil_scoped_acquire acquire;

    py::object ret = d_py_handle.attr("work")(work_input, work_output
        );

    return ret.cast<work_return_code_t>();
}

bool python_sync_block::start(void)
{
    py::gil_scoped_acquire acquire;

    py::object ret = d_py_handle.attr("start")();
    return ret.cast<bool>();
}

bool python_sync_block::stop(void)
{
    py::gil_scoped_acquire acquire;

    py::object ret = d_py_handle.attr("stop")();
    return ret.cast<bool>();
}


} /* namespace gr */
