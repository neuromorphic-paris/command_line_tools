#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"
#include "timecode.hpp"

/// cut creates a new Event Stream file with only events from the given time range.
template <sepia::type event_stream_type>
void cut(sepia::header header, const pontella::command& command) {
    const auto begin_t = timecode(command.arguments[2]).value();
    const auto end_t = timecode(command.arguments[3]).value();
    auto first_t = std::numeric_limits<uint64_t>::max();
    const auto normalize = command.flags.find("normalize") != command.flags.end();
    sepia::write<event_stream_type> write(
        sepia::filename_to_ofstream(command.arguments[1]), header.width, header.height);
    sepia::join_observable<event_stream_type>(
        sepia::filename_to_ifstream(command.arguments[0]), [&](sepia::event<event_stream_type> event) {
            if (event.t < begin_t || event.t >= end_t) {
                return;
            }
            if (first_t == std::numeric_limits<uint64_t>::max()) {
                first_t = event.t;
            }
            if (normalize) {
                event.t -= first_t;
            }
            write(event);
        });
}

int main(int argc, char* argv[]) {
    return pontella::main(
        {
            "cut generates a new Event Stream file with only events from the given time range.",
            "Syntax: ./cut [options] /path/to/input.es /path/to/output.es begin end",
            "Available options:",
            "    -n, --normalize    sets the first generated timestamp to zero",
            "    -h, --help         shows this help message",
        },
        argc,
        argv,
        4,
        {},
        {{"normalize", {"n"}}},
        [](pontella::command command) {
            if (command.arguments[0] == command.arguments[1]) {
                throw std::runtime_error("The Event Stream input and output must be different files");
            }
            const auto header = sepia::read_header(sepia::filename_to_ifstream(command.arguments[0]));
            switch (header.event_stream_type) {
                case sepia::type::generic: {
                    cut<sepia::type::generic>(header, command);
                    break;
                }
                case sepia::type::dvs: {
                    cut<sepia::type::dvs>(header, command);
                    break;
                }
                case sepia::type::atis: {
                    cut<sepia::type::atis>(header, command);
                    break;
                }
                case sepia::type::color: {
                    cut<sepia::type::color>(header, command);
                    break;
                }
            }
        });
    return 0;
}
