/* -*- c++ -*- */
/*
 * Copyright 2021 Jeff Long
 * Copyright 2018-2021 Libre Space Foundation <http://libre.space>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "block_impl.h"
#include "setting_string_conversion.h"
#include <SoapySDR/Formats.h>
#include <SoapySDR/Version.hpp>
#include <fmt/core.h>
#include <cmath>
#include <numeric>
#include <pmtf/string.hpp>
#include <pmtf/scalar.hpp>

namespace gr {
namespace soapy {

static bool arg_info_has_key(const arginfo_list_t& info_list, const std::string& key)
{
    return std::any_of(info_list.begin(), info_list.end(), [key](const arginfo_t& info) {
        return info.key == key;
    });
}

template <typename T>
static inline bool vector_contains(const std::vector<T>& vec, const std::string& elem)
{
    return (std::find(vec.begin(), vec.end(), elem) != vec.end());
}

static inline bool value_in_range(const range_t& range, double value)
{
    return (value >= range.minimum()) && (value <= range.maximum());
}

static inline bool value_in_ranges(const range_list_t& ranges, double value)
{
    return std::any_of(ranges.begin(), ranges.end(), [value](const range_t& range) {
        return value_in_range(range, value);
    });
}

static std::string string_vector_to_string(const std::vector<std::string>& vec)
{
    if (vec.empty())
        return "[]";

    std::string str = "[";
    for (const auto& vecStr : vec) {
        str += vecStr;
        str += ", ";
    }
    str.erase(str.length() - 2, 2);

    str += "]";

    return str;
}

static std::string range_to_string(const range_t& range)
{
    const auto min = range.minimum();
    const auto max = range.maximum();

    if (min == max)
        return std::to_string(min);
    else
        return "[" + std::to_string(min) + ", " + std::to_string(max) + "]";
}

static std::string ranges_to_string(const range_list_t& ranges)
{
    if (ranges.empty())
        return "[]";

    std::string str;
    for (const auto& range : ranges) {
        str += range_to_string(range);
        str += ", ";
    }
    str.erase(str.length() - 2, 2);

    return str;
}

static void check_abi(void)
{
    const std::string buildtime_abi = SOAPY_SDR_ABI_VERSION;
    const std::string runtime_abi = SoapySDR::getABIVersion();

    if (buildtime_abi != runtime_abi) {
        throw std::runtime_error(
            fmt::format("\nGR-Soapy detected ABI compatibility mismatch with SoapySDR library.\n"
                "GR-Soapy was built against ABI: {},\n"
                "but the SoapySDR library reports ABI: {}\n"
                "Suggestion: install an ABI compatible version of SoapySDR,\n"
                "or rebuild GR-Soapy component against this ABI version.\n", buildtime_abi, runtime_abi));
    }
}

const std::string CMD_CHAN_KEY = "chan";
const std::string CMD_FREQ_KEY = "freq";
const std::string CMD_GAIN_KEY = "gain";
const std::string CMD_ANTENNA_KEY = "antenna";
const std::string CMD_RATE_KEY = "rate";
const std::string CMD_BW_KEY = "bandwidth";
const std::string CMD_GAINMODE_KEY = "gain_mode";
const std::string CMD_FREQCORR_KEY = "freq_corr";
const std::string CMD_DCOFFSETMODE_KEY = "dc_offset_mode";
const std::string CMD_DCOFFSET_KEY = "dc_offset";
const std::string CMD_IQBAL_KEY = "iq_bal";
const std::string CMD_IQBALMODE_KEY = "iq_bal_mode";
const std::string CMD_MCR_KEY = "master_clock_rate";
const std::string CMD_REFCLOCKRATE_KEY = "ref_clock_rate";
const std::string CMD_CLOCKSRC_KEY = "clock_src";
const std::string CMD_TIMESRC_KEY = "time_src";
const std::string CMD_HWTIME_KEY = "hw_time";
const std::string CMD_REG_KEY = "reg";
const std::string CMD_REGS_KEY = "regs";
const std::string CMD_SETTING_KEY = "setting";
const std::string CMD_GPIO_KEY = "gpio";
const std::string CMD_GPIODIR_KEY = "gpio_dir";
const std::string CMD_I2C_KEY = "i2c";
const std::string CMD_UART_KEY = "uart";

const std::string CMD_NAME_KEY = "name";
const std::string CMD_KEY_KEY = "key";
const std::string CMD_VALUE_KEY = "value";
const std::string CMD_ADDR_KEY = "addr";
const std::string CMD_MASK_KEY = "mask";
const std::string CMD_TIME_KEY = "time";
const std::string CMD_BANK_KEY = "bank";
const std::string CMD_DATA_KEY = "data";

// Common default values for pmt::dict_ref
const pmtf::wrap PMT_EMPTYSTR = pmtf::string("");
const pmtf::wrap PMT_ZERO = pmtf::scalar<int>(0);

/*
 * The private constructor
 */
