#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"

/// crop creates a new Event Stream file with only events from the given region.
template <sepia::type event_stream_type>
void crop(sepia::header header, const pontella::command& command) {
    const uint16_t left = std::stoull(command.arguments[2]);
    const uint16_t bottom = std::stoull(command.arguments[3]);
    const uint16_t width = std::stoull(command.arguments[4]);
    const uint16_t height = std::stoull(command.arguments[5]);
    const uint16_t right = left + width;
    const uint16_t top = bottom + height;

    if (command.arguments[6] == "true") {
    sepia::write<event_stream_type> write(
        sepia::filename_to_ofstream(command.arguments[1]), header.width, header.height);
    sepia::join_observable<event_stream_type>(
        sepia::filename_to_ifstream(command.arguments[0]), [&](sepia::event<event_stream_type> event) {
            if (event.x >= left && event.x < right && event.y >= bottom && event.y < top) {
                write(event);
            }
        });
    } else if (command.arguments[6] == "false") {
    sepia::write<event_stream_type> write(
        sepia::filename_to_ofstream(command.arguments[1]), width, height);
    sepia::join_observable<event_stream_type>(
        sepia::filename_to_ifstream(command.arguments[0]), [&](sepia::event<event_stream_type> event) {
            if (event.x >= left && event.x < right && event.y >= bottom && event.y < top) {
                event.x -= left;
                event.y -= bottom;
                write(event);
            }
        });
    } else {
        throw std::runtime_error("Please specify if keeps offset (true) or not (false)");
    }
}

int main(int argc, char* argv[]) {
    return pontella::main(
        {
            "crop generates a new Event Stream file with only events from the given region.",
            "Syntax: ./crop [options] /path/to/input.es /path/to/output.es left bottom width height offset",
            "Available options:",
            "    -h, --help    shows this help message",
        },
        argc,
        argv,
        7,
        {},
        {},
        [](pontella::command command) {
            if (command.arguments[0] == command.arguments[1]) {
                throw std::runtime_error("The Event Stream input and output must be different files");
            }
            const auto header = sepia::read_header(sepia::filename_to_ifstream(command.arguments[0]));
            if (std::stoull(command.arguments[2]) + std::stoull(command.arguments[4]) > header.width
                || std::stoull(command.arguments[3]) + std::stoull(command.arguments[5]) > header.height) {
                throw std::runtime_error("The selected region is out of scope");
            }
            switch (header.event_stream_type) {
                case sepia::type::generic: {
                    throw std::runtime_error("Unsupported event type: generic");
                    break;
                }
                case sepia::type::dvs: {
                    crop<sepia::type::dvs>(header, command);
                    break;
                }
                case sepia::type::atis: {
                    crop<sepia::type::atis>(header, command);
                    break;
                }
                case sepia::type::color: {
                    crop<sepia::type::color>(header, command);
                    break;
                }
            }
        });
    return 0;
}
