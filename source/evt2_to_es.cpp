#include "../third_party/pontella/source/pontella.hpp"
#include "evt.hpp"

int main(int argc, char* argv[]) {
    return pontella::main(
        {"evt2_to_es converts a raw file (EVT2) into an Event Stream file",
         "Syntax: ./evt3_to_es [options] /path/to/input.raw /path/to/output.es",
         "Available options:",
         "    -x size, --width size     sets the sensor width in pixels if not specified in the header",
         "                                  defaults to 640",
         "    -y size, --height size    sets the sensor height in pixels if not specified in the header",
         "                                  defaults to 480",
         "    -n, --normalize           offsets the timestamps so that the first one is zero",
         "    -h, --help                shows this help message"},
        argc,
        argv,
        2,
        {
            {"width", {"x"}},
            {"height", {"y"}},
        },
        {
            {"normalize", {"n"}},
        },
        [](pontella::command command) {
            if (command.arguments[0] == command.arguments[1]) {
                throw std::runtime_error("The raw input and the Event Stream output must be different files");
            }
            if (command.arguments[0] == "none" && command.arguments[1] == "none") {
                throw std::runtime_error("none cannot be used for both the td file and aps file");
            }
            evt::header default_header{640, 480};
            {
                const auto name_and_argument = command.options.find("width");
                if (name_and_argument != command.options.end()) {
                    default_header.width = static_cast<uint16_t>(std::stoull(name_and_argument->second));
                }
            }
            {
                const auto name_and_argument = command.options.find("height");
                if (name_and_argument != command.options.end()) {
                    default_header.height = static_cast<uint16_t>(std::stoull(name_and_argument->second));
                }
            }
            auto stream = sepia::filename_to_ifstream(command.arguments[0]);
            const auto header = evt::read_header(*stream, std::move(default_header));
            evt::observable_2(
                *stream,
                header,
                command.flags.find("normalize") != command.flags.end(),
                sepia::write<sepia::type::dvs>(
                    sepia::filename_to_ofstream(command.arguments[1]), header.width, header.height));
        });
}