block_impl::block_impl(int direction,
                       const std::string& device,
                       const std::string& type,
                       size_t nchan,
                       const std::string& dev_args,
                       const std::string& stream_args,
                       const std::vector<std::string>& tune_args,
                       const std::vector<std::string>& other_settings)
    : gr::block("soapy_block"),
      d_direction(direction),
      d_nchan(nchan),
      d_stream_args(stream_args),
      d_channels(nchan)
{
    check_abi();

    // Channel list
    std::iota(std::begin(d_channels), std::end(d_channels), 0);

    // Must be 1 or nchan.
    if (nchan != tune_args.size() && tune_args.size() != 1) {
        throw std::invalid_argument(name() +
                                    ": Wrong number of channels and tune arguments");
    }

    // Must be 1 or nchan.
    if (nchan != other_settings.size() && other_settings.size() != 1) {
        throw std::invalid_argument(name() + ": Wrong number of channels and settings");
    }

    // Convert type to Soapy type.
    if (type == "fc32" || type == SOAPY_SDR_CF32) {
        d_soapy_type = SOAPY_SDR_CF32;
    } else if (type == "sc16" || type == SOAPY_SDR_CS16) {
        d_soapy_type = SOAPY_SDR_CS16;
    } else if (type == "sc8" || type == SOAPY_SDR_CS8) {
        d_soapy_type = SOAPY_SDR_CS8;
    } else {
        throw std::invalid_argument(name() + ": Invalid IO type");
    }

    kwargs_t dev_kwargs = SoapySDR::KwargsFromString(device + ", " + dev_args);
    d_device = device_ptr_t(SoapySDR::Device::make(dev_kwargs));

    /* Some of the arguments maybe device settings, so we need to re-apply them */
    arginfo_list_t supported_settings = d_device->getSettingInfo();
    for (const auto& arg : dev_kwargs) {
        for (const auto& supported : supported_settings) {
            if (supported.key == arg.first) {
                d_device->writeSetting(arg.first, arg.second);
            }
        }
    }

    if (d_nchan > d_device->getNumChannels(d_direction)) {
        throw std::invalid_argument(
            name() + ": Unsupported number of channels. Only  " +
            std::to_string(d_device->getNumChannels(d_direction)) +
            " channels available.");
    }

    /*
     * Validate stream arguments for all channels
     */
    for (const auto& channel : d_channels) {
        arginfo_list_t supported_args = d_device->getStreamArgsInfo(d_direction, channel);
        kwargs_t stream_kwargs = SoapySDR::KwargsFromString(stream_args);

        for (const auto& arg : stream_kwargs) {
            if (!arg_info_has_key(supported_args, arg.first)) {
                throw std::invalid_argument(name() + ": Unsupported stream argument " +
                                            arg.first + " for channel " +
                                            std::to_string(channel));
            }
        }
    }

    /* Validate tuning arguments for each channel */
    for (const auto& channel : d_channels) {
        arginfo_list_t supported_args =
            d_device->getFrequencyArgsInfo(d_direction, channel);

        /* If single set of args, apply to all channels */
        const size_t arg_idx = tune_args.size() == 1 ? 0 : channel;
        kwargs_t tune_kwargs = SoapySDR::KwargsFromString(tune_args[arg_idx]);

        for (const auto& arg : tune_kwargs) {
            if (!arg_info_has_key(supported_args, arg.first)) {
                throw std::invalid_argument(name() + ": Unsupported tune argument " +
                                            arg.first + " for channel " +
                                            std::to_string(channel));
            }
        }
        d_tune_args.push_back(tune_kwargs);
    }

    /* Validate and apply other settings to each channel */
    for (const auto& channel : d_channels) {
        arginfo_list_t supported_settings =
            d_device->getSettingInfo(d_direction, channel);

        /* If single set of args, apply to all channels */
        const size_t arg_idx = other_settings.size() == 1 ? 0 : channel;
        kwargs_t settings_kwargs = SoapySDR::KwargsFromString(other_settings[arg_idx]);

        /*
         * Before applying the setting, make a last check if the setting
         * is a valid gain
         */
        std::vector<std::string> gain_names = d_device->listGains(d_direction, channel);
        for (const auto& gain_name : gain_names) {
            SoapySDR::Kwargs::iterator iter = settings_kwargs.find(gain_name);
            if (iter != settings_kwargs.end()) {
                d_device->setGain(
                    d_direction, channel, gain_name, std::stod(iter->second));
                /*
                 * As only valid settings are applied, we remove it so it does not
                 * recognized as an invalid setting
                 */
                settings_kwargs.erase(iter);
            }
        }

        for (const auto& arg : settings_kwargs) {
            if (!arg_info_has_key(supported_settings, arg.first)) {
                throw std::invalid_argument(name() + ": Unsupported setting " +
                                            arg.first + " for channel " +
                                            std::to_string(channel));
            }
            d_device->writeSetting(d_direction, channel, arg.first, arg.second);
        }
    }

    // message_port_register_in(pmtf::string("cmd"));
    // set_msg_handler(pmtf::string("cmd"),
    //                 [this](pmtf::wrap pmt) { this->msg_handler_cmd(pmt); });
    // register_msg_cmd_handler(CMD_FREQ_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_frequency(val, channel);
    // });
    // register_msg_cmd_handler(CMD_GAIN_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_gain(val, channel);
    // });

    // register_msg_cmd_handler(CMD_RATE_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_samp_rate(val, channel);
    // });
    // register_msg_cmd_handler(CMD_BW_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_bw(val, channel);
    // });
    // register_msg_cmd_handler(CMD_ANTENNA_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_antenna(val, channel);
    // });
    // register_msg_cmd_handler(CMD_GAINMODE_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_gain_mode(val, channel);
    // });
    // register_msg_cmd_handler(CMD_FREQCORR_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_frequency_correction(val, channel);
    // });
    // register_msg_cmd_handler(CMD_FREQCORR_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_frequency_correction(val, channel);
    // });
    // register_msg_cmd_handler(CMD_DCOFFSETMODE_KEY,
    //                          [this](pmtf::wrap val, size_t channel) {
    //                              this->cmd_handler_dc_offset_mode(val, channel);
    //                          });
    // register_msg_cmd_handler(CMD_DCOFFSET_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_dc_offset(val, channel);
    // });
    // register_msg_cmd_handler(CMD_IQBAL_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_iq_balance(val, channel);
    // });
    // register_msg_cmd_handler(CMD_IQBALMODE_KEY, [this](pmtf::wrap val, size_t channel)
    // {
    //     this->cmd_handler_iq_balance_mode(val, channel);
    // });
    // register_msg_cmd_handler(CMD_MCR_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_master_clock_rate(val, channel);
    // });
    // register_msg_cmd_handler(CMD_REFCLOCKRATE_KEY,
    //                          [this](pmtf::wrap val, size_t channel) {
    //                              this->cmd_handler_reference_clock_rate(val, channel);
    //                          });
    // register_msg_cmd_handler(CMD_CLOCKSRC_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_clock_source(val, channel);
    // });
    // register_msg_cmd_handler(CMD_TIMESRC_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_time_source(val, channel);
    // });
    // register_msg_cmd_handler(CMD_HWTIME_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_hardware_time(val, channel);
    // });
    // register_msg_cmd_handler(CMD_REG_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_register(val, channel);
    // });
    // register_msg_cmd_handler(CMD_REGS_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_registers(val, channel);
    // });
    // register_msg_cmd_handler(CMD_SETTING_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_setting(val, channel);
    // });
    // register_msg_cmd_handler(CMD_GPIO_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_gpio(val, channel);
    // });
    // register_msg_cmd_handler(CMD_GPIODIR_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_gpio_dir(val, channel);
    // });
    // register_msg_cmd_handler(CMD_I2C_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_i2c(val, channel);
    // });
    // register_msg_cmd_handler(CMD_UART_KEY, [this](pmtf::wrap val, size_t channel) {
    //     this->cmd_handler_uart(val, channel);
    // });
}

bool block_impl::start()
{
    std::lock_guard<std::mutex> l(d_device_mutex);

    d_stream = d_device->setupStream(
        d_direction, d_soapy_type, d_channels, SoapySDR::KwargsFromString(d_stream_args));
    d_mtu = d_device->getStreamMTU(d_stream);

    // /* This limits each work invocation to MTU transfers */
    // if (d_mtu > 0) {
    //     set_max_noutput_items(d_mtu);
    // } else {
    //     set_max_noutput_items(1024);
    // }

    d_device->activateStream(d_stream);

    return true;
}

bool block_impl::stop()
{
    if (d_stream) {
        std::lock_guard<std::mutex> l(d_device_mutex);
        d_device->closeStream(d_stream);
        d_stream = nullptr;
    }

    return true;
}

