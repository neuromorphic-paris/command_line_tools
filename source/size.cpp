#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"

int main(int argc, char* argv[]) {
    return pontella::main(
        {
            "size prints the spatial dimensions of the given Event Stream file.",
            "Syntax: ./size [options] /path/to/input.es",
            "Available options:",
            "    -h, --help    shows this help message",
        },
        argc,
        argv,
        1,
        {},
        {},
        [](pontella::command command) {
            const auto header = sepia::read_header(sepia::filename_to_ifstream(command.arguments[0]));
            switch (header.event_stream_type) {
                case sepia::type::generic:
                    throw std::runtime_error("generic events do not have spatial dimensions");
                default:
                    std::cout << header.width << "x" << header.height;
                    std::cout.flush();
            }
        });
    return 0;
}
