#include <chrono>
#include <iostream>
#include <thread>

#include <gnuradio/blocks/copy.hh>
#include <gnuradio/blocks/head.hh>
#include <gnuradio/blocks/null_sink.hh>
#include <gnuradio/blocks/null_source.hh>
#include <gnuradio/flowgraph.hh>
#include <gnuradio/realtime.hh>
#include <gnuradio/schedulers/nbt/scheduler_nbt.hh>
#include <gnuradio/buffer_cpu_simple.hh>
#include <gnuradio/buffer_cpu_vmcirc.hh>

#include <iostream>

#include "CLI/App.hpp"
#include "CLI/Formatter.hpp"
#include "CLI/Config.hpp"

using namespace gr;

int main(int argc, char* argv[])
{
    uint64_t samples = 15000000;
    int nblocks = 1;
    int veclen = 1;
    int buffer_type = 0;
    bool rt_prio = false;

    CLI::App app{"App description"};

    app.add_option("--samples", samples, "Number of Samples");
    app.add_option("--veclen", veclen, "Vector Length");
    app.add_option("--nblocks", nblocks, "Number of copy blocks");
    app.add_option("--buffer_type", buffer_type, "Buffer Type (0:simple, 1:vmcirc, 2:cuda, 3:cuda_pinned");
    app.add_flag("--rt_prio", rt_prio, "Enable Real-time priority");

    if (rt_prio && gr::enable_realtime_scheduling() != RT_OK) {
        std::cout << "Error: failed to enable real-time scheduling." << std::endl;
    }

    {
        auto src = blocks::null_source::make({sizeof(gr_complex) * veclen});
        auto head = blocks::head::make_cpu({sizeof(gr_complex) * veclen, samples / veclen});

        std::vector<blocks::null_sink::sptr> sink_blks(nblocks);
        std::vector<blocks::copy::sptr> copy_blks(nblocks);
        for (int i = 0; i < nblocks; i++) {
            copy_blks[i] = blocks::copy::make({sizeof(gr_complex) * veclen});
            sink_blks[i] = blocks::null_sink::make({sizeof(gr_complex) * veclen});
        }
        flowgraph_sptr fg(new flowgraph());

        if (buffer_type == 0) {
            fg->connect(src, 0, head, 0);

            for (int i = 0; i < nblocks; i++) {
                fg->connect(head, 0, copy_blks[i], 0);
                fg->connect(copy_blks[i], 0, sink_blks[i], 0);
            }

        } else {
            fg->connect(src, 0, head, 0)->set_custom_buffer(BUFFER_CPU_VMCIRC_ARGS);

            for (int i = 0; i < nblocks; i++) {
                fg->connect(head, 0, copy_blks[i], 0)->set_custom_buffer(BUFFER_CPU_VMCIRC_ARGS);
                fg->connect(copy_blks[i], 0, sink_blks[i], 0)->set_custom_buffer(BUFFER_CPU_VMCIRC_ARGS);
            }
        }

        auto sched1 = schedulers::scheduler_nbt::make("sched1");
        fg->add_scheduler(sched1);
        fg->validate();

        auto t1 = std::chrono::steady_clock::now();

        fg->start();
        fg->wait();

        auto t2 = std::chrono::steady_clock::now();
        auto time =
            std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count() / 1e9;

        std::cout << "[PROFILE_TIME]" << time << "[PROFILE_TIME]" << std::endl;
    }
}