block_impl::~block_impl() {}

void block_impl::register_msg_cmd_handler(const pmtf::wrap& cmd, cmd_handler_t handler)
{
    // d_cmd_handlers[cmd] = handler;
}

void block_impl::validate_channel(size_t channel) const
{
    if (channel >= d_nchan) {
        throw std::invalid_argument(name() + ": Channel " + std::to_string(channel) +
                                    " is not activated.");
    }
}

std::string block_impl::get_driver_key() const { return d_device->getDriverKey(); }

std::string block_impl::get_hardware_key() const { return d_device->getHardwareKey(); }

kwargs_t block_impl::get_hardware_info() const { return d_device->getHardwareInfo(); }

void block_impl::set_frontend_mapping(const std::string& mapping)
{
    d_device->setFrontendMapping(d_direction, mapping);
}

std::string block_impl::get_frontend_mapping() const
{
    return d_device->getFrontendMapping(d_direction);
}

kwargs_t block_impl::get_channel_info(size_t channel) const
{
    validate_channel(channel);
    return d_device->getChannelInfo(d_direction, channel);
}

void block_impl::set_sample_rate(size_t channel, double sample_rate)
{
    validate_channel(channel);

    range_list_t sps_range = d_device->getSampleRateRange(d_direction, channel);

    if (!value_in_ranges(sps_range, sample_rate)) {
        std::string msg = name() + ": Unsupported sample rate (" +
                          std::to_string(sample_rate) + ").  Rate must be in the range ";
        msg += ranges_to_string(sps_range);

        throw std::invalid_argument(msg);
    }

    d_device->setSampleRate(d_direction, channel, sample_rate);
}

double block_impl::get_sample_rate(size_t channel) const
{
    validate_channel(channel);
    return d_device->getSampleRate(d_direction, channel);
}

range_list_t block_impl::get_sample_rate_range(size_t channel) const
{
    validate_channel(channel);
    return d_device->getSampleRateRange(d_direction, channel);
}

void block_impl::set_frequency(size_t channel, double frequency)
{
    validate_channel(channel);
    d_device->setFrequency(d_direction, channel, frequency, d_tune_args[channel]);
}

void block_impl::set_frequency(size_t channel, const std::string& name, double frequency)
{
    validate_channel(channel);
    std::vector<std::string> freqs = d_device->listFrequencies(d_direction, channel);

    // Set frequency for specified element, silently ignoring a non-existant
    // element if frequency is zero.
    if (vector_contains(freqs, name)) {
        d_device->setFrequency(d_direction, channel, name, frequency);
    } else if (std::fpclassify(std::abs(frequency)) != FP_ZERO) {
        throw std::invalid_argument(
            this->name() + ": Channel " + std::to_string(channel) +
            " does not support frequency " + name +
            ". Consider setting this frequency to 0 to bypass this error");
    }
}

double block_impl::get_frequency(size_t channel) const
{
    validate_channel(channel);
    return d_device->getFrequency(d_direction, channel);
}

double block_impl::get_frequency(size_t channel, const std::string& name) const
{
    validate_channel(channel);
    return d_device->getFrequency(d_direction, channel, name);
}

std::vector<std::string> block_impl::list_frequencies(size_t channel) const
{
    validate_channel(channel);
    return d_device->listFrequencies(d_direction, channel);
}

range_list_t block_impl::get_frequency_range(size_t channel) const
{
    validate_channel(channel);
    return d_device->getFrequencyRange(d_direction, channel);
}

range_list_t block_impl::get_frequency_range(size_t channel,
                                             const std::string& name) const
{
    validate_channel(channel);
    return d_device->getFrequencyRange(d_direction, channel, name);
}

arginfo_list_t block_impl::get_frequency_args_info(size_t channel) const
{
    validate_channel(channel);
    return d_device->getFrequencyArgsInfo(d_direction, channel);
}

void block_impl::set_bandwidth(size_t channel, double bandwidth)
{
    validate_channel(channel);
    d_device->setBandwidth(d_direction, channel, bandwidth);
}

double block_impl::get_bandwidth(size_t channel) const
{
    validate_channel(channel);
    return d_device->getBandwidth(d_direction, channel);
}

range_list_t block_impl::get_bandwidth_range(size_t channel) const
{
    validate_channel(channel);
    return d_device->getBandwidthRange(d_direction, channel);
}

std::vector<std::string> block_impl::list_antennas(int channel) const
{
    validate_channel(channel);
    return d_device->listAntennas(d_direction, channel);
}

void block_impl::set_antenna(const size_t channel, const std::string& name)
{
    validate_channel(channel);
    std::vector<std::string> antennas = d_device->listAntennas(d_direction, channel);

    // Ignore call if there are no antennas.
    if (antennas.empty()) {
        return;
    }

    if (!vector_contains(antennas, name)) {
        std::string msg = this->name() + ": Antenna " + name + " at channel " +
                          std::to_string(channel) + " is not supported. " +
                          "Available antennas are: ";
        msg += string_vector_to_string(antennas);

        throw std::invalid_argument(msg);
    }

    d_device->setAntenna(d_direction, channel, name);
}

std::string block_impl::get_antenna(size_t channel) const
{
    validate_channel(channel);
    return d_device->getAntenna(d_direction, channel);
}

bool block_impl::has_gain_mode(size_t channel) const
{
    validate_channel(channel);
    return d_device->hasGainMode(d_direction, channel);
}

void block_impl::set_gain_mode(size_t channel, bool enable)
{
    validate_channel(channel);
    // TODO: ignoring error if disabling AGC ... is this a good idea?
    if (!has_gain_mode(channel) && enable) {
        throw std::invalid_argument(name() + ": Channel " + std::to_string(channel) +
                                    " does not support AGC");
    }
    d_device->setGainMode(d_direction, channel, enable);
}

bool block_impl::get_gain_mode(size_t channel) const
{
    validate_channel(channel);
    return d_device->getGainMode(d_direction, channel);
}

std::vector<std::string> block_impl::list_gains(size_t channel) const
{
    validate_channel(channel);
    return d_device->listGains(d_direction, channel);
}

void block_impl::set_gain(size_t channel, double gain)
{
    validate_channel(channel);
    range_t rGain = d_device->getGainRange(d_direction, channel);

    if (!value_in_range(rGain, gain)) {
        GR_LOG_ERROR(_debug_logger,
                     "Gain out of range: {} <= gain <= {}",
                     rGain.minimum(),
                     rGain.maximum());
        return;
    }

    d_device->setGain(d_direction, channel, gain);
}

