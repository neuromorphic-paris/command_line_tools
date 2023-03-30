#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"
#include "timecode.hpp"
#include <iostream>
#include <sstream>

constexpr uint32_t from_bytes(char b0, char b1, char b2, char b3) {
    return static_cast<uint32_t>(b0) | (static_cast<uint32_t>(b1) << 8) | (static_cast<uint32_t>(b2) << 16)
           | (static_cast<uint32_t>(b3) << 24);
}

static void write_le_uint32(std::ostream& stream, uint32_t value) {
    std::array<uint8_t, 4> bytes{
        static_cast<uint8_t>(value & 0xff),
        static_cast<uint8_t>((value >> 8) & 0xff),
        static_cast<uint8_t>((value >> 16) & 0xff),
        static_cast<uint8_t>((value >> 24) & 0xff),
    };
    stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

enum class output_mode {
    carriage_return,
    line_feed,
    quiet,
};

/// synth converts a visual event stream to sound.
template <sepia::type event_stream_type>
void synth(
    const sepia::header& header,
    std::unique_ptr<std::istream> input_stream,
    std::unique_ptr<std::ostream> output_stream,
    uint64_t begin_t,
    uint64_t end_t,
    float amplitude_gain,
    float minimum_frequency,
    float maximum_frequency,
    float tracker_lambda,
    uint32_t sampling_rate,
    float playback_speed,
    uint64_t activity_tau,
    output_mode mode) {
    for (const auto header_value : std::initializer_list<uint32_t>{
             from_bytes('R', 'I', 'F', 'F'),
             0, // chunk data size
             from_bytes('W', 'A', 'V', 'E'),
             from_bytes('f', 'm', 't', ' '),
             16,                      // format chunk size
             from_bytes(1, 0, 2, 0),  // audio format and channels
             sampling_rate,
             sampling_rate * 4,       // bytes per second
             from_bytes(4, 0, 16, 0), //  block align and bits per sample
             from_bytes('d', 'a', 't', 'a'),
             0,                       // chunk data size
         }) {
        write_le_uint32(*output_stream.get(), header_value);
    }
    const auto inverse_tau = 1.0f / static_cast<float>(activity_tau);
    const auto frequency_a = static_cast<double>(minimum_frequency) / static_cast<double>(sampling_rate) * 2.0 * M_PI;
    const auto frequency_b =
        (std::log(maximum_frequency) - std::log(minimum_frequency)) / static_cast<float>(header.height - 1);
    uint32_t sample = 0;
    uint64_t sample_t = 0;
    auto gain = amplitude_gain * static_cast<float>(std::numeric_limits<int16_t>::max());
    std::vector<std::pair<uint64_t, float>> ts_and_activities(
        header.height, {std::numeric_limits<uint64_t>::max(), 0.0f});
    std::vector<float> row_trackers(header.height, static_cast<float>(header.width) / 2.0f);
    const auto sample_factor = static_cast<double>(playback_speed) / static_cast<double>(sampling_rate) * 1e6;
    const auto output_modulo =
        std::min(1u, static_cast<uint32_t>(std::round(static_cast<double>(sampling_rate) / 100.0)));
    sepia::join_observable<event_stream_type>(std::move(input_stream), [&](sepia::event<event_stream_type> event) {
        if (event.t >= begin_t) {
            if (event.t >= end_t) {
                throw sepia::end_of_file();
            }
            event.t -= begin_t;
            while (event.t > sample_t) {
                if (sample % output_modulo == 0) {
                    switch (mode) {
                        case output_mode::carriage_return:
                        case output_mode::line_feed: {
                            std::stringstream message;
                            if (mode == output_mode::carriage_return) {
                                message << "\r";
                            }
                            message << "synth sample " << sample << " (" << timecode(sample_t).to_timecode_string()
                                    << ")";
                            if (mode == output_mode::line_feed) {
                                message << "\n";
                            }
                            std::cout << message.str();
                            std::cout.flush();
                        } break;
                        case output_mode::quiet:
                            break;
                    }
                }
                auto left = 0.0;
                auto right = 0.0;
                for (uint16_t y = 0; y < header.height; ++y) {
                    const auto signal =
                        ts_and_activities[y].first == std::numeric_limits<uint64_t>::max() ?
                            0.0f :
                            (ts_and_activities[y].second
                             * std::exp(-static_cast<float>(sample_t - ts_and_activities[y].first) * inverse_tau)
                             / static_cast<float>(header.width))
                                * gain
                                * std::sin(
                                    static_cast<float>(static_cast<double>(sample) * frequency_a)
                                    * std::exp(frequency_b * static_cast<float>(y)));
                    const auto balance = row_trackers[y] / static_cast<float>(header.width - 1);
                    left += static_cast<double>((1.0f - balance) * signal);
                    right += static_cast<double>(balance * signal);
                }
                int16_t left_sample = 0;
                if (left < std::numeric_limits<int16_t>::min()) {
                    left_sample = std::numeric_limits<int16_t>::min();
                } else if (left > std::numeric_limits<int16_t>::max()) {
                    left_sample = std::numeric_limits<int16_t>::max();
                } else {
                    left_sample = static_cast<int16_t>(std::round(left));
                }
                int16_t right_sample = 0;
                if (right < std::numeric_limits<int16_t>::min()) {
                    right_sample = std::numeric_limits<int16_t>::min();
                } else if (right > std::numeric_limits<int16_t>::max()) {
                    right_sample = std::numeric_limits<int16_t>::max();
                } else {
                    right_sample = static_cast<int16_t>(std::round(right));
                }
                std::array<uint8_t, 4> bytes{
                    static_cast<uint8_t>(*reinterpret_cast<uint16_t*>(&left_sample) & 0xff),
                    static_cast<uint8_t>((*reinterpret_cast<uint16_t*>(&left_sample) >> 8) & 0xff),
                    static_cast<uint8_t>(*reinterpret_cast<uint16_t*>(&right_sample) & 0xff),
                    static_cast<uint8_t>((*reinterpret_cast<uint16_t*>(&right_sample) >> 8) & 0xff),
                };
                output_stream->write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                ++sample;
                sample_t = static_cast<uint64_t>(std::round(static_cast<double>(sample) * sample_factor));
            }
            auto& t_and_activity = ts_and_activities[event.y];
            if (t_and_activity.first == std::numeric_limits<uint64_t>::max()) {
                t_and_activity.second = 1.0f;
            } else {
                t_and_activity.second =
                    t_and_activity.second * std::exp(-static_cast<float>(event.t - t_and_activity.first) * inverse_tau)
                    + 1.0f;
            }
            t_and_activity.first = event.t;
            row_trackers[event.y] =
                (1.0f - tracker_lambda) * row_trackers[event.y] + tracker_lambda * static_cast<float>(event.x);
        }
    });
    if (mode == output_mode::carriage_return) {
        std::cout << std::endl;
    }
    output_stream->seekp(4, std::ostream::beg);
    write_le_uint32(*output_stream.get(), sample * 4 + 36);
    output_stream->seekp(40, std::ostream::beg);
    write_le_uint32(*output_stream.get(), sample * 4);
}

int main(int argc, char* argv[]) {
    return pontella::main(
        {
            "synth converts events into sound (.wav)",
            "Syntax: ./synth [options] /path/to/input.es /path/to/output.wav",
            "Available options:",
            "    -b timecode, --begin timecode              ignore events before this timestamp (timecode)",
            "                                                   defaults to 00:00:00",
            "    -e timecode, --end timecode                ignore events after this timestamp (timecode)",
            "                                                   defaults to the end of the recording",
            "    -a float, --amplitude-gain float           activity to sound amplitude conversion factor",
            "                                                   defaults to 0.1",
            "    -i float, --minimum-frequency float        minimum frequency (bottom row) in Hertz",
            "                                                   defaults to 27.5",
            "    -j float, --maximum-frequency float        maximum frequency (top row) in Hertz",
            "                                                   defaults to 4186.009",
            "    -s integer, --sampling-rate integer        sampling rate in Hertz",
            "                                                   defaults to 44100",
            "    -l float, --tracker-lambda float           row tracker moving mean parameter",
            "                                                   defaults to 0.1",
            "    -p float, --playback-speed float           relative playback speed",
            "                                                   defaults to 1.0 (real-time)",
            "    -t integer, --activity-tau integer         row decay in µs",
            "                                                   defaults to 10000",
            "    -o integer, --output-mode integer          0 prints progress without new lines",
            "                                                   1 prints progress with new lines",
            "                                                   2 is quiet",
            "                                                   defaults to 0",
            "    -h, --help                                 show this help message",
        },
        argc,
        argv,
        2,
        {
            {"begin", {"b"}},
            {"end", {"e"}},
            {"amplitude-gain", {"a"}},
            {"minimum-frequency", {"i"}},
            {"maximum-frequency", {"j"}},
            {"sampling-rate", {"s"}},
            {"tracker-lambda", {"l"}},
            {"playback-speed", {"p"}},
            {"activity-tau", {"t"}},
            {"output-mode", {"o"}},
        },
        {},
        [](pontella::command command) {
            if (command.arguments[0] == command.arguments[1]) {
                throw std::runtime_error("input and output must be different files");
            }
            uint64_t begin_t = 0;
            {
                const auto name_and_argument = command.options.find("begin");
                if (name_and_argument != command.options.end()) {
                    begin_t = timecode(name_and_argument->second).value();
                }
            }
            auto end_t = std::numeric_limits<uint64_t>::max();
            {
                const auto name_and_argument = command.options.find("end");
                if (name_and_argument != command.options.end()) {
                    end_t = timecode(name_and_argument->second).value();
                    if (end_t <= begin_t) {
                        throw std::runtime_error("end must be strictly larger than begin");
                    }
                }
            }
            auto amplitude_gain = 0.1f;
            {
                const auto name_and_argument = command.options.find("amplitude-gain");
                if (name_and_argument != command.options.end()) {
                    amplitude_gain = std::stof(name_and_argument->second);
                    if (amplitude_gain <= 0.0f) {
                        throw std::runtime_error("amplitude-gain must be larger than zero");
                    }
                }
            }
            auto minimum_frequency = 27.5f;
            {
                const auto name_and_argument = command.options.find("minimum-frequency");
                if (name_and_argument != command.options.end()) {
                    minimum_frequency = std::stof(name_and_argument->second);
                    if (minimum_frequency <= 0.0f) {
                        throw std::runtime_error("minimum-frequency must be larger than zero");
                    }
                }
            }
            auto maximum_frequency = 4186.009f;
            {
                const auto name_and_argument = command.options.find("maximum-frequency");
                if (name_and_argument != command.options.end()) {
                    maximum_frequency = std::stof(name_and_argument->second);
                    if (maximum_frequency <= 0.0f) {
                        throw std::runtime_error("maximum-frequency must be larger than zero");
                    }
                    if (minimum_frequency >= maximum_frequency) {
                        throw std::runtime_error("minimum-frequency must be smaller than maximum-frequency");
                    }
                }
            }
            uint32_t sampling_rate = 44100;
            {
                const auto name_and_argument = command.options.find("sampling-rate");
                if (name_and_argument != command.options.end()) {
                    sampling_rate = static_cast<uint32_t>(std::stoul(name_and_argument->second));
                    if (sampling_rate == 0) {
                        throw std::runtime_error("sampling-rate must be larger than zero");
                    }
                }
            }
            auto tracker_lambda = 0.01f;
            {
                const auto name_and_argument = command.options.find("tracker-lambda");
                if (name_and_argument != command.options.end()) {
                    tracker_lambda = std::stof(name_and_argument->second);
                    if (tracker_lambda < 0.0f || tracker_lambda > 1.0f) {
                        throw std::runtime_error("tracker-lambda must be in the range [0, 1]");
                    }
                }
            }
            auto playback_speed = 1.0f;
            {
                const auto name_and_argument = command.options.find("playback-speed");
                if (name_and_argument != command.options.end()) {
                    playback_speed = std::stof(name_and_argument->second);
                    if (playback_speed <= 0.0f) {
                        throw std::runtime_error("playack-speed must be larger than zero");
                    }
                }
            }
            uint64_t activity_tau = 10000;
            {
                const auto name_and_argument = command.options.find("activity-tau");
                if (name_and_argument != command.options.end()) {
                    activity_tau = timecode(name_and_argument->second).value();
                    if (activity_tau == 0) {
                        throw std::runtime_error("activity-tau must be larger than 0");
                    }
                }
            }
            auto mode = output_mode::carriage_return;
            {
                const auto name_and_argument = command.options.find("output-mode");
                if (name_and_argument != command.options.end()) {
                    switch (std::stoul(name_and_argument->second)) {
                        case 0:
                            break;
                        case 1:
                            mode = output_mode::line_feed;
                            break;
                        case 2:
                            mode = output_mode::quiet;
                            break;
                        default:
                            throw std::runtime_error("output-mode must 0, 1, or 2");
                            break;
                    }
                }
            }

            const auto header = sepia::read_header(sepia::filename_to_ifstream(command.arguments[0]));
            auto input_stream = sepia::filename_to_ifstream(command.arguments[0]);
            auto output_stream = sepia::filename_to_ofstream(command.arguments[1]);
            switch (header.event_stream_type) {
                case sepia::type::generic: {
                    throw std::runtime_error("Unsupported event type: generic");
                    break;
                }
                case sepia::type::dvs: {
                    synth<sepia::type::dvs>(
                        header,
                        std::move(input_stream),
                        std::move(output_stream),
                        begin_t,
                        end_t,
                        amplitude_gain,
                        minimum_frequency,
                        maximum_frequency,
                        tracker_lambda,
                        sampling_rate,
                        playback_speed,
                        activity_tau,
                        mode);
                    break;
                }
                case sepia::type::atis: {
                    synth<sepia::type::atis>(
                        header,
                        std::move(input_stream),
                        std::move(output_stream),
                        begin_t,
                        end_t,
                        amplitude_gain,
                        minimum_frequency,
                        maximum_frequency,
                        tracker_lambda,
                        sampling_rate,
                        playback_speed,
                        activity_tau,
                        mode);
                    break;
                }
                case sepia::type::color: {
                    throw std::runtime_error("Unsupported event type: color");
                    break;
                }
            }
        });
    return 0;
}
