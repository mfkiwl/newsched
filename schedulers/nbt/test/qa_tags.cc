#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <thread>

#include <gnuradio/blocks/annotator.hh>
#include <gnuradio/blocks/head.hh>
#include <gnuradio/blocks/null_sink.hh>
#include <gnuradio/blocks/null_source.hh>
#include <gnuradio/blocks/vector_sink.hh>
#include <gnuradio/blocks/vector_source.hh>
#include <gnuradio/flowgraph.hh>
#include <gnuradio/schedulers/nbt/scheduler_nbt.hh>
#include <gnuradio/buffer_cpu_vmcirc.hh>

using namespace gr;

TEST(SchedulerMTTags, OneToOne)
{
    size_t N = 40000;
    auto fg = flowgraph::make();
    auto src = gr::blocks::null_source::make({});
    auto head = gr::blocks::head::make_cpu({N});
    auto ann0 = gr::blocks::annotator::make_cpu(
        {10000, 1, 2, tag_propagation_policy_t::TPP_ONE_TO_ONE});
    auto ann1 = gr::blocks::annotator::make_cpu(
        {10000, 1, 1, tag_propagation_policy_t::TPP_ONE_TO_ONE});
    auto ann2 = gr::blocks::annotator::make_cpu(
        {10000, 1, 1, tag_propagation_policy_t::TPP_ONE_TO_ONE});
    auto snk0 = gr::blocks::null_sink::make({});
    auto snk1 = gr::blocks::null_sink::make({});

    // using 1-to-1 tag propagation without having equal number of
    // ins and outs. Make sure this works; will just exit run early.
    fg->connect(src, 0, head, 0);
    fg->connect(head, 0, ann0, 0);
    fg->connect(ann0, 0, ann1, 0);
    fg->connect(ann0, 1, ann2, 0);
    fg->connect(ann1, 0, snk0, 0);
    fg->connect(ann2, 0, snk1, 0);

    std::cerr << std::endl
              << "NOTE: This is supposed to produce an error from block_executor"
              << std::endl;

    auto sched = schedulers::scheduler_nbt::make();
    fg->set_scheduler(sched);

    fg->validate();

    fg->start();
    fg->wait();

    std::vector<gr::tag_t> tags0 = ann0->data();
    std::vector<gr::tag_t> tags1 = ann1->data();
    std::vector<gr::tag_t> tags2 = ann2->data();

    // The first annotator does not receive any tags from the null sink upstream
    EXPECT_EQ(tags0.size(), 0);
    EXPECT_EQ(tags1.size(), 4);
    EXPECT_EQ(tags2.size(), 4);
}

TEST(SchedulerMTTags, t1)
{
    size_t N = 40000;
    auto fg = flowgraph::make();
    auto src = gr::blocks::null_source::make({});
    auto head = gr::blocks::head::make_cpu({N});
    auto ann0 = gr::blocks::annotator::make_cpu(
        {10000, 1, 2, tag_propagation_policy_t::TPP_ALL_TO_ALL});
    auto ann1 = gr::blocks::annotator::make_cpu(
        {10000, 1, 1, tag_propagation_policy_t::TPP_ALL_TO_ALL});
    auto ann2 = gr::blocks::annotator::make_cpu(
        {10000, 1, 1, tag_propagation_policy_t::TPP_ALL_TO_ALL});
    auto ann3 = gr::blocks::annotator::make_cpu(
        {10000, 1, 1, tag_propagation_policy_t::TPP_ALL_TO_ALL});
    auto ann4 = gr::blocks::annotator::make_cpu(
        {10000, 1, 1, tag_propagation_policy_t::TPP_ALL_TO_ALL});

    auto snk0 = gr::blocks::null_sink::make({});
    auto snk1 = gr::blocks::null_sink::make({});

    fg->connect(src, 0, head, 0);
    fg->connect(head, 0, ann0, 0);

    fg->connect(ann0, 0, ann1, 0);
    fg->connect(ann0, 1, ann2, 0);
    fg->connect(ann1, 0, ann3, 0);
    fg->connect(ann2, 0, ann4, 0);

    fg->connect(ann3, 0, snk0, 0);
    fg->connect(ann4, 0, snk1, 0);

    auto sched = schedulers::scheduler_nbt::make();
    fg->set_scheduler(sched);
    fg->validate();

    fg->run();

    std::vector<gr::tag_t> tags0 = ann0->data();
    std::vector<gr::tag_t> tags3 = ann3->data();
    std::vector<gr::tag_t> tags4 = ann4->data();

    // The first annotator does not receive any tags from the null sink upstream
    EXPECT_EQ(tags0.size(), (size_t)0);
    EXPECT_EQ(tags3.size(), (size_t)8);
    EXPECT_EQ(tags4.size(), (size_t)8);
}