void block_impl::set_gain(size_t channel, const std::string& name, double gain)
{
    validate_channel(channel);

    /* Validate gain name */
    std::vector<std::string> gains = d_device->listGains(d_direction, channel);
    if (!vector_contains(gains, name)) {
        throw std::invalid_argument(this->name() + ": Unknown gain " + name +
                                    " for channel " + std::to_string(channel));
    }

    /* Validate gain value */
    range_t rGain = d_device->getGainRange(d_direction, channel, name);
    if (!value_in_range(rGain, gain)) {
        GR_LOG_ERROR(_debug_logger,
                     "Gain {} out of range: {} <= gain <= {}" , name ,
                         rGain.minimum() , rGain.maximum());
    }

    d_device->setGain(d_direction, channel, name, gain);
}

double block_impl::get_gain(size_t channel) const
{
    validate_channel(channel);
    return d_device->getGain(d_direction, channel);
}

double block_impl::get_gain(size_t channel, const std::string& name) const
{
    validate_channel(channel);
    return d_device->getGain(d_direction, channel, name);
}

range_t block_impl::get_gain_range(size_t channel) const
{
    validate_channel(channel);
    return d_device->getGainRange(d_direction, channel);
}

range_t block_impl::get_gain_range(size_t channel, const std::string& name) const
{
    validate_channel(channel);
    return d_device->getGainRange(d_direction, channel, name);
}

bool block_impl::has_frequency_correction(size_t channel) const
{
    validate_channel(channel);
    return d_device->hasFrequencyCorrection(d_direction, channel);
}

void block_impl::set_frequency_correction(size_t channel, double freq_correction)
{
    validate_channel(channel);

    // Set frequency correction, ignoring missing functionality if correction
    // is zero.
    if (has_frequency_correction(channel)) {
        d_device->setFrequencyCorrection(d_direction, channel, freq_correction);
    } else if (std::fpclassify(std::abs(freq_correction)) != FP_ZERO) {
        throw std::invalid_argument(this->name() + ": Channel " +
                                    std::to_string(channel) +
                                    " does not support frequency correction setting");
    }
}

double block_impl::get_frequency_correction(size_t channel) const
{
    validate_channel(channel);
    return d_device->getFrequencyCorrection(d_direction, channel);
}

bool block_impl::has_dc_offset_mode(size_t channel) const
{
    validate_channel(channel);
    return d_device->hasDCOffsetMode(d_direction, channel);
}

void block_impl::set_dc_offset_mode(size_t channel, bool automatic)
{
    validate_channel(channel);

    // TODO: ignoring error if disabling automatic ... is this a good idea?
    if (!has_dc_offset_mode(channel) && automatic) {
        throw std::invalid_argument(this->name() + ": Channel " +
                                    std::to_string(channel) +
                                    " does not support automatic DC removal setting");
    }
    d_device->setDCOffsetMode(d_direction, channel, automatic);
}

bool block_impl::get_dc_offset_mode(size_t channel) const
{
    validate_channel(channel);
    return d_device->getDCOffsetMode(d_direction, channel);
}

bool block_impl::has_dc_offset(size_t channel) const
{
    validate_channel(channel);
    return d_device->hasDCOffset(d_direction, channel);
}

void block_impl::set_dc_offset(size_t channel, const gr_complexd& dc_offset)
{
    validate_channel(channel);

    const bool supported = has_dc_offset(channel);
    /*
     * In case of 0 DC offset, the method exits to avoid failing in case of the
     * device does not supports DC offset
     */
    if (std::fpclassify(std::norm(dc_offset)) == FP_ZERO) {
        if (supported && !d_device->getDCOffsetMode(d_direction, channel)) {
            d_device->setDCOffset(d_direction, channel, dc_offset);
        }
        return;
    }

    if (!supported) {
        throw std::invalid_argument(this->name() + ": Channel " +
                                    std::to_string(channel) +
                                    " does not support DC offset setting");
    }
    d_device->setDCOffset(d_direction, channel, dc_offset);
}

gr_complexd block_impl::get_dc_offset(size_t channel) const
{
    validate_channel(channel);
    return d_device->getDCOffset(d_direction, channel);
}

bool block_impl::has_iq_balance(size_t channel) const
{
    validate_channel(channel);
    return d_device->hasIQBalance(d_direction, channel);
}

void block_impl::set_iq_balance(size_t channel, const gr_complexd& iq_balance)
{
    validate_channel(channel);

    const bool supported = has_iq_balance(channel);
    /*
     * In case of 0 frequency correction, the method exits to avoid failing
     * in case of the device does not supports DC offset
     */
    if (std::fpclassify(std::norm(iq_balance)) == FP_ZERO) {
        if (supported) {
            d_device->setIQBalance(d_direction, channel, iq_balance);
        }
        return;
    }

    if (!supported) {
        throw std::invalid_argument(this->name() + ": Channel " +
                                    std::to_string(channel) +
                                    " does not support IQ imbalance correction setting");
    }
    d_device->setIQBalance(d_direction, channel, iq_balance);
}

gr_complexd block_impl::get_iq_balance(size_t channel) const
{
    validate_channel(channel);
    return d_device->getIQBalance(d_direction, channel);
}

bool block_impl::has_iq_balance_mode(size_t channel) const
{
    validate_channel(channel);

#ifdef SOAPY_SDR_API_HAS_IQ_BALANCE_MODE
    return d_device->hasIQBalanceMode(d_direction, channel);
#else
    (void)channel;
    throw std::runtime_error(
        "Automatic IQ imbalance correction API not implemented in this version");
#endif
}

void block_impl::set_iq_balance_mode(size_t channel, bool automatic)
{
    validate_channel(channel);

#ifdef SOAPY_SDR_API_HAS_IQ_BALANCE_MODE
    if (!has_iq_balance_mode(channel)) {
        throw std::invalid_argument(
            this->name() + ": Channel " + std::to_string(channel) +
            " does not support automatic IQ imbalance correction");
    }
    d_device->setIQBalanceMode(d_direction, channel, automatic);
#else
    (void)channel;
    (void)automatic;
    throw std::runtime_error(
        "Automatic IQ imbalance correction API not implemented in this version");
#endif
}

bool block_impl::get_iq_balance_mode(size_t channel) const
{
    validate_channel(channel);

#ifdef SOAPY_SDR_API_HAS_IQ_BALANCE_MODE
    if (!has_iq_balance_mode(channel)) {
        throw std::invalid_argument(
            this->name() + ": Channel " + std::to_string(channel) +
            " does not support automatic IQ imbalance correction");
    }
    return d_device->getIQBalanceMode(d_direction, channel);
#else
    (void)channel;
    throw std::runtime_error(
        "Automatic IQ imbalance correction API not implemented in this version");
#endif
}

void block_impl::set_master_clock_rate(double clock_rate)
{
    const auto clock_rates = d_device->getMasterClockRates();

    if (!value_in_ranges(clock_rates, clock_rate)) {
        std::string msg = "Unsupported clock rate (";
        msg += std::to_string(clock_rate);
        msg += "). Clock rate must be in the range ";
        msg += ranges_to_string(clock_rates);

        throw std::invalid_argument(msg);
    }
    d_device->setMasterClockRate(clock_rate);
}

