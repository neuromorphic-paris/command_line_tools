#include "../third_party/lodepng/lodepng.h"
#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"
#include "../third_party/tarsier/source/stitch.hpp"
#include "html.hpp"
#include <numeric>

/// exposure_measurement represents an exposure measurement as a time delta.
SEPIA_PACK(struct exposure_measurement {
    uint64_t t;
    uint64_t delta_t;
    uint16_t x;
    uint16_t y;
});

/// filename_to_string reads the contents of a file to a string.
std::string filename_to_string(const std::string& filename) {
    auto stream = sepia::filename_to_ifstream(filename);
    return std::string(std::istreambuf_iterator<char>(*stream), std::istreambuf_iterator<char>());
}

int main(int argc, char* argv[]) {
    return pontella::main(
        {"rainmaker generates a standalone HTML file containing a 3D representation of events",
         "Syntax: ./rainmaker [options] /path/to/input.es /path/to/output.html",
         "Available options:",
         "    -t [timestamp], --timestamp [timestamp]    sets the initial timestamp for the point cloud",
         "                                                   default to 0",
         "    -d [duration], --duration [duration]       sets the duration (in microseconds) for the point cloud",
         "                                                   defaults to 1000000",
         "    -r [ratio], --ratio [ratio]                sets the discard ratio for logarithmic tone mapping",
         "                                                   defaults to 0.05",
         "                                                   ignored if the file does not contain ATIS events",
         "    -f [duration], --frametime [duration]      sets the frame duration",
         "                                                   defaults to 'auto'",
         "                                                   'auto' calculates the time between two frames so that",
         "                                                   there is the same amount of raw data in events",
         "                                                   and frames,",
         "                                                   a duration in microseconds can be provided instead,",
         "                                                   'none' disables the frames,",
         "                                                   ignored if the file contains DVS events",
         "    -h, --help                                 shows this help message"},
        argc,
        argv,
        2,
        {
            {"timestamp", {"t"}},
            {"duration", {"d"}},
            {"ratio", {"r"}},
            {"frametime", {"f"}},
        },
        {},
        [](pontella::command command) {
            const auto nodes = html::parse(filename_to_string(sepia::join({SEPIA_DIRNAME, "rainmaker.html"})));
            uint64_t begin_t = 0;
            {
                const auto name_and_argument = command.options.find("timestamp");
                if (name_and_argument != command.options.end()) {
                    begin_t = std::stoull(name_and_argument->second);
                }
            }
            auto end_t = begin_t + 1000000;
            {
                const auto name_and_argument = command.options.find("duration");
                if (name_and_argument != command.options.end()) {
                    const auto duration = std::stoull(name_and_argument->second);
                    end_t = begin_t + duration;
                }
            }
            {
                std::ofstream output(command.arguments[1]);
                if (!output.good()) {
                    throw sepia::unwritable_file(command.arguments[1]);
                }
            }
            std::vector<sepia::color_event> color_events;
            const auto header = sepia::read_header(sepia::filename_to_ifstream(command.arguments[0]));
            std::vector<uint8_t> base_frame(header.width * header.height * 4, 0);
            switch (header.event_stream_type) {
                case sepia::type::generic: {
                    throw std::runtime_error("generic events are not compatible with this application");
                    break;
                }
                case sepia::type::dvs: {
                    sepia::join_observable<sepia::type::dvs>(
                        sepia::filename_to_ifstream(command.arguments[0]), [&](sepia::dvs_event dvs_event) {
                            if (dvs_event.t >= end_t) {
                                throw sepia::end_of_file();
                            }
                            if (dvs_event.t >= begin_t) {
                                if (dvs_event.is_increase) {
                                    color_events.push_back({dvs_event.t, dvs_event.x, dvs_event.y, 0x00, 0x8c, 0xff});
                                } else {
                                    color_events.push_back({dvs_event.t, dvs_event.x, dvs_event.y, 0x33, 0x4d, 0x5c});
                                }
                            }
                        });
                    if (color_events.empty()) {
                        throw std::runtime_error("there are no DVS events in the given file and range");
                    }
                    break;
                }
                case sepia::type::atis: {
                    auto ratio = 0.05;
                    {
                        const auto name_and_argument = command.options.find("ratio");
                        if (name_and_argument != command.options.end()) {
                            ratio = std::stod(name_and_argument->second);
                        }
                        if (ratio < 0 || ratio >= 1) {
                            throw std::runtime_error("[ratio] must be a real number in the range [0, 1[");
                        }
                    }
                    std::vector<exposure_measurement> exposure_measurements;
                    std::vector<uint64_t> delta_t_base_frame(header.width * header.height, 0);
                    sepia::join_observable<sepia::type::atis>(
                        sepia::filename_to_ifstream(command.arguments[0]),
                        sepia::make_split<sepia::type::atis>(
                            [](sepia::dvs_event) {},
                            tarsier::make_stitch<sepia::threshold_crossing, exposure_measurement>(
                                header.width,
                                header.height,
                                [](sepia::threshold_crossing threshold_crossing,
                                   uint64_t delta_t) -> exposure_measurement {
                                    return {threshold_crossing.t, delta_t, threshold_crossing.x, threshold_crossing.y};
                                },
                                [&](exposure_measurement exposure_measurement) {
                                    if (exposure_measurement.t >= end_t) {
                                        throw sepia::end_of_file();
                                    } else if (exposure_measurement.t >= begin_t) {
                                        exposure_measurements.push_back(exposure_measurement);
                                    } else {
                                        delta_t_base_frame
                                            [exposure_measurement.x
                                             + header.width * (header.height - 1 - exposure_measurement.y)] =
                                                exposure_measurement.delta_t;
                                    }
                                })));
                    if (exposure_measurements.empty()) {
                        throw std::runtime_error("there are no ATIS events in the given file and range");
                    }
                    auto slope = 0.0;
                    auto intercept = 128.0;
                    {
                        std::vector<uint64_t> delta_ts;
                        delta_ts.resize(exposure_measurements.size());
                        std::transform(
                            exposure_measurements.begin(),
                            exposure_measurements.end(),
                            delta_ts.begin(),
                            [](exposure_measurement exposure_measurement) { return exposure_measurement.delta_t; });

                        // @DEBUG {
                        {
                            std::ofstream debug("/Users/Alex/idv/libraries/utilities/build/debug/delta_ts.log");
                            for (auto delta_t : delta_ts) {
                                debug << delta_t << "\n";
                            }
                        }
                        // }

                        delta_ts.reserve(delta_ts.size() + delta_t_base_frame.size());
                        for (auto delta_t : delta_t_base_frame) {
                            if (delta_t > 0 && delta_t < std::numeric_limits<uint64_t>::max()) {
                                delta_ts.push_back(delta_t);
                            }
                        }
                        auto discarded_delta_ts =
                            std::vector<uint64_t>(static_cast<std::size_t>(delta_ts.size() * ratio));
                        std::partial_sort_copy(
                            delta_ts.begin(), delta_ts.end(), discarded_delta_ts.begin(), discarded_delta_ts.end());
                        auto white_discard = discarded_delta_ts.back();
                        const auto white_discard_fallback = discarded_delta_ts.front();
                        std::partial_sort_copy(
                            delta_ts.begin(),
                            delta_ts.end(),
                            discarded_delta_ts.begin(),
                            discarded_delta_ts.end(),
                            std::greater<uint64_t>());
                        auto black_discard = discarded_delta_ts.back();
                        const auto black_discard_fallback = discarded_delta_ts.front();
                        if (black_discard <= white_discard) {
                            white_discard = white_discard_fallback;
                            black_discard = black_discard_fallback;
                        }
                        if (black_discard > white_discard) {
                            const auto delta =
                                std::log(static_cast<double>(black_discard) / static_cast<double>(white_discard));
                            slope = -255.0 / delta;
                            intercept = 255.0 * std::log(static_cast<double>(black_discard)) / delta;
                        }
                    }
                    auto delta_t_to_exposure = [&](uint64_t delta_t) -> uint8_t {
                        const auto exposure_candidate = slope * std::log(delta_t) + intercept;
                        return static_cast<uint8_t>(
                            exposure_candidate > 255 ? 255 : (exposure_candidate < 0 ? 0 : exposure_candidate));
                    };
                    for (auto delta_t_iterator = delta_t_base_frame.begin();
                         delta_t_iterator != delta_t_base_frame.end();
                         ++delta_t_iterator) {
                        const auto exposure = delta_t_to_exposure(*delta_t_iterator);
                        const std::size_t index = std::distance(delta_t_base_frame.begin(), delta_t_iterator) * 4;
                        base_frame[index] = exposure;
                        base_frame[index + 1] = exposure;
                        base_frame[index + 2] = exposure;
                        base_frame[index + 3] = 255;
                    }
                    color_events.resize(exposure_measurements.size());
                    std::transform(
                        exposure_measurements.begin(),
                        exposure_measurements.end(),
                        color_events.begin(),
                        [&](exposure_measurement exposure_measurement) -> sepia::color_event {
                            const auto exposure = delta_t_to_exposure(exposure_measurement.delta_t);
                            return {exposure_measurement.t,
                                    exposure_measurement.x,
                                    exposure_measurement.y,
                                    exposure,
                                    exposure,
                                    exposure};
                        });
                    break;
                }
                case sepia::type::color: {
                    sepia::join_observable<sepia::type::color>(
                        sepia::filename_to_ifstream(command.arguments[0]), [&](sepia::color_event color_event) {
                            if (color_event.t >= end_t) {
                                throw sepia::end_of_file();
                            }
                            if (color_event.t >= begin_t) {
                                color_events.push_back(color_event);
                            } else {
                                const auto index =
                                    (color_event.x + header.width * (header.height - 1 - color_event.y)) * 4;
                                base_frame[index] = color_event.r;
                                base_frame[index + 1] = color_event.g;
                                base_frame[index + 2] = color_event.b;
                                base_frame[index + 3] = 255;
                            }
                        });
                    if (color_events.empty()) {
                        throw std::runtime_error("there are no color events in the given file and range");
                    }
                    break;
                }
            }

            // retrieve the frametime
            uint64_t frametime = 0;
            if (header.event_stream_type != sepia::type::dvs) {
                const auto name_and_argument = command.options.find("frametime");
                if (name_and_argument == command.options.end() || name_and_argument->second == "auto") {
                    frametime = (end_t - begin_t) / (color_events.size() / (header.height * header.width));
                    if (frametime == 0) {
                        frametime = 1;
                    }
                } else if (name_and_argument != command.options.end() && name_and_argument->second != "none") {
                    frametime = std::stoull(name_and_argument->second);
                }
            }

            // generate the frames
            std::vector<std::vector<uint8_t>> frames;
            if (frametime > 0) {
                frames.push_back(base_frame);
                for (auto color_event : color_events) {
                    if (color_event.t - begin_t > frametime * (frames.size() - 1)) {
                        if (frametime * frames.size() >= end_t) {
                            break;
                        } else {
                            frames.push_back(frames.back());
                        }
                    }
                    const auto index = (color_event.x + header.width * (header.height - 1 - color_event.y)) * 4;
                    frames.back()[index] = color_event.r;
                    frames.back()[index + 1] = color_event.g;
                    frames.back()[index + 2] = color_event.b;
                    frames.back()[index + 3] = 255;
                }
            }

            // encode the events
            std::vector<uint8_t> events_bytes;
            {
                std::stringstream event_stream;
                sepia::write_to_reference<sepia::type::color> write(event_stream, header.width, header.height);
                for (auto color_event : color_events) {
                    write(color_event);
                }
                sepia::read_header(event_stream);
                const auto event_stream_as_string = event_stream.str();
                events_bytes.assign(
                    std::next(
                        event_stream_as_string.begin(),
                        static_cast<std::iterator_traits<std::vector<uint8_t>::iterator>::difference_type>(
                            event_stream.tellg())),
                    event_stream_as_string.end());
            }

            // encode the base frame
            std::vector<uint8_t> png_bytes;
            {
                if (lodepng::encode(png_bytes, base_frame, header.width, header.height) != 0) {
                    throw std::logic_error("encoding the base frame failed");
                }
            }

            // render the HTML output
            html::render(
                sepia::filename_to_ofstream(command.arguments[1]),
                nodes,
                {
                    {"title", html::variable("rainmaker")},
                    {"x_max",
                     html::variable(std::to_string(
                         header.width > header.height ? 1.0 : static_cast<double>(header.width) / header.height))},
                    {"y_max",
                     html::variable(std::to_string(
                         header.width > header.height ? static_cast<double>(header.height) / header.width : 1.0))},
                    {"z_max", html::variable(std::to_string(1.0))},
                    {"width", html::variable(std::to_string(header.width))},
                    {"height", html::variable(std::to_string(header.height))},
                    {"begin_t", html::variable(std::to_string(begin_t))},
                    {"end_t", html::variable(std::to_string(end_t))},
                    {"has_frames", html::variable(!frames.empty())},
                    {"frametime", html::variable(std::to_string(frametime))},
                    {"base_frame", html::variable(html::bytes_to_encoded_characters(png_bytes))},
                    {"frames",
                     html::variable(std::accumulate(
                         frames.begin(),
                         frames.end(),
                         std::string(),
                         [&](const std::string& frames_as_string, const std::vector<uint8_t>& frame) {
                             std::vector<uint8_t> png_bytes;
                             if (lodepng::encode(png_bytes, frame, header.width, header.height) != 0) {
                                 throw std::logic_error("encoding a PNG frame failed");
                             }
                             return (frames_as_string.empty() ? std::string() : frames_as_string + ", ") + "'"
                                    + html::bytes_to_encoded_characters(png_bytes) + "'";
                         }))},
                    {"events", html::variable(html::bytes_to_encoded_characters(events_bytes))},
                    {"x3dom",
                     html::variable(
                         filename_to_string(sepia::join({sepia::dirname(SEPIA_DIRNAME), "third_party", "x3dom.js"})))},
                });
        });
}
