#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"
#include <iomanip>
#include <sstream>

template <sepia::type type>
std::pair<uint64_t, uint64_t> t_range(std::unique_ptr<std::istream> stream) {
    uint64_t first_t = std::numeric_limits<uint64_t>::max();
    uint64_t last_t = 0;
    sepia::join_observable<type>(std::move(stream), [&](sepia::event<type> event) {
        if (first_t == std::numeric_limits<uint64_t>::max()) {
            first_t = event.t;
        }
        last_t = event.t;
    });
    return {first_t, last_t};
}

int main(int argc, char* argv[]) {
    return pontella::main(
        {"es_to_ppms converts an Event Stream file to Netpbm (https://en.wikipedia.org/wiki/Netpbm) frames.",
        "The output directory must be created first",
         "Syntax: ./es_to_ppms [options] /path/to/input.es /path/to/output/directory",
         "Available options:",
         "    -d decay, --decay decay                sets the timesurface decay (in microseconds)",
         "                                               defaults to 100000",
         "    -f frametime, --frametime frametime    sets the time between two frames (in microseconds)",
         "                                               defaults to 10000",
         "    -h, --help                 shows this help message"},
        argc,
        argv,
        2,
        {
            {"decay", {"d"}},
            {"frametime", {"f"}},
        },
        {},
        [](pontella::command command) {
            uint64_t decay = 100000;
            {
                const auto name_and_argument = command.options.find("decay");
                if (name_and_argument != command.options.end()) {
                    decay = std::stoull(name_and_argument->second);
                    if (decay == 0) {
                        throw std::runtime_error("the timesurface decay must be larger than 0");
                    }
                }
            }
            uint64_t frametime = 10000;
            {
                const auto name_and_argument = command.options.find("frametime");
                if (name_and_argument != command.options.end()) {
                    frametime = std::stoull(name_and_argument->second);
                    if (frametime == 0) {
                        throw std::runtime_error("the frametime must be larger than 0");
                    }
                }
            }
            const auto header = sepia::read_header(sepia::filename_to_ifstream(command.arguments[0]));
            auto input = sepia::filename_to_ifstream(command.arguments[0]);
            std::pair<uint64_t, uint64_t> range{std::numeric_limits<uint64_t>::max(), 0};
            auto get_range = [&](sepia::dvs_event event) {
                if (std::get<0>(range) == std::numeric_limits<uint64_t>::max()) {
                    std::get<0>(range) = event.t;
                }
                std::get<1>(range) = event.t;
            };
            switch (header.event_stream_type) {
                case sepia::type::generic:
                    throw std::runtime_error("unsupported event stream type 'generic'");
                case sepia::type::dvs:
                    sepia::join_observable<sepia::type::dvs>(std::move(input), get_range);
                    break;
                case sepia::type::atis:
                    sepia::make_split<sepia::type::atis>(get_range, [](sepia::threshold_crossing) {});
                    break;
                case sepia::type::color:
                    throw std::runtime_error("unsupported event stream type 'color'");
            }
            uint8_t name_width;
            {
                const auto number_of_frames = (range.second - range.first) / frametime;
                if (number_of_frames == 0) {
                    throw std::runtime_error("the frametime is too large, or there are not enough events: no frames will be generated");
                }
                name_width = static_cast<uint8_t>(std::log10(number_of_frames - 1)) + 1;
            }
            std::vector<std::pair<uint64_t, bool>> ts_and_ons(header.width * header.height, {std::numeric_limits<uint64_t>::max(), false});
            uint64_t frame_index = 0;
            auto generate_ppms = [&](sepia::dvs_event event) {
                const auto frame_t = std::get<0>(range) + frame_index * frametime;
                if (event.t >= frame_t) {
                    std::vector<uint8_t> frame(header.width * header.height * 3);
                    for (uint16_t y = 0; y < header.height; ++y) {
                        for (uint16_t x = 0; x < header.width; ++x) {
                            const auto& t_and_on = ts_and_ons[x + y * header.width];
                            uint8_t value;
                            if (t_and_on.first == std::numeric_limits<uint64_t>::max()) {
                                value = 127;
                            } else {
                                if (t_and_on.second) {
                                    value = static_cast<uint8_t>(127 + 128 * std::exp(-static_cast<double>(frame_t - 1 - t_and_on.first) / decay));
                                } else {
                                    value = static_cast<uint8_t>(127 - 127 * std::exp(-static_cast<double>(frame_t - 1 - t_and_on.first) / decay));
                                }
                            }
                            frame[(x + (header.height - 1 - y) * header.width) * 3 + 0] = value;
                            frame[(x + (header.height - 1 - y) * header.width) * 3 + 1] = value;
                            frame[(x + (header.height - 1 - y) * header.width) * 3 + 2] = value;
                        }
                    }
                    std::stringstream name;
                    name << std::setfill('0') << std::setw(name_width) << frame_index << ".ppm";
                    ++frame_index;
                    std::ofstream output(sepia::join({command.arguments[1], name.str()}));
                    if (!output.good()) {
                        throw sepia::unwritable_file(name.str());
                    }
                    output << "P6\n" << header.width << " " << header.height << "\n255\n";
                    output.write(reinterpret_cast<const char*>(frame.data()), frame.size());
                }
                ts_and_ons[event.x + event.y * header.width].first = event.t;
                ts_and_ons[event.x + event.y * header.width].second = event.is_increase;
            };
            input = sepia::filename_to_ifstream(command.arguments[0]);
            switch (header.event_stream_type) {
                case sepia::type::generic:
                    throw std::runtime_error("unsupported event stream type 'generic'");
                case sepia::type::dvs:
                    sepia::join_observable<sepia::type::dvs>(std::move(input), generate_ppms);
                    break;
                case sepia::type::atis:
                    sepia::join_observable<sepia::type::atis>(
                        std::move(input),
                        sepia::make_split<sepia::type::atis>(generate_ppms, [](sepia::threshold_crossing) {}));
                    break;
                case sepia::type::color:
                    throw std::runtime_error("unsupported event stream type 'color'");
            }
        });
}
