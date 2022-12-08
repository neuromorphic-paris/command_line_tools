#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"
#include "timecode.hpp"

int main(int argc, char* argv[]) {
    return pontella::main(
        {"es_to_ply converts an Event Stream file to a PLY file (Polygon File Format, compatible with Blender)\n"
         "Syntax: ./es_to_ply [options] /path/to/input.es /path/to/output_on.ply /path/to/output_off.ply\n",
         "Available options:",
         "    -t timestamp, --timestamp timestamp    sets the initial timestamp for the point cloud (timecode)",
         "                                               defaults to 00:00:00",
         "    -d duration, --duration duration       sets the duration for the point cloud (timecode)",
         "                                               defaults to 00:00:01",
         "    -h, --help                             shows this help message"},
        argc,
        argv,
        3,
        {
            {"timestamp", {"t"}},
            {"duration", {"d"}},
        },
        {},
        [](pontella::command command) {
            const auto header = sepia::read_header(sepia::filename_to_ifstream(command.arguments[0]));
            uint64_t begin_t = 0;
            {
                const auto name_and_argument = command.options.find("timestamp");
                if (name_and_argument != command.options.end()) {
                    begin_t = timecode(name_and_argument->second).value();
                }
            }
            auto target_end_t = std::numeric_limits<uint64_t>::max();
            {
                const auto name_and_argument = command.options.find("duration");
                if (name_and_argument != command.options.end()) {
                    const auto duration = timecode(name_and_argument->second).value();
                    target_end_t = begin_t + duration;
                }
            }
            auto input = sepia::filename_to_ifstream(command.arguments[0]);
            std::size_t on_count = 0;
            std::size_t off_count = 0;
            auto end_t = target_end_t;
            switch (header.event_stream_type) {
                case sepia::type::generic:
                    throw std::runtime_error("generic events are not compatible with this application");
                    break;
                case sepia::type::dvs:
                    sepia::join_observable<sepia::type::dvs>(std::move(input), [&](sepia::dvs_event dvs_event) {
                        if (dvs_event.t >= begin_t) {
                            if (dvs_event.t >= target_end_t) {
                                throw sepia::end_of_file();
                            }
                            if (dvs_event.is_increase) {
                                ++on_count;
                            } else {
                                ++off_count;
                            }
                            if (target_end_t == std::numeric_limits<uint64_t>::max()) {
                                end_t = dvs_event.t + 1;
                            }
                        }
                    });
                    break;
                case sepia::type::atis:
                    sepia::join_observable<sepia::type::atis>(std::move(input), [&](sepia::atis_event atis_event) {
                        if (!atis_event.is_threshold_crossing && atis_event.t >= begin_t) {
                            if (atis_event.t >= end_t) {
                                return;
                            }
                            if (atis_event.polarity) {
                                ++on_count;
                            } else {
                                ++off_count;
                            }
                            if (target_end_t == std::numeric_limits<uint64_t>::max()) {
                                end_t = atis_event.t + 1;
                            }
                        }
                    });
                    break;
                case sepia::type::color:
                    sepia::join_observable<sepia::type::color>(std::move(input), [&](sepia::color_event color_event) {
                        if (color_event.t >= begin_t) {
                            if (color_event.t >= end_t) {
                                return;
                            }
                            if (color_event.r + color_event.b + color_event.g >= 128 * 3) {
                                ++on_count;
                            } else {
                                ++off_count;
                            }
                            if (target_end_t == std::numeric_limits<uint64_t>::max()) {
                                end_t = color_event.t + 1;
                            }
                        }
                    });
                    break;
            }
            if (on_count == 0 && off_count == 0) {
                throw std::runtime_error("there are no DVS events in the given file and range");
            }
            auto output_on = sepia::filename_to_ofstream(command.arguments[1]);
            auto output_off = sepia::filename_to_ofstream(command.arguments[2]);
            *output_on << "ply\nformat binary_little_endian 1.0\nelement vertex " << on_count
                       << "\nproperty double x\nproperty double y\nproperty double z\nend_header\n";
            *output_off << "ply\nformat binary_little_endian 1.0\nelement vertex " << off_count
                        << "\nproperty double x\nproperty double y\nproperty double z\nend_header\n";
            input = sepia::filename_to_ifstream(command.arguments[0]);
            const auto spatial_scale = 1.0 / static_cast<double>(std::max(header.width, header.height));
            const auto temporal_scale = begin_t == end_t ? 1.0 : 1.0 / (end_t - begin_t);
            switch (header.event_stream_type) {
                case sepia::type::generic:
                    throw std::runtime_error("generic events are not compatible with this application");
                    break;
                case sepia::type::dvs:
                    sepia::join_observable<sepia::type::dvs>(std::move(input), [&](sepia::dvs_event dvs_event) {
                        if (dvs_event.t >= begin_t) {
                            if (dvs_event.t >= end_t) {
                                return;
                            }
                            std::array<double, 3> vertex{
                                (dvs_event.x - header.width / 2.0) * spatial_scale,
                                (dvs_event.y - header.height / 2.0) * spatial_scale,
                                (dvs_event.t - begin_t) * temporal_scale,
                            };
                            if (dvs_event.is_increase) {
                                output_on->write(
                                    reinterpret_cast<char*>(vertex.data()), vertex.size() * sizeof(double));
                            } else {
                                output_off->write(
                                    reinterpret_cast<char*>(vertex.data()), vertex.size() * sizeof(double));
                            }
                        }
                    });
                    break;
                case sepia::type::atis:
                    sepia::join_observable<sepia::type::atis>(std::move(input), [&](sepia::atis_event atis_event) {
                        if (!atis_event.is_threshold_crossing && atis_event.t >= begin_t) {
                            if (atis_event.t >= end_t) {
                                return;
                            }
                            std::array<double, 3> vertex{
                                (atis_event.x - header.width / 2.0) * spatial_scale,
                                (atis_event.y - header.height / 2.0) * spatial_scale,
                                (atis_event.t - begin_t) * temporal_scale,
                            };
                            if (atis_event.polarity) {
                                output_on->write(
                                    reinterpret_cast<char*>(vertex.data()), vertex.size() * sizeof(double));
                            } else {
                                output_off->write(
                                    reinterpret_cast<char*>(vertex.data()), vertex.size() * sizeof(double));
                            }
                        }
                    });
                    break;
                case sepia::type::color:
                    sepia::join_observable<sepia::type::color>(std::move(input), [&](sepia::color_event color_event) {
                        if (color_event.t >= begin_t) {
                            if (color_event.t >= end_t) {
                                return;
                            }
                            std::array<double, 3> vertex{
                                (color_event.x - header.width / 2.0) * spatial_scale,
                                (color_event.y - header.height / 2.0) * spatial_scale,
                                (color_event.t - begin_t) * temporal_scale,
                            };
                            if (color_event.r + color_event.b + color_event.g >= 128 * 3) {
                                output_on->write(
                                    reinterpret_cast<char*>(vertex.data()), vertex.size() * sizeof(double));
                            } else {
                                output_off->write(
                                    reinterpret_cast<char*>(vertex.data()), vertex.size() * sizeof(double));
                            }
                        }
                    });
                    break;
            }
        });
}
