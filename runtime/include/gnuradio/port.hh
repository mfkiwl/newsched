#pragma once

#include <gnuradio/api.h>
#include <gnuradio/buffer.hh>
#include <gnuradio/neighbor_interface.hh>
#include <gnuradio/scheduler_message.hh>
#include <gnuradio/parameter_types.hh>
#include <algorithm>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <utility>


namespace gr {

enum class GR_RUNTIME_API port_type_t { STREAM, MESSAGE };

enum class GR_RUNTIME_API port_direction_t {
    INPUT,
    OUTPUT,
    BIDIRECTONAL //?? can it be done
};

class block;

/**
 * @brief Base class for all ports
 *
 * Holds the necessary information to describe the port to the runtime
 *
 */
class GR_RUNTIME_API port_base : public std::enable_shared_from_this<port_base>
{

public:
    typedef std::shared_ptr<port_base> sptr;
    static sptr make(const std::string& name,
                     const port_direction_t direction,
                     const param_type_t data_type = param_type_t::CFLOAT,
                     const port_type_t port_type = port_type_t::STREAM,
                     const std::vector<size_t>& dims = std::vector<size_t>{ 1 },
                     const bool optional = false,
                     const int multiplicity = 1)
    {
        return std::make_shared<port_base>(
            name, direction, data_type, port_type, dims, optional, multiplicity);
    }

    port_base(const std::string& name,
              const port_direction_t direction,
              const param_type_t data_type = param_type_t::CFLOAT,
              const port_type_t port_type = port_type_t::STREAM,
              const std::vector<size_t>& dims = std::vector<size_t>{ 1 },
              const bool optional = false,
              const int multiplicity = 1)
        : _name(name),
          _direction(direction),
          _data_type(data_type),
          _port_type(port_type),
          _dims(dims),
          _optional(optional),
          _multiplicity(multiplicity)
    {
        // _type_info = param_type_info(_data_type); // might not be needed
        _datasize = parameter_functions::param_size_info(_data_type);
        _itemsize = _datasize;

        // If dims is empty or [1], then the port type is a scalar value
        // If dims has values, then the total itemsize is the product of the dimensions *
        // the scalar itemsize
        for (auto d : _dims)
            _itemsize *= d;
    }

    port_base(const std::string& name,
              const port_direction_t direction,
              const size_t itemsize,
              const port_type_t port_type = port_type_t::STREAM,
              const bool optional = false,
              const int multiplicity = 1)
        : _name(name),
          _direction(direction),
          _data_type(param_type_t::UNTYPED),
          _port_type(port_type),
          _optional(optional),
          _multiplicity(multiplicity),
          _datasize(itemsize),
          _itemsize(itemsize)
    {
    }

    virtual ~port_base() = default;

    std::string name() { return _name; }
    std::string alias() { return _alias; }
    void set_alias(const std::string& alias) { _alias = alias; }
    void set_index(int val) { _index = val; }
    int index() { return _index; }
    port_type_t type() { return _port_type; }
    param_type_t data_type() { return _data_type; }
    port_direction_t direction() { return _direction; }
    size_t data_size() { return _datasize; }
    size_t itemsize() { return _itemsize; }
    void set_itemsize(size_t itemsize) { _itemsize = itemsize; }
    std::vector<size_t> dims() { return _dims; }
    sptr base() { return shared_from_this(); }
    bool optional() { return _optional; }
    auto& connected_ports() { return _connected_ports; }

    void set_parent_intf(neighbor_interface_sptr intf) { _parent_intf = intf; }
    std::string format_descriptor()
    {
        if (_format_descriptor != "") {
            return _format_descriptor;
        } else {
            return parameter_functions::get_format_descriptor(_data_type);
        }
    }
    void set_format_descriptor(const std::string& fd) { _format_descriptor = fd; }
    void set_buffer(buffer_sptr buffer) { _buffer = buffer; }
    buffer_sptr buffer() { return _buffer; }
    void set_buffer_reader(buffer_reader_sptr rdr) { _buffer_reader = rdr; }
    buffer_reader_sptr buffer_reader() { return _buffer_reader; }

    void notify_connected_ports(scheduler_message_sptr msg)
    {
        for (auto& p : _connected_ports) {
            p->push_message(msg);
        }

        // FIXME: To achieve maximum performance, we need to stimulate our own
        //  thread by pushing messages into the queue and causing the next
        //  call to work() to be immediately evaluated
        // Without this, performance is significantly worse than the GR TPB
        //  scheduler.  This needs more investigation
        // this->push_message(msg);
    }
    // Inbound messages
    virtual void push_message(scheduler_message_sptr msg)
    {
        // push it to the queue of the owning thread
        if (_parent_intf) {
            _parent_intf->push_message(msg);
        } else {
            throw std::runtime_error("port has no parent interface");
        }
    }

