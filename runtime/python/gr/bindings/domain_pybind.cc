/*
 * Copyright 2020 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/domain.hh>
// pydoc.h is automatically generated in the build directory
// #include <edge_pydoc.h>

void bind_domain(py::module& m)
{
    using domain_conf = ::gr::domain_conf;

    py::class_<domain_conf, std::shared_ptr<domain_conf>>(
        m, "domain_conf")
        .def(py::init<std::shared_ptr<::gr::scheduler> ,
                std::vector<std::shared_ptr<::gr::node>> >())
        ;
}
