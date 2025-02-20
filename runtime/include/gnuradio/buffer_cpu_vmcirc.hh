#pragma once

#include <gnuradio/buffer.hh>
#include <mutex>
#include <string>

// Doubly mapped circular buffer class
// For now, just do this as the sysv_shm flavor
// expand out with the preferences and the factories later

namespace gr {

extern std::mutex s_vm_mutex;

enum class buffer_cpu_vmcirc_type { AUTO, SYSV_SHM, MMAP_SHM, MMAP_TMPFILE };


class buffer_cpu_vmcirc_reader;

class buffer_cpu_vmcirc : public buffer
{
protected:
    uint8_t* _buffer;

public:
    typedef std::shared_ptr<buffer_cpu_vmcirc> sptr;


    static buffer_sptr make(size_t num_items,
                            size_t item_size,
                            std::shared_ptr<buffer_properties> buffer_properties);

    buffer_cpu_vmcirc(size_t num_items,
                  size_t item_size,
                  size_t granularity,
                  std::shared_ptr<buffer_properties> buf_properties);

    // These methods are common to all the vmcircbufs

    void* read_ptr(size_t index) { return (void*)&_buffer[index]; }
    void* write_ptr();

    // virtual void post_read(int num_items);
    virtual void post_write(int num_items);

    // virtual void copy_items(std::shared_ptr<buffer> from, int nitems);

    virtual std::shared_ptr<buffer_reader> add_reader(std::shared_ptr<buffer_properties> buf_props, size_t itemsize);
};

class buffer_cpu_vmcirc_reader : public buffer_reader
{
public:
    buffer_cpu_vmcirc_reader(buffer_sptr buffer, std::shared_ptr<buffer_properties> buf_props, size_t itemsize, size_t read_index = 0)
        : buffer_reader(buffer, buf_props, itemsize, read_index)
    {
    }

    virtual void post_read(int num_items);
};

class buffer_cpu_vmcirc_properties : public buffer_properties
{
public:
    // typedef sptr std::shared_ptr<buffer_properties>;
    buffer_cpu_vmcirc_properties(buffer_cpu_vmcirc_type buffer_type_ = buffer_cpu_vmcirc_type::AUTO)
        : buffer_properties(), _buffer_type(buffer_type_)

    {
        _bff = buffer_cpu_vmcirc::make;
    }
    buffer_cpu_vmcirc_type buffer_type() { return _buffer_type; }
    static std::shared_ptr<buffer_properties> make(buffer_cpu_vmcirc_type buffer_type_)
    {
        return std::static_pointer_cast<buffer_properties>(
            std::make_shared<buffer_cpu_vmcirc_properties>(buffer_type_));
    }

private:
    buffer_cpu_vmcirc_type _buffer_type;
};

} // namespace gr

#define BUFFER_CPU_VMCIRC_ARGS buffer_cpu_vmcirc_properties::make(buffer_cpu_vmcirc_type::AUTO)
#define BUFFER_CPU_VMCIRC_SYSV_SHM_ARGS \
    buffer_cpu_vmcirc_properties::make(buffer_cpu_vmcirc_type::SYSV_SHM)
#define BUFFER_CPU_VMCIRC_MMAP_SHM_ARGS \
    buffer_cpu_vmcirc_properties::make(buffer_cpu_vmcirc_type::MMAP_SHM)
