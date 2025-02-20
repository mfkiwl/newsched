#pragma once

#include <gnuradio/qtgui/time_sink.hh>

#include <gnuradio/high_res_timer.hh>
#include <gnuradio/qtgui/timedisplayform.h>

#include <mutex>

namespace gr {
namespace qtgui {

template <class T>
class time_sink_cpu : public time_sink<T>
{
public:
    time_sink_cpu(const typename time_sink<T>::block_args& args);

    virtual work_return_code_t work(std::vector<block_work_input_sptr>& work_input,
                                    std::vector<block_work_output_sptr>& work_output) override;

    virtual void exec_() { d_qApplication->exec(); };
    virtual QWidget* qwidget() { return d_main_gui; };
    void set_y_axis(double min, double max) override;
    void set_y_label(const std::string& label, const std::string& unit = "") override;
    void set_update_time(double t) override;
    void set_title(const std::string& title) override;
    void set_line_label(unsigned int which, const std::string& label) override;
    void set_line_color(unsigned int which, const std::string& color) override;
    void set_line_width(unsigned int which, int width) override;
    void set_line_style(unsigned int which, int style) override;
    void set_line_marker(unsigned int which, int marker) override;
    void set_nsamps(const int size) override;
    void set_samp_rate(const double samp_rate) override;
    void set_line_alpha(unsigned int which, double alpha) override;
    // void set_trigger_mode(trigger_mode mode,
    //                       trigger_slope slope,
    //                       float level,
    //                       float delay,
    //                       int channel,
    //                       const std::string& tag_key = "") override;

    // std::string title() override;
    // std::string line_label(unsigned int which) override;
    // std::string line_color(unsigned int which) override;
    // int line_width(unsigned int which) override;
    // int line_style(unsigned int which) override;
    // int line_marker(unsigned int which) override;
    // double line_alpha(unsigned int which) override;

    void set_size(int width, int height) override;

    // int nsamps() const override;

    // void enable_menu(bool en) override;
    // void enable_grid(bool en) override;
    // void enable_autoscale(bool en) override;
    // void enable_stem_plot(bool en) override;
    // void enable_semilogx(bool en) override;
    // void enable_semilogy(bool en) override;
    // void enable_control_panel(bool en) override;
    // void enable_tags(unsigned int which, bool en) override;
    // void enable_tags(bool en) override;
    // void enable_axis_labels(bool en) override;
    // void disable_legend() override;

    // void reset() override;



private:
    std::mutex d_setlock;
    void initialize();

    int d_size, d_buffer_size;
    double d_samp_rate;
    const std::string d_name;
    unsigned int d_nconnections;

    const pmtf::wrap d_tag_key;

    int d_index, d_start, d_end;
    std::vector<volk::vector<T>> d_Tbuffers;
    std::vector<volk::vector<double>> d_buffers;
    std::vector<std::vector<gr::tag_t>> d_tags;

    // Required now for Qt; argc must be greater than 0 and argv
    // must have at least one valid character. Must be valid through
    // life of the qApplication:
    // http://harmattan-dev.nokia.com/docs/library/html/qt4/qapplication.html
    char d_zero;
    int d_argc = 1;
    char* d_argv = &d_zero;
    QWidget* d_parent = nullptr;
    TimeDisplayForm* d_main_gui = nullptr;

    gr::high_res_timer_type d_update_time;
    gr::high_res_timer_type d_last_time;

    // Members used for triggering scope
    trigger_mode d_trigger_mode;
    trigger_slope d_trigger_slope;
    float d_trigger_level;
    int d_trigger_channel = 0;
    int d_trigger_delay = 0;
    pmtf::wrap d_trigger_tag_key;
    bool d_triggered;
    int d_trigger_count;

    void _reset();
    void _npoints_resize();
    void _adjust_tags(int adj);
    void _gui_update_trigger();
    void _test_trigger_tags(int nitems);
    void _test_trigger_norm(int nitems, gr_vector_const_void_star inputs);
    bool _test_trigger_slope(const T* in) const;

    // Handles message input port for displaying PDU samples.
    void handle_pdus(pmtf::wrap msg);

    QApplication* d_qApplication;

};


} // namespace qtgui
} // namespace gr
