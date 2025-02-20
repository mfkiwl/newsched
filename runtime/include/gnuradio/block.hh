#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <gnuradio/api.h>
#include <gnuradio/block_work_io.hh>
#include <gnuradio/gpdict.hh>
#include <gnuradio/neighbor_interface.hh>
#include <gnuradio/node.hh>
#include <gnuradio/parameter.hh>

#include <pmtf/map.hpp>
#include <pmtf/string.hpp>
#include <pmtf/wrap.hpp>

namespace gr {

class pyblock_detail;
/**
 * @brief The abstract base class for all signal processing blocks in the GR
 * Block Library
 *
 * Blocks are the bare abstraction of an entity that has a name and a set of
 * inputs and outputs  These are never instantiated directly; rather, this is
 * the abstract parent class of blocks that implement actual signal processing
 * functions.
 *
 */
class GR_RUNTIME_API block : public gr::node, public std::enable_shared_from_this<block>
{
private:
    bool d_running = false;
    tag_propagation_policy_t d_tag_propagation_policy;
    int d_output_multiple = 1;
    bool d_output_multiple_set = false;
    double d_relative_rate = 1.0;

protected:
    neighbor_interface_sptr p_scheduler = nullptr;
    std::map<std::string, int> d_param_str_map;
    message_port_sptr _msg_param_update;
    std::shared_ptr<pyblock_detail> d_pyblock_detail;

    void notify_scheduler();
    void notify_scheduler_input();
    void notify_scheduler_output();

public:
    /**
     * @brief Construct a new block object
     *
     * @param name The non-unique name of this block representing the block type
     */
    block(const std::string& name);
    virtual ~block(){};
    virtual bool start();
    virtual bool stop();
    virtual bool done();

    typedef std::shared_ptr<block> sptr;
    sptr base() { return shared_from_this(); }

    tag_propagation_policy_t tag_propagation_policy();
    void set_tag_propagation_policy(tag_propagation_policy_t policy);
    void set_pyblock_detail(std::shared_ptr<pyblock_detail> p);
    std::shared_ptr<pyblock_detail> pb_detail();
    /**
     * @brief Abstract method to call signal processing work from a derived block
     *
     * @param work_input Vector of block_work_input structs
     * @param work_output Vector of block_work_output structs
     * @return work_return_code_t
     */
    virtual work_return_code_t work(std::vector<block_work_input_sptr>& work_input,
                                    std::vector<block_work_output_sptr>& work_output)
    {
        throw std::runtime_error("work function has been called but not implemented");
    }

    /**
     * @brief Wrapper for work to perform special checks and take care of special
     * cases for certain types of blocks, e.g. sync_block, decim_block
     *
     * @param work_input Vector of block_work_input structs
     * @param work_output Vector of block_work_output structs
     * @return work_return_code_t
     */
    virtual work_return_code_t do_work(std::vector<block_work_input_sptr>& work_input,
                                       std::vector<block_work_output_sptr>& work_output)
    {
        return work(work_input, work_output);
    };

    void set_parent_intf(neighbor_interface_sptr sched) { p_scheduler = sched; }
    parameter_config d_parameters;
    void add_param(param_sptr p) { d_parameters.add(p); }
    pmtf::wrap request_parameter_query(int param_id);
    void request_parameter_change(int param_id, pmtf::wrap new_value, bool block = true);
    virtual void on_parameter_change(param_action_sptr action);
    virtual void on_parameter_query(param_action_sptr action);
    static void consume_each(int num, std::vector<block_work_input_sptr>& work_input);
    static void produce_each(int num, std::vector<block_work_output_sptr>& work_output);
    void set_output_multiple(int multiple);
    int output_multiple() const { return d_output_multiple; }
    bool output_multiple_set() const { return d_output_multiple_set; }
    void set_relative_rate(double relative_rate) { d_relative_rate = relative_rate; }
    double relative_rate() const { return d_relative_rate; }

    virtual int get_param_id(const std::string& id) { return d_param_str_map[id]; }

    /**
     * Every Block should have a param update message handler
     */
    virtual void handle_msg_param_update(pmtf::wrap msg);
    gpdict attributes; // this is a HACK for storing metadata.  Needs to go.
};

typedef block::sptr block_sptr;
typedef std::vector<block_sptr> block_vector_t;
typedef std::vector<block_sptr>::iterator block_viter_t;

} // namespace gr