TEST(SchedulerMTTags,t2)
{
    size_t N = 40000;
    auto fg = flowgraph::make();
    auto src = gr::blocks::null_source::make({});
    auto head = gr::blocks::head::make_cpu({N});
    auto ann0 = gr::blocks::annotator::make_cpu(
        {10000, 1, 2, tag_propagation_policy_t::TPP_ALL_TO_ALL});
    auto ann1 = gr::blocks::annotator::make_cpu(
        {10000, 2, 3, tag_propagation_policy_t::TPP_ALL_TO_ALL});
    auto ann2 = gr::blocks::annotator::make_cpu(
        {10000, 1, 1, tag_propagation_policy_t::TPP_ALL_TO_ALL});
    auto ann3 = gr::blocks::annotator::make_cpu(
        {10000, 1, 1, tag_propagation_policy_t::TPP_ALL_TO_ALL});
    auto ann4 = gr::blocks::annotator::make_cpu(
        {10000, 1, 1, tag_propagation_policy_t::TPP_ALL_TO_ALL});
    auto snk0 = gr::blocks::null_sink::make({});
    auto snk1 = gr::blocks::null_sink::make({});
    auto snk2 = gr::blocks::null_sink::make({});

    fg->connect(src, 0, head, 0);
    fg->connect(head, 0, ann0, 0);

    fg->connect(ann0, 0, ann1, 0);
    fg->connect(ann0, 1, ann1, 1);
    fg->connect(ann1, 0, ann2, 0);
    fg->connect(ann1, 1, ann3, 0);
    fg->connect(ann1, 2, ann4, 0);

    fg->connect(ann2, 0, snk0, 0);
    fg->connect(ann3, 0, snk1, 0);
    fg->connect(ann4, 0, snk2, 0);


    auto sched = schedulers::scheduler_nbt::make();
    fg->set_scheduler(sched);
    fg->validate();

    fg->run();

    std::vector<gr::tag_t> tags0 = ann0->data();
    std::vector<gr::tag_t> tags1 = ann1->data();
    std::vector<gr::tag_t> tags2 = ann2->data();
    std::vector<gr::tag_t> tags3 = ann4->data();
    std::vector<gr::tag_t> tags4 = ann4->data();

    // The first annotator does not receive any tags from the null sink upstream
    EXPECT_EQ(tags0.size(), (size_t)0);
    EXPECT_EQ(tags1.size(), (size_t)8);

    // Make sure the rest all have 12 tags
    EXPECT_EQ(tags2.size(), (size_t)12);
    EXPECT_EQ(tags3.size(), (size_t)12);
    EXPECT_EQ(tags4.size(), (size_t)12);

}