    void connect(sptr other_port)
    {

        auto pred = [other_port](sptr p) { return (p == other_port); };
        std::vector<sptr>::iterator it =
            std::find_if(std::begin(_connected_ports), std::end(_connected_ports), pred);

        if (it == std::end(_connected_ports)) {
            _connected_ports.push_back(
                other_port); // only connect if it is not already in there
        }
    }

protected:
    std::string _name;
    std::string _alias;
    port_direction_t _direction;
    param_type_t _data_type;
    port_type_t _port_type;
    int _index = -1;           // how does this get set??
    std::vector<size_t> _dims; // allow for matrices to be sent naturally across ports
    // empty dims refers to a scalar, dims=[n] same as vlen=n
    bool _optional;
    int _multiplicity; // port can be replicated as in grc
    size_t _datasize;
    size_t _itemsize; // data size across all dims
    std::string _format_descriptor = "";

    std::vector<sptr> _connected_ports;
    neighbor_interface_sptr _parent_intf = nullptr;
    buffer_sptr _buffer = nullptr;
    buffer_reader_sptr _buffer_reader = nullptr;

    block* _parent_block = nullptr;
};

typedef port_base::sptr port_sptr;
typedef std::vector<port_sptr> port_vector_t;


/**
 * @brief Typed port class
 *
 * Wraps the port_base class with a type to take care of all the sizing and lower level
 * properties
 *
 * @tparam T datatype to instantiate the base port class
 */
template <class T>
class GR_RUNTIME_API port : public port_base
{
public:
    static std::shared_ptr<port<T>> make(const std::string& name,
                                         const port_direction_t direction,
                                         const std::vector<size_t>& dims = { 1 },
                                         const bool optional = false,
                                         const int multiplicity = 1)
    {
        return std::shared_ptr<port<T>>(
            new port<T>(name, direction, dims, optional, multiplicity));
    }
    port(const std::string& name,
         const port_direction_t direction,
         const std::vector<size_t>& dims = { 1 },
         const bool optional = false,
         const int multiplicity = 1)
        : port_base(name,
                    //    parent,
                    direction,
                    parameter_functions::get_param_type_from_typeinfo(
                        std::type_index(typeid(T))),
                    port_type_t::STREAM,
                    dims,
                    optional,
                    multiplicity)
    {
    }
};


/**
 * @brief Untyped port class
 *
 * Wraps the port base class but only populates stream size info.  To be used in case of
 * e.g. head block where the underlying datatype is not used, just copied byte for byte
 *
 */
class GR_RUNTIME_API untyped_port : public port_base
{
public:
    static std::shared_ptr<untyped_port> make(const std::string& name,
                                              const port_direction_t direction,
                                              const size_t itemsize,
                                              const bool optional = false,
                                              const int multiplicity = 1)
    {
        return std::shared_ptr<untyped_port>(
            new untyped_port(name, direction, itemsize, optional, multiplicity));
    }
    untyped_port(const std::string& name,
                 const port_direction_t direction,
                 const size_t itemsize,
                 const bool optional = false,
                 const int multiplicity = 1)
        : port_base(
              name, direction, itemsize, port_type_t::STREAM, optional, multiplicity)
    {
    }
};


/**
 * @brief Message port class
 *
 * Wraps the port_base class to provide a message port where streaming parameters are
 * absent and the type is MESSAGE
 *
 */
class GR_RUNTIME_API message_port : public port_base
{
private: //
    message_port_callback_fcn _callback_fcn;

public:
    typedef std::shared_ptr<message_port> sptr;
    static sptr make(const std::string& name,
                     const port_direction_t direction,
                     const bool optional = true,
                     const int multiplicity = 1)
    {
        return std::make_shared<message_port>(name, direction, optional, multiplicity);
    }
    message_port(const std::string& name,
                 const port_direction_t direction,
                 const bool optional = false,
                 const int multiplicity = 1)
        : port_base(name, direction, 0, port_type_t::MESSAGE, optional, multiplicity)
    {
    }


    message_port_callback_fcn callback() { return _callback_fcn; }
    void register_callback(message_port_callback_fcn fcn) { _callback_fcn = fcn; }
    void post(pmtf::wrap msg) // should be a pmt, just pass strings for now
    {
        notify_connected_ports(std::make_shared<msgport_message>(msg, _callback_fcn));
    }
    virtual void push_message(scheduler_message_sptr msg)
    {
        auto m = std::static_pointer_cast<msgport_message>(msg);
        m->set_callback(callback());

        // push it to the queue of the owning thread
        if (_parent_intf) {
            _parent_intf->push_message(msg);
        } else {
            throw std::runtime_error("port has no parent interface");
        }
    }
};
typedef message_port::sptr message_port_sptr;

} // namespace gr
