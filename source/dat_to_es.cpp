#include "../third_party/pontella/source/pontella.hpp"
#include "dat.hpp"

int main(int argc, char* argv[]) {
    return pontella::main(
        {"dat_to_es converts a td file and an aps file into an Event Stream file",
         "Syntax: ./dat_to_es [options] /path/to/input_td.dat /path/to/input_aps.dat /path/to/output.es",
         "    If the string 'none' (without quotes) is used for the td (respectively, aps) file,",
         "    the Event Stream file is build from the aps (respectively, td) file only",
         "Available options:",
         "    -h, --help    shows this help message"},
        argc,
        argv,
        3,
        {},
        {},
        [](pontella::command command) {
            if (command.arguments[0] == command.arguments[1]) {
                throw std::runtime_error("The td and aps inputs must be different files, and cannot be both none");
            }
            if (command.arguments[0] != "none" && command.arguments[0].find("td") == std::string::npos) {
                std::cout << "The file " << command.arguments[0]
                          << " does not have 'td' in its name. Do you want to continue anyway? (Y/n)" << '\n';
                std::string answer;
                std::getline(std::cin, answer);
                if (answer == "n") {
                    throw std::runtime_error("Aborting...");
                } else {
                    std::cout << "Continuing..." << '\n';
                }
            }
            if (command.arguments[0] == command.arguments[2]) {
                throw std::runtime_error("The td input and the Event Stream output must be different files");
            }
            if (command.arguments[1] == command.arguments[2]) {
                throw std::runtime_error("The aps input and the Event Stream output must be different files");
            }
            if (command.arguments[0] == "none" && command.arguments[1] == "none") {
                throw std::runtime_error("none cannot be used for both the td file and aps file");
            }

            if (command.arguments[1] == "none") {
                auto stream = sepia::filename_to_ifstream(command.arguments[0]);
                const auto header = dat::read_header(*stream);
                dat::td_observable(
                    *stream,
                    header,
                    sepia::write<sepia::type::dvs>(
                        sepia::filename_to_ofstream(command.arguments[2]), header.width, header.height));
            } else if (command.arguments[0] == "none") {
                auto stream = sepia::filename_to_ifstream(command.arguments[1]);
                const auto header = dat::read_header(*stream);
                dat::aps_observable(
                    *stream,
                    header,
                    sepia::write<sepia::type::atis>(
                        sepia::filename_to_ofstream(command.arguments[2]), header.width, header.height));
            } else {
                auto td_stream = sepia::filename_to_ifstream(command.arguments[0]);
                auto aps_stream = sepia::filename_to_ifstream(command.arguments[1]);
                const auto header = dat::read_header(*td_stream);
                {
                    const auto aps_header = dat::read_header(*aps_stream);
                    if (header.version != aps_header.version || header.width != aps_header.width
                        || header.height != aps_header.height) {
                        throw std::runtime_error("the td and aps file have incompatible headers");
                    }
                }
                dat::td_aps_observable(
                    *td_stream,
                    *aps_stream,
                    header,
                    sepia::write<sepia::type::atis>(
                        sepia::filename_to_ofstream(command.arguments[2]), header.width, header.height));
            }
        });
}
