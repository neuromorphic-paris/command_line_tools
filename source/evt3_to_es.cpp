#include "../third_party/pontella/source/pontella.hpp"
#include "evt3.hpp"

int main(int argc, char* argv[]) {
    return pontella::main(
        {"evt3_to_es converts a raw file (EVT3) into an Event Stream file",
         "Syntax: ./evt3_to_es [options] /path/to/input.raw /path/to/output.es",
         "Available options:",
         "    -h, --help    shows this help message"},
        argc,
        argv,
        2,
        {},
        {},
        [](pontella::command command) {
            if (command.arguments[0] == command.arguments[1]) {
                throw std::runtime_error("The raw input and the Event Stream output must be different files");
            }
            if (command.arguments[0] == "none" && command.arguments[1] == "none") {
                throw std::runtime_error("none cannot be used for both the td file and aps file");
            }

            auto stream = sepia::filename_to_ifstream(command.arguments[0]);
            const auto header = evt3::read_header(*stream);
            evt3::observable(
                *stream,
                header,
                sepia::write<sepia::type::dvs>(
                    sepia::filename_to_ofstream(command.arguments[1]), header.width, header.height));
        });
}