double block_impl::get_master_clock_rate() const
{
    return d_device->getMasterClockRate();
}

range_list_t block_impl::get_master_clock_rates() const
{
    return d_device->getMasterClockRates();
}

void block_impl::set_reference_clock_rate(double clock_rate)
{
#ifdef SOAPY_SDR_API_HAS_REF_CLOCK_RATE_API
    const auto clock_rates = d_device->getReferenceClockRates();

    if (!value_in_ranges(clock_rates, clock_rate)) {
        std::string msg = "Unsupported clock rate (";
        msg += std::to_string(clock_rate);
        msg += "). Clock rate must be in the range ";
        msg += ranges_to_string(clock_rates);

        throw std::invalid_argument(msg);
    }
    d_device->setReferenceClockRate(clock_rate);
#else
    (void)clock_rate;
    throw std::runtime_error("Reference clock API not implemented in this version");
#endif
}

double block_impl::get_reference_clock_rate() const
{
#ifdef SOAPY_SDR_API_HAS_REF_CLOCK_RATE_API
    return d_device->getReferenceClockRate();
#else
    throw std::runtime_error("Reference clock API not implemented in this version");
#endif
}

range_list_t block_impl::get_reference_clock_rates() const
{
#ifdef SOAPY_SDR_API_HAS_REF_CLOCK_RATE_API
    return d_device->getReferenceClockRates();
#else
    throw std::runtime_error("Reference clock API not implemented in this version");
#endif
}

std::vector<std::string> block_impl::list_time_sources() const
{
    return d_device->listTimeSources();
}

void block_impl::set_time_source(const std::string& time_source)
{
    const auto time_sources = d_device->listTimeSources();

    if (!vector_contains(time_sources, time_source)) {
        std::string msg = "Invalid time source (" + time_source + ").";
        msg += "Valid time sources: ";
        msg += string_vector_to_string(time_sources);

        throw std::invalid_argument(msg);
    }

    d_device->setTimeSource(time_source);
}

std::string block_impl::get_time_source() const { return d_device->getTimeSource(); }

std::vector<std::string> block_impl::list_clock_sources() const
{
    return d_device->listClockSources();
}

void block_impl::set_clock_source(const std::string& clock_source)
{
    const auto clock_sources = d_device->listClockSources();

    if (!vector_contains(clock_sources, clock_source)) {
        std::string msg = "Invalid clock source (" + clock_source + ").";
        msg += "Valid clock sources: ";
        msg += string_vector_to_string(clock_sources);

        throw std::invalid_argument(msg);
    }

    d_device->setClockSource(clock_source);
}

std::string block_impl::get_clock_source() const { return d_device->getClockSource(); }

bool block_impl::has_hardware_time(const std::string& what) const
{
    return d_device->hasHardwareTime(what);
}

long long block_impl::get_hardware_time(const std::string& what) const
{
    if (!has_hardware_time(what)) {
        std::string msg =
            this->name() + ": device does not support querying hardware time";
        if (!what.empty()) {
            msg += " (";
            msg += what;
            msg += ")";
        }

        throw std::invalid_argument(msg);
    }
    return d_device->getHardwareTime(what);
}

void block_impl::set_hardware_time(long long timeNs, const std::string& what)
{
    if (!has_hardware_time(what)) {
        std::string msg =
            this->name() + ": device does not support querying hardware time";
        if (!what.empty()) {
            msg += " (" + what + ")";
        }

        throw std::invalid_argument(msg);
    }
    d_device->setHardwareTime(timeNs, what);
}

std::vector<std::string> block_impl::list_sensors() const
{
    return d_device->listSensors();
}

arginfo_t block_impl::get_sensor_info(const std::string& key) const
{
    const auto sensors = d_device->listSensors();

    if (vector_contains(sensors, key)) {
        return d_device->getSensorInfo(key);
    } else {
        throw std::invalid_argument("Invalid sensor: " + key);
    }
}

std::string block_impl::read_sensor(const std::string& key) const
{
    const auto sensors = d_device->listSensors();

    if (vector_contains(sensors, key)) {
        return d_device->readSensor(key);
    } else {
        throw std::invalid_argument("Invalid sensor: " + key);
    }
}

std::vector<std::string> block_impl::list_sensors(size_t channel) const
{
    validate_channel(channel);
    return d_device->listSensors(d_direction, channel);
}

arginfo_t block_impl::get_sensor_info(size_t channel, const std::string& key) const
{
    validate_channel(channel);
    const auto sensors = d_device->listSensors(d_direction, channel);

    if (vector_contains(sensors, key)) {
        return d_device->getSensorInfo(d_direction, channel, key);
    } else {
        throw std::invalid_argument("Invalid sensor: " + key);
    }
}

std::string block_impl::read_sensor(size_t channel, const std::string& key) const
{
    validate_channel(channel);
    const auto sensors = d_device->listSensors(d_direction, channel);

    if (vector_contains(sensors, key)) {
        return d_device->readSensor(d_direction, channel, key);
    } else {
        throw std::invalid_argument("Invalid sensor: " + key);
    }
}

std::vector<std::string> block_impl::list_register_interfaces() const
{
    return d_device->listRegisterInterfaces();
}

void block_impl::write_register(const std::string& name, unsigned addr, unsigned value)
{
    const auto register_interfaces = list_register_interfaces();
    if (!vector_contains(register_interfaces, name)) {
        throw std::invalid_argument(this->name() + ": invalid register interface (" +
                                    name + ")");
    }
    d_device->writeRegister(name, addr, value);
}

unsigned block_impl::read_register(const std::string& name, unsigned addr) const
{
    const auto register_interfaces = list_register_interfaces();
    if (!vector_contains(register_interfaces, name)) {
        throw std::invalid_argument(this->name() + ": invalid register interface (" +
                                    name + ")");
    }
    return d_device->readRegister(name, addr);
}

void block_impl::write_registers(const std::string& name,
                                 unsigned addr,
                                 const std::vector<unsigned>& value)
{
    const auto register_interfaces = list_register_interfaces();
    if (!vector_contains(register_interfaces, name)) {
        throw std::invalid_argument(this->name() + ": invalid register interface (" +
                                    name + ")");
    }
    d_device->writeRegisters(name, addr, value);
}

std::vector<unsigned>
block_impl::read_registers(const std::string& name, unsigned addr, size_t length) const
{
    const auto register_interfaces = list_register_interfaces();
    if (!vector_contains(register_interfaces, name)) {
        throw std::invalid_argument(this->name() + ": invalid register interface (" +
                                    name + ")");
    }
    return d_device->readRegisters(name, addr, length);
}

