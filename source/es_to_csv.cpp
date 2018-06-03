#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"

int main(int argc, char* argv[]) {
    return pontella::main(
        {"es_to_csv converts an Event Stream file into a csv file (compatible with Excel and Matlab)\n"
         "Syntax: ./es_to_csv [options] /path/to/input.es /path/to/output.csv\n",
         "Available options:",
         "    -h, --help    shows this help message"},
        argc,
        argv,
        2,
        {},
        {},
        [](pontella::command command) {
            const auto header = sepia::read_header(sepia::filename_to_ifstream(command.arguments[0]));
            auto input = sepia::filename_to_ifstream(command.arguments[0]);
            auto output = sepia::filename_to_ofstream(command.arguments[1]);
            switch (header.event_stream_type) {
                case sepia::type::generic:
                    *output << "t,bytes\n";
                    sepia::join_observable<sepia::type::generic>(
                        std::move(input), [&](sepia::generic_event generic_event) {
                            *output << generic_event.t << ",";
                            *output << std::hex;
                            for (std::size_t index = 0; index < generic_event.bytes.size(); ++index) {
                                *output << static_cast<uint32_t>(generic_event.bytes[index]);
                                if (index == generic_event.bytes.size() - 1) {
                                    *output << "\n";
                                } else {
                                    *output << " ";
                                }
                            }
                            *output << std::dec;
                        });
                    break;
                case sepia::type::dvs:
                    *output << "t,x,y,is_increase\n";
                    sepia::join_observable<sepia::type::dvs>(std::move(input), [&](sepia::dvs_event dvs_event) {
                        *output << dvs_event.t << "," << dvs_event.x << "," << dvs_event.y << ","
                                << dvs_event.is_increase << "\n";
                    });
                    break;
                case sepia::type::atis:
                    *output << "t,x,y,is_threshold_crossing,polarity\n";
                    sepia::join_observable<sepia::type::atis>(std::move(input), [&](sepia::atis_event atis_event) {
                        *output << atis_event.t << "," << atis_event.x << "," << atis_event.y << ","
                                << atis_event.is_threshold_crossing << "," << atis_event.polarity << "\n";
                    });
                    break;
                case sepia::type::color:
                    *output << "t,x,y,r,g,b\n";
                    sepia::join_observable<sepia::type::color>(std::move(input), [&](sepia::color_event color_event) {
                        *output << color_event.t << "," << color_event.x << "," << color_event.y << ","
                                << static_cast<uint32_t>(color_event.r) << "," << static_cast<uint32_t>(color_event.g)
                                << "," << static_cast<uint32_t>(color_event.b) << "\n";
                    });
                    break;
            }
        });
}
