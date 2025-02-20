/* -*- c++ -*- */
/*
 * Copyright 2004,2006,2007,2009,2013 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/fileio/file_sink_base.hh>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstdio>
#include <stdexcept>
#include <unistd.h>

// win32 (mingw/msvc) specific
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef O_BINARY
#define OUR_O_BINARY O_BINARY
#else
#define OUR_O_BINARY 0
#endif

// should be handled via configure
#ifdef O_LARGEFILE
#define OUR_O_LARGEFILE O_LARGEFILE
#else
#define OUR_O_LARGEFILE 0
#endif

namespace gr {
namespace fileio {

file_sink_base::file_sink_base(const char* filename, bool is_binary, bool append)
    : d_fp(0), d_new_fp(0), d_updated(false), d_is_binary(is_binary), d_append(append)
{
    if (!open(filename))
        throw std::runtime_error("can't open file");

    d_logger = logging::get_logger("file_sink_base", "default");
    d_debug_logger = logging::get_logger("file_sink_base_dbg", "debug");
}

file_sink_base::~file_sink_base()
{
    close();
    if (d_fp) {
        fclose(d_fp);
        d_fp = 0;
    }
}

bool file_sink_base::open(const char* filename)
{
    std::scoped_lock guard(d_mutex); // hold mutex for duration of this function

    // we use the open system call to get access to the O_LARGEFILE flag.
    int fd;
    int flags;

    if (d_append) {
        flags = O_WRONLY | O_CREAT | O_APPEND | OUR_O_LARGEFILE | OUR_O_BINARY;
    } else {
        flags = O_WRONLY | O_CREAT | O_TRUNC | OUR_O_LARGEFILE | OUR_O_BINARY;
    }
    if ((fd = ::open(filename, flags, 0664)) < 0) {
        GR_LOG_ERROR(d_logger, "{}: {}", filename, strerror(errno));
        return false;
    }
    if (d_new_fp) { // if we've already got a new one open, close it
        fclose(d_new_fp);
        d_new_fp = 0;
    }

    if ((d_new_fp = fdopen(fd, d_is_binary ? "wb" : "w")) == NULL) {
        GR_LOG_ERROR(d_logger, "{}: {}", filename, strerror(errno));
        ::close(fd); // don't leak file descriptor if fdopen fails.
    }

    d_updated = true;
    return d_new_fp != 0;
}

void file_sink_base::close()
{
    std::scoped_lock guard(d_mutex); // hold mutex for duration of this function

    if (d_new_fp) {
        fclose(d_new_fp);
        d_new_fp = 0;
    }
    d_updated = true;
}

void file_sink_base::do_update()
{
    if (d_updated) {
        std::scoped_lock guard(d_mutex); // hold mutex for duration of this block
        if (d_fp)
            fclose(d_fp);
        d_fp = d_new_fp; // install new file pointer
        d_new_fp = 0;
        d_updated = false;
    }
}

void file_sink_base::set_unbuffered(bool unbuffered) { d_unbuffered = unbuffered; }

} /* namespace blocks */
} /* namespace gr */