TEST(SchedulerMTTags, t3)
{
    size_t N = 40000;
    auto fg = flowgraph::make();
    auto src = gr::blocks::null_source::make({});
    auto head = gr::blocks::head::make_cpu({N});
    auto ann0 = gr::blocks::annotator::make_cpu(
        {10000, 2, 2, tag_propagation_policy_t::TPP_ONE_TO_ONE});
    auto ann1 = gr::blocks::annotator::make_cpu(
        {10000, 1, 1, tag_propagation_policy_t::TPP_ALL_TO_ALL});
    auto ann2 = gr::blocks::annotator::make_cpu(
        {10000, 1, 1, tag_propagation_policy_t::TPP_ALL_TO_ALL});
    auto ann3 = gr::blocks::annotator::make_cpu(
        {10000, 1, 1, tag_propagation_policy_t::TPP_ONE_TO_ONE});
    auto ann4 = gr::blocks::annotator::make_cpu(
        {10000, 1, 1, tag_propagation_policy_t::TPP_ONE_TO_ONE});
    auto snk0 = gr::blocks::null_sink::make({});
    auto snk1 = gr::blocks::null_sink::make({});

    fg->connect(src, 0, head, 0);
    fg->connect(head, 0, ann0, 0);
    fg->connect(head, 0, ann0, 1);

    fg->connect(ann0, 0, ann1, 0);
    fg->connect(ann0, 1, ann2, 0);
    fg->connect(ann1, 0, ann3, 0);
    fg->connect(ann2, 0, ann4, 0);

    fg->connect(ann3, 0, snk0, 0);
    fg->connect(ann4, 0, snk1, 0);

    auto sched = schedulers::scheduler_nbt::make();
    sched->add_block_group({src,head,ann0,ann1,ann2,ann3,ann4,snk0,snk1});
    fg->set_scheduler(sched);
    

    fg->validate();

    fg->start();
    fg->wait();

    auto tags0 = ann0->data();
    auto tags3 = ann3->data();
    auto tags4 = ann4->data();

    // The first annotator does not receive any tags from the null sink upstream
    EXPECT_EQ(tags0.size(), (size_t)0);
    EXPECT_EQ(tags3.size(), (size_t)8);
    EXPECT_EQ(tags4.size(), (size_t)8);
}


#if 0 // TODO Rate Change Blocks
TEST(SchedulerMTTags, t5)
{
    int N = 40000;

    auto fg = flowgraph::make();
    auto src = gr::blocks::null_source::make({sizeof(int)});
    auto head = gr::blocks::head::make_cpu(sizeof(int), N);
    auto ann0 = gr::blocks::annotator::make_cpu(
        10000, 1, 2, tag_propagation_policy_t::TPP_ALL_TO_ALL);
    auto ann1 = gr::blocks::annotator::make_cpu(
        10000, 1, 1, tag_propagation_policy_t::TPP_ALL_TO_ALL);
    auto ann2 = gr::blocks::annotator::make_cpu(
        10000, 1, 1, tag_propagation_policy_t::TPP_ALL_TO_ALL);
    auto snk0 = gr::blocks::null_sink::make(sizeof(int));

    // Rate change blocks
    gr::blocks::keep_one_in_n::sptr dec10(
        gr::blocks::keep_one_in_n::make(sizeof(float), 10));

    tb->connect(src, 0, head, 0);
    tb->connect(head, 0, ann0, 0);
    tb->connect(ann0, 0, ann1, 0);
    tb->connect(ann1, 0, dec10, 0);
    tb->connect(dec10, 0, ann2, 0);
    tb->connect(ann2, 0, snk0, 0);

    tb->run();

    std::vector<gr::tag_t> tags0 = ann0->data();
    std::vector<gr::tag_t> tags1 = ann1->data();
    std::vector<gr::tag_t> tags2 = ann2->data();

    // The first annotator does not receive any tags from the null sink upstream
    BOOST_REQUIRE_EQUAL(tags0.size(), (size_t)0);
    BOOST_REQUIRE_EQUAL(tags1.size(), (size_t)4);
    BOOST_REQUIRE_EQUAL(tags2.size(), (size_t)8);
}
#endif