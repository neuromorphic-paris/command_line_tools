#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"

/// crop creates a new Event Stream file with only events from the given region.
template <sepia::type event_stream_type>
void crop(
    const sepia::header& header,
    std::unique_ptr<std::istream> input_stream,
    std::unique_ptr<std::ostream> output_stream,
    uint16_t left,
    uint16_t bottom,
    uint16_t width,
    uint16_t height,
    bool preserve_offset) {
    const auto right = left + width;
    const auto top = bottom + height;
    if (preserve_offset) {
        sepia::write<event_stream_type> write(std::move(output_stream), header.width, header.height);
        sepia::join_observable<event_stream_type>(std::move(input_stream), [&](sepia::event<event_stream_type> event) {
            if (event.x >= left && event.x < right && event.y >= bottom && event.y < top) {
                write(event);
            }
        });
    } else {
        sepia::write<event_stream_type> write(std::move(output_stream), width, height);
        sepia::join_observable<event_stream_type>(std::move(input_stream), [&](sepia::event<event_stream_type> event) {
            if (event.x >= left && event.x < right && event.y >= bottom && event.y < top) {
                event.x -= left;
                event.y -= bottom;
                write(event);
            }
        });
    }
}

int main(int argc, char* argv[]) {
    return pontella::main(
        {
            "crop generates a new Event Stream file with only events from the given region.",
            "Syntax: ./crop [options] /path/to/input.es /path/to/output.es left bottom width height",
            "Available options:",
            "    -p, --preserve-offset    prevents the coordinates of the cropped area from being normalized",
            "    -h, --help               shows this help message",
        },
        argc,
        argv,
        6,
        {},
        {{"preserve-offset", {"p"}}},
        [](pontella::command command) {
            if (command.arguments[0] == command.arguments[1]) {
                throw std::runtime_error("The Event Stream input and output must be different files");
            }
            const auto header = sepia::read_header(sepia::filename_to_ifstream(command.arguments[0]));
            const auto left = std::stoull(command.arguments[2]);
            const auto bottom = std::stoull(command.arguments[3]);
            const auto width = std::stoull(command.arguments[4]);
            const auto height = std::stoull(command.arguments[5]);
            if (left + width > header.width || bottom + height > header.height) {
                throw std::runtime_error("The selected region is out of scope");
            }
            const auto preserve_offset = command.flags.find("preserve-offset") != command.flags.end();
            auto input_stream = sepia::filename_to_ifstream(command.arguments[0]);
            auto output_stream = sepia::filename_to_ofstream(command.arguments[1]);
            switch (header.event_stream_type) {
                case sepia::type::generic: {
                    throw std::runtime_error("Unsupported event type: generic");
                    break;
                }
                case sepia::type::dvs: {
                    crop<sepia::type::dvs>(
                        header,
                        std::move(input_stream),
                        std::move(output_stream),
                        left,
                        bottom,
                        width,
                        height,
                        preserve_offset);
                    break;
                }
                case sepia::type::atis: {
                    crop<sepia::type::atis>(
                        header,
                        std::move(input_stream),
                        std::move(output_stream),
                        left,
                        bottom,
                        width,
                        height,
                        preserve_offset);
                    break;
                }
                case sepia::type::color: {
                    crop<sepia::type::color>(
                        header,
                        std::move(input_stream),
                        std::move(output_stream),
                        left,
                        bottom,
                        width,
                        height,
                        preserve_offset);
                    break;
                }
            }
        });
    return 0;
}