arginfo_list_t block_impl::get_setting_info() const { return d_device->getSettingInfo(); }

void block_impl::write_setting(const std::string& key, const std::string& value)
{
    const auto setting_info = d_device->getSettingInfo();

    if (arg_info_has_key(setting_info, key)) {
        d_device->writeSetting(key, value);
    } else {
        throw std::invalid_argument("Invalid setting: " + key);
    }
}

std::string block_impl::read_setting(const std::string& key) const
{
    const auto setting_info = d_device->getSettingInfo();

    if (arg_info_has_key(setting_info, key)) {
        return d_device->readSetting(key);
    } else {
        throw std::invalid_argument("Invalid setting: " + key);
    }
}

arginfo_list_t block_impl::get_setting_info(size_t channel) const
{
    validate_channel(channel);
    return d_device->getSettingInfo(d_direction, channel);
}

void block_impl::write_setting(size_t channel,
                               const std::string& key,
                               const std::string& value)
{
    validate_channel(channel);
    const auto setting_info = d_device->getSettingInfo(d_direction, channel);

    if (arg_info_has_key(setting_info, key)) {
        d_device->writeSetting(d_direction, channel, key, value);
    } else {
        throw std::invalid_argument("Invalid setting: " + key);
    }
}

std::string block_impl::read_setting(size_t channel, const std::string& key) const
{
    validate_channel(channel);
    const auto setting_info = d_device->getSettingInfo(d_direction, channel);

    if (arg_info_has_key(setting_info, key)) {
        return d_device->readSetting(d_direction, channel, key);
    } else {
        throw std::invalid_argument("Invalid setting: " + key);
    }
}

std::vector<std::string> block_impl::list_gpio_banks() const
{
    return d_device->listGPIOBanks();
}

void block_impl::write_gpio(const std::string& bank, unsigned value)
{
    const auto gpio_banks = list_gpio_banks();
    if (!vector_contains(gpio_banks, bank)) {
        throw std::invalid_argument(this->name() + ": invalid GPIO bank (" + bank + ")");
    }
    d_device->writeGPIO(bank, value);
}

void block_impl::write_gpio(const std::string& bank, unsigned value, unsigned mask)
{
    const auto gpio_banks = list_gpio_banks();
    if (!vector_contains(gpio_banks, bank)) {
        throw std::invalid_argument(this->name() + ": invalid GPIO bank (" + bank + ")");
    }
    d_device->writeGPIO(bank, value, mask);
}

unsigned block_impl::read_gpio(const std::string& bank) const
{
    const auto gpio_banks = list_gpio_banks();

    if (!vector_contains(gpio_banks, bank)) {
        throw std::invalid_argument(this->name() + ": invalid GPIO bank (" + bank + ")");
    }
    return d_device->readGPIO(bank);
}

void block_impl::write_gpio_dir(const std::string& bank, unsigned dir)
{
    const auto gpio_banks = list_gpio_banks();
    if (!vector_contains(gpio_banks, bank)) {
        throw std::invalid_argument(this->name() + ": invalid GPIO bank (" + bank + ")");
    }
    d_device->writeGPIODir(bank, dir);
}

void block_impl::write_gpio_dir(const std::string& bank, unsigned dir, unsigned mask)
{
    const auto gpio_banks = list_gpio_banks();
    if (!vector_contains(gpio_banks, bank)) {
        throw std::invalid_argument(this->name() + ": invalid GPIO bank (" + bank + ")");
    }
    d_device->writeGPIODir(bank, dir, mask);
}

unsigned block_impl::read_gpio_dir(const std::string& bank) const
{
    const auto gpio_banks = list_gpio_banks();

    if (!vector_contains(gpio_banks, bank)) {
        throw std::invalid_argument(this->name() + ": invalid GPIO bank (" + bank + ")");
    }
    return d_device->readGPIODir(bank);
}

void block_impl::write_i2c(int addr, const std::string& data)
{
    d_device->writeI2C(addr, data);
}

std::string block_impl::read_i2c(int addr, size_t num_bytes)
{
    return d_device->readI2C(addr, num_bytes);
}

unsigned block_impl::transact_spi(int addr, unsigned data, size_t num_bits)
{
    return d_device->transactSPI(addr, data, num_bits);
}

std::vector<std::string> block_impl::list_uarts() const { return d_device->listUARTs(); }

void block_impl::write_uart(const std::string& which, const std::string& data)
{
    const auto uarts = list_uarts();

    if (!vector_contains(uarts, which)) {
        std::string msg = "Invalid UART (" + which + ").";
        msg += "Valid UARTs: ";
        msg += string_vector_to_string(uarts);

        throw std::invalid_argument(msg);
    }
    d_device->writeUART(which, data);
}

std::string block_impl::read_uart(const std::string& which, long timeout_us) const
{
    const auto uarts = list_uarts();

    if (!vector_contains(uarts, which)) {
        std::string msg = "Invalid UART (" + which + ").";
        msg += "Valid UARTs: ";
        msg += string_vector_to_string(uarts);

        throw std::invalid_argument(msg);
    }
    return d_device->readUART(which, timeout_us);
}

/* End public API implementation */

// void block_impl::cmd_handler_frequency(pmtf::wrap val, size_t channel)
// {
//     if (!(val->is_number() && !val->is_complex())) {
//         GR_LOG_ERROR(_debug_logger, "soapy: freq must be float/int");
//         return;
//     }
//     set_frequency(channel, pmtf::scalar<double>(val));
// }

// void block_impl::cmd_handler_gain(pmtf::wrap val, size_t channel)
// {
//     // For compatibility, accept either a numeric value for setting the
//     // overall gain or a dict with a gain element and element-specific
//     // value.
//     if (!(val->is_number() && !val->is_complex()) && !val->is_dict()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: gain must be float/int or a dict");
//         return;
//     }

//     if (val->is_dict()) {
//         if (!pmt::dict_has_key(val, CMD_GAIN_KEY)) {
//             GR_LOG_ERROR(_debug_logger, "soapy: gain dict must contain key \"gain\"");
//             return;
//         }

//         // Accept no name, falls back to default argument ""
//         const auto name =
//             pmt::symbol_to_string(pmt::dict_ref(val, CMD_NAME_KEY, PMT_EMPTYSTR));
//         const auto gain = pmtf::scalar<double>(pmt::dict_ref(val, CMD_GAIN_KEY, PMT_ZERO));

//         set_gain(channel, name, gain);
//     } else {
//         set_gain(channel, pmtf::scalar<double>(val));
//     }
// }

// void block_impl::cmd_handler_samp_rate(pmtf::wrap val, size_t channel)
// {
//     if (!(val->is_number() && !val->is_complex())) {
//         GR_LOG_ERROR(_debug_logger, "soapy: rate must be float/int");
//         return;
//     }
//     set_sample_rate(channel, pmtf::scalar<double>(val));
// }

// void block_impl::cmd_handler_bw(pmtf::wrap val, size_t channel)
// {
//     if (!(val->is_number() && !val->is_complex())) {
//         GR_LOG_ERROR(_debug_logger, "soapy: bw must be float/int");
//         return;
//     }
//     set_bandwidth(channel, pmtf::scalar<double>(val));
// }

// void block_impl::cmd_handler_antenna(pmtf::wrap val, size_t channel)
// {
//     if (!val->is_symbol()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: ant must be string");
//         return;
//     }
//     set_antenna(channel, pmt::symbol_to_string(val));
// }

// void block_impl::cmd_handler_gain_mode(pmtf::wrap val, size_t channel)
// {
//     if (!val->is_bool()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: gain mode must be bool");
//         return;
//     }
//     set_gain_mode(channel, pmt::to_bool(val));
// }

// void block_impl::cmd_handler_frequency_correction(pmtf::wrap val, size_t channel)
// {
//     if (!(val->is_number() && !val->is_complex())) {
//         GR_LOG_ERROR(_debug_logger, "soapy: frequency correction must be float/int");
//         return;
//     }
//     set_frequency_correction(channel, pmtf::scalar<double>(val));
// }

// void block_impl::cmd_handler_dc_offset_mode(pmtf::wrap val, size_t channel)
// {
//     if (!val->is_bool()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: DC offset mode must be bool");
//         return;
//     }
//     set_dc_offset_mode(channel, pmt::to_bool(val));
// }

// void block_impl::cmd_handler_dc_offset(pmtf::wrap val, size_t channel)
// {
//     if (!val->is_number()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: DC offset must be numeric");
//         return;
//     }
//     set_dc_offset(channel, pmt::to_complex(val));
// }

// void block_impl::cmd_handler_iq_balance(pmtf::wrap val, size_t channel)
// {
//     if (!val->is_number()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: IQ balance must be numeric");
//         return;
//     }
//     set_iq_balance(channel, pmt::to_complex(val));
// }

// void block_impl::cmd_handler_iq_balance_mode(pmtf::wrap val, size_t channel)
// {
//     if (!val->is_bool()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: IQ balance mode must be bool");
//         return;
//     }
//     set_iq_balance_mode(channel, pmt::to_bool(val));
// }

// void block_impl::cmd_handler_master_clock_rate(pmtf::wrap val, size_t)
// {
//     if (!(val->is_number() && !val->is_complex())) {
//         GR_LOG_ERROR(_debug_logger, "soapy: master clock rate must be float/int");
//         return;
//     }
//     set_master_clock_rate(pmtf::scalar<double>(val));
// }

// void block_impl::cmd_handler_reference_clock_rate(pmtf::wrap val, size_t)
// {
//     if (!(val->is_number() && !val->is_complex())) {
//         GR_LOG_ERROR(_debug_logger, "soapy: reference clock rate must be float/int");
//         return;
//     }
//     set_reference_clock_rate(pmtf::scalar<double>(val));
// }

// void block_impl::cmd_handler_clock_source(pmtf::wrap val, size_t)
// {
//     if (!val->is_symbol()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: clock source must be string");
//         return;
//     }
//     set_clock_source(pmt::symbol_to_string(val));
// }

// void block_impl::cmd_handler_time_source(pmtf::wrap val, size_t)
// {
//     if (!val->is_symbol()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: time source must be string");
//         return;
//     }
//     set_time_source(pmt::symbol_to_string(val));
// }

// void block_impl::cmd_handler_hardware_time(pmtf::wrap val, size_t)
// {
//     if (!val->is_dict()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: hardware time must be a dict");
//         return;
//     }

//     if (!pmt::dict_has_key(val, CMD_TIME_KEY)) {
//         GR_LOG_ERROR(_debug_logger,
//                      "soapy: hardware time dict must contain key \"time\"");
//         return;
//     }

//     // Accept no name, falls back to default argument ""
//     const auto name =
//         pmt::symbol_to_string(pmt::dict_ref(val, CMD_NAME_KEY, PMT_EMPTYSTR));
//     const auto time = pmt::to_long(pmt::dict_ref(val, CMD_TIME_KEY, PMT_ZERO));

//     set_hardware_time(static_cast<long long>(time), name);
// }

// void block_impl::cmd_handler_register(pmtf::wrap val, size_t)
// {
//     if (!val->is_dict()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: register write param must be a dict");
//         return;
//     }

//     if (!pmt::dict_has_key(val, CMD_NAME_KEY) || !pmt::dict_has_key(val, CMD_ADDR_KEY) ||
//         !pmt::dict_has_key(val, CMD_VALUE_KEY)) {
//         GR_LOG_ERROR(
//             _debug_logger,
//             "soapy: register write dict must contain keys \"name\", \"addr\", \"value\"");
//         return;
//     }

//     const auto name =
//         pmt::symbol_to_string(pmt::dict_ref(val, CMD_NAME_KEY, PMT_EMPTYSTR));
//     const auto addr = pmt::to_uint64(pmt::dict_ref(val, CMD_ADDR_KEY, PMT_ZERO));
//     const auto value = pmt::to_uint64(pmt::dict_ref(val, CMD_VALUE_KEY, PMT_ZERO));

//     write_register(name, static_cast<unsigned>(addr), static_cast<unsigned>(value));
// }

// void block_impl::cmd_handler_registers(pmtf::wrap val, size_t)
// {
//     if (!val->is_dict()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: multi-register write param must be a dict");
//         return;
//     }

//     if (!pmt::dict_has_key(val, CMD_NAME_KEY) || !pmt::dict_has_key(val, CMD_ADDR_KEY) ||
//         !pmt::dict_has_key(val, CMD_VALUE_KEY)) {
//         GR_LOG_ERROR(_debug_logger,
//                      "soapy: multi-register write dict must contain keys \"name\", "
//                      "\"addr\", \"value\"");
//         return;
//     }

//     const auto name =
//         pmt::symbol_to_string(pmt::dict_ref(val, CMD_NAME_KEY, PMT_EMPTYSTR));
//     const auto addr = pmt::to_uint64(pmt::dict_ref(val, CMD_ADDR_KEY, PMT_ZERO));
//     const auto value = pmt::u32vector_elements(
//         pmt::dict_ref(val, CMD_VALUE_KEY, pmt::make_u32vector(0, 0)));

//     write_registers(name, static_cast<unsigned>(addr), value);
// }

// // TODO: any way to call channel-less version?
// void block_impl::cmd_handler_setting(pmtf::wrap val, size_t channel)
// {
//     if (!val->is_dict()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: GPIO must be a dict");
//         return;
//     }

//     if (!pmt::dict_has_key(val, CMD_KEY_KEY) || !pmt::dict_has_key(val, CMD_VALUE_KEY)) {
//         GR_LOG_ERROR(_debug_logger, "soapy: GPIO must contain keys \"key\", \"value\"");
//         return;
//     }

//     const auto key = pmt::symbol_to_string(pmt::dict_ref(val, CMD_KEY_KEY, PMT_EMPTYSTR));
//     const auto value_pmt = pmt::dict_ref(val, CMD_VALUE_KEY, PMT_EMPTYSTR);

//     std::string value;
//     if (pmt::is_bool(val)) {
//         write_setting(channel, key, setting_to_string(pmt::to_bool(value_pmt)));
//     } else if (pmt::is_number(val)) {
//         write_setting(channel, key, setting_to_string(pmtf::scalar<double>(value_pmt)));
//     } else {
//         write_setting(channel, key, pmt::symbol_to_string(value_pmt));
//     }
// }

// void block_impl::cmd_handler_gpio(pmtf::wrap val, size_t)
// {
//     if (!val->is_dict()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: setting must be a dict");
//         return;
//     }

//     if (!pmt::dict_has_key(val, CMD_BANK_KEY) || !pmt::dict_has_key(val, CMD_VALUE_KEY)) {
//         GR_LOG_ERROR(_debug_logger, "soapy: GPIO must contain keys \"bank\", \"value\"");
//         return;
//     }

//     const auto bank =
//         pmt::symbol_to_string(pmt::dict_ref(val, CMD_BANK_KEY, PMT_EMPTYSTR));
//     const auto value = pmt::to_uint64(pmt::dict_ref(val, CMD_VALUE_KEY, PMT_ZERO));

//     if (pmt::dict_has_key(val, CMD_MASK_KEY)) {
//         const auto mask = pmt::to_uint64(pmt::dict_ref(val, CMD_MASK_KEY, PMT_ZERO));

//         write_gpio(bank, static_cast<unsigned>(value), static_cast<unsigned>(mask));
//     } else {
//         write_gpio(bank, static_cast<unsigned>(value));
//     }
// }

// void block_impl::cmd_handler_gpio_dir(pmtf::wrap val, size_t)
// {
//     static const pmtf::wrap CMD_DIR_KEY = pmtf::string("dir");

//     if (!val->is_dict()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: GPIO dir must be a dict");
//         return;
//     }

//     if (!pmt::dict_has_key(val, CMD_BANK_KEY) || !pmt::dict_has_key(val, CMD_DIR_KEY)) {
//         GR_LOG_ERROR(_debug_logger,
//                      "soapy: GPIO dir must contain keys \"bank\", \"dir\"");
//         return;
//     }

//     const auto bank =
//         pmt::symbol_to_string(pmt::dict_ref(val, CMD_BANK_KEY, PMT_EMPTYSTR));
//     const auto dir = pmt::to_uint64(pmt::dict_ref(val, CMD_DIR_KEY, PMT_ZERO));

//     if (pmt::dict_has_key(val, CMD_MASK_KEY)) {
//         const auto mask = pmt::to_uint64(pmt::dict_ref(val, CMD_MASK_KEY, PMT_ZERO));

//         write_gpio_dir(bank, static_cast<unsigned>(dir), static_cast<unsigned>(mask));
//     } else {
//         write_gpio_dir(bank, static_cast<unsigned>(dir));
//     }
// }

// void block_impl::cmd_handler_i2c(pmtf::wrap val, size_t)
// {
//     if (!val->is_dict()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: I2C must be a dict");
//         return;
//     }

//     if (!pmt::dict_has_key(val, CMD_ADDR_KEY) || !pmt::dict_has_key(val, CMD_DATA_KEY)) {
//         GR_LOG_ERROR(_debug_logger, "soapy: I2C must contain keys \"addr\", \"data\"");
//         return;
//     }

//     const auto addr = pmt::to_long(pmt::dict_ref(val, CMD_ADDR_KEY, PMT_ZERO));
//     const auto data =
//         pmt::symbol_to_string(pmt::dict_ref(val, CMD_DATA_KEY, PMT_EMPTYSTR));

//     write_i2c(static_cast<int>(addr), data);
// }

// void block_impl::cmd_handler_uart(pmtf::wrap val, size_t)
// {
//     if (!val->is_dict()) {
//         GR_LOG_ERROR(_debug_logger, "soapy: UART must be a dict");
//         return;
//     }

//     if (!pmt::dict_has_key(val, CMD_NAME_KEY) || !pmt::dict_has_key(val, CMD_DATA_KEY)) {
//         GR_LOG_ERROR(_debug_logger, "soapy: UART must contain keys \"name\", \"data\"");
//         return;
//     }

//     const auto name =
//         pmt::symbol_to_string(pmt::dict_ref(val, CMD_NAME_KEY, PMT_EMPTYSTR));
//     const auto data =
//         pmt::symbol_to_string(pmt::dict_ref(val, CMD_DATA_KEY, PMT_EMPTYSTR));

//     write_uart(name, data);
// }

// void block_impl::msg_handler_cmd(pmtf::wrap msg)
// {
//     const long CHANNEL_ALL = 0x12345678; // arbitrary

//     if (!pmt::is_dict(msg)) {
//         GR_LOG_ERROR(_debug_logger, "soapy: commands must be pmt::dict");
//         return;
//     }

//     // Channel not specified means apply to all
//     const pmtf::wrap channel_all_pmt = pmt::from_long(CHANNEL_ALL);
//     size_t channel = pmt::to_long(pmt::dict_ref(msg, CMD_CHAN_KEY, channel_all_pmt));

//     if (channel != CHANNEL_ALL && channel >= d_nchan) {
//         GR_LOG_ERROR(_debug_logger,
//                      "soapy: ignoring command for invalid channel {}",
//                          channel);
//         return;
//     }

//     for (size_t i = 0; i < pmt::length(msg); i++) {
//         const pmtf::wrap item = pmt::nth(i, msg);
//         const pmtf::wrap key = pmt::car(item);
//         const pmtf::wrap val = pmt::cdr(item);
//         if (key == CMD_CHAN_KEY) {
//             continue;
//         }

//         // Find command handler
//         auto it = d_cmd_handlers.find(key);
//         if (it == d_cmd_handlers.end()) {
//             GR_LOG_ERROR(_debug_logger,
//                          "soapy: ignoring unknown command key '{}'",
//                              pmt::symbol_to_string(key));
//             continue;
//         }
//         cmd_handler_t handler = it->second;

//         if (channel != CHANNEL_ALL) {
//             std::lock_guard<std::mutex> l(d_device_mutex);
//             handler(val, channel);
//         } else {
//             for (size_t c = 0; c < d_nchan; c++) {
//                 std::lock_guard<std::mutex> l(d_device_mutex);
//                 handler(val, c);
//             }
//         }
//     }
// }
} /* namespace soapy */
} /* namespace gr */
