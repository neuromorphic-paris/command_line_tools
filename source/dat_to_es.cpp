#include "../third_party/pontella/source/pontella.hpp"
#include "../third_party/sepia/source/sepia.hpp"

/// header bundles a .dat file header's information.
struct header {
    uint8_t version;
    uint16_t width;
    uint16_t height;
};

/// read_header retrieves header information from a .dat file.
header read_header(std::istream& stream) {
    std::vector<std::string> header_lines;
    for (;;) {
        if (stream.peek() != '%' || stream.eof()) {
            break;
        }
        stream.ignore();
        header_lines.emplace_back();
        for (;;) {
            const auto character = stream.get();
            if (stream.eof()) {
                break;
            }
            if (character == '\n') {
                break;
            }
            header_lines.back().push_back(character);
        }
    }
    if (header_lines.empty()
        || std::any_of(header_lines.begin(), header_lines.end(), [](const std::string& header_line) {
               return std::any_of(header_line.begin(), header_line.end(), [](char character) {
                   return !std::isprint(character) && !std::isspace(character);
               });
           })) {
        stream.seekg(0, std::istream::beg);
        return {0, 304, 240};
    }
    stream.ignore(2);
    header header = {};
    for (const auto& header_line : header_lines) {
        std::vector<std::string> words;
        for (auto character : header_line) {
            if (std::isspace(character)) {
                if (!words.empty() && !words.back().empty()) {
                    words.emplace_back();
                }
            } else {
                if (words.empty()) {
                    words.emplace_back();
                }
                words.back().push_back(character);
            }
        }
        if (words.size() > 1) {
            try {
                if (words[0] == "Version") {
                    header.version = static_cast<uint8_t>(stoul(words[1]));
                } else if (words[0] == "Width") {
                    header.width = static_cast<uint16_t>(stoul(words[1]));
                } else if (words[0] == "Height") {
                    header.height = static_cast<uint16_t>(stoul(words[1]));
                }
            } catch (const std::invalid_argument&) {
            } catch (const std::out_of_range&) {
            }
        }
    }
    if (header.version >= 2 && header.width > 0 && header.height > 0) {
        return header;
    }
    return {1, 304, 240};
}

/// bytes_to_dvs_event converts raw byte to a polarized event.
/// The DVS event type is used for both td and aps .dat files.
sepia::dvs_event bytes_to_dvs_event(std::array<uint8_t, 8> bytes, header header) {
    if (header.version < 2) {
        return {
            static_cast<uint64_t>(bytes[0]) | (static_cast<uint64_t>(bytes[1]) << 8)
                | (static_cast<uint64_t>(bytes[2]) << 16) | (static_cast<uint64_t>(bytes[3]) << 24),
            static_cast<uint16_t>(static_cast<uint16_t>(bytes[4]) | (static_cast<uint16_t>(bytes[5] & 1) << 8)),
            static_cast<uint16_t>(
                header.height - 1
                - (static_cast<uint16_t>(bytes[5] >> 1) | (static_cast<uint16_t>(bytes[6] & 1) << 7))),
            (bytes[6] & 0b10) == 0b10,
        };
    }
    return {
        static_cast<uint64_t>(bytes[0]) | (static_cast<uint64_t>(bytes[1]) << 8)
            | (static_cast<uint64_t>(bytes[2]) << 16) | (static_cast<uint64_t>(bytes[3]) << 24),
        static_cast<uint16_t>(static_cast<uint16_t>(bytes[4]) | (static_cast<uint16_t>(bytes[5] & 0b111111) << 8)),
        static_cast<uint16_t>(
            header.height - 1
            - (static_cast<uint16_t>(bytes[5] >> 6) | (static_cast<uint16_t>(bytes[6]) << 2)
               | (static_cast<uint16_t>(bytes[7] & 0b1111) << 10))),
        (bytes[7] & 0b10000) == 0b10000,
    };
}

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
	    if (command.arguments[0].find("td") == std::string::npos) {
	    	std::cout << "The file " << command.arguments[0] << " does not have 'td' in its name. Do you want to continue anyway? (Y/n)" << '\n';
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
            if (command.arguments[0] == "none") {
                auto stream = sepia::filename_to_ifstream(command.arguments[1]);
                const auto header = read_header(*stream);
                sepia::write<sepia::type::atis> write(
                    sepia::filename_to_ofstream(command.arguments[2]), header.width, header.height);
                uint64_t previous_t = 0;
                for (;;) {
                    std::array<uint8_t, 8> bytes;
                    stream->read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                    if (stream->eof()) {
                        break;
                    }
                    const auto dvs_event = bytes_to_dvs_event(bytes, header);
                    if (dvs_event.t >= previous_t && dvs_event.x < header.width && dvs_event.y < header.height) {
                        write({dvs_event.t, dvs_event.x, dvs_event.y, true, dvs_event.is_increase});
                        previous_t = dvs_event.t;
                    }
                }
            } else if (command.arguments[1] == "none") {
                auto stream = sepia::filename_to_ifstream(command.arguments[0]);
                const auto header = read_header(*stream);
                sepia::write<sepia::type::dvs> write(
                    sepia::filename_to_ofstream(command.arguments[2]), header.width, header.height);
                uint64_t previous_t = 0;
                for (;;) {
                    std::array<uint8_t, 8> bytes;
                    stream->read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                    if (stream->eof()) {
                        break;
                    }
                    const auto dvs_event = bytes_to_dvs_event(bytes, header);
                    if (dvs_event.t >= previous_t && dvs_event.x < header.width && dvs_event.y < header.height) {
                        write(dvs_event);
                        previous_t = dvs_event.t;
                    }
                }
            } else {
                auto td_stream = sepia::filename_to_ifstream(command.arguments[0]);
                auto aps_stream = sepia::filename_to_ifstream(command.arguments[1]);
                const auto header = read_header(*td_stream);
                {
                    const auto aps_header = read_header(*aps_stream);
                    if (header.version != aps_header.version || header.width != aps_header.width
                        || header.height != aps_header.height) {
                        throw std::runtime_error("the td and aps file have incompatible headers");
                    }
                }
                sepia::write<sepia::type::atis> write(
                    sepia::filename_to_ofstream(command.arguments[2]), header.width, header.height);
                sepia::dvs_event td_event;
                sepia::dvs_event aps_event;
                uint64_t previous_t = 0;
                for (;;) {
                    std::array<uint8_t, 8> bytes;
                    td_stream->read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                    if (td_stream->eof()) {
                        td_stream.reset();
                        break;
                    } else {
                        td_event = bytes_to_dvs_event(bytes, header);
                        if (td_event.x < header.width && td_event.y < header.height) {
                            break;
                        }
                    }
                }
                for (;;) {
                    std::array<uint8_t, 8> bytes;
                    aps_stream->read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                    if (aps_stream->eof()) {
                        aps_stream.reset();
                        break;
                    } else {
                        aps_event = bytes_to_dvs_event(bytes, header);
                        if (aps_event.x < header.width && aps_event.y < header.height) {
                            break;
                        }
                    }
                }
                while (td_stream && aps_stream) {
                    if (td_event.t <= aps_event.t) {
                        write({td_event.t, td_event.x, td_event.y, false, td_event.is_increase});
                        previous_t = td_event.t;
                        for (;;) {
                            std::array<uint8_t, 8> bytes;
                            td_stream->read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                            if (td_stream->eof()) {
                                td_stream.reset();
                                break;
                            }
                            td_event = bytes_to_dvs_event(bytes, header);
                            if (td_event.t >= previous_t && td_event.x < header.width && td_event.y < header.height) {
                                break;
                            }
                        }
                    } else {
                        write({aps_event.t, aps_event.x, aps_event.y, true, aps_event.is_increase});
                        previous_t = aps_event.t;
                        for (;;) {
                            std::array<uint8_t, 8> bytes;
                            aps_stream->read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                            if (aps_stream->eof()) {
                                aps_stream.reset();
                                break;
                            }
                            aps_event = bytes_to_dvs_event(bytes, header);
                            if (aps_event.t >= previous_t && aps_event.x < header.width
                                && aps_event.y < header.height) {
                                break;
                            }
                        }
                    }
                }
                if (td_stream) {
                    write({td_event.t, td_event.x, td_event.y, false, td_event.is_increase});
                    for (;;) {
                        std::array<uint8_t, 8> bytes;
                        td_stream->read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                        if (td_stream->eof()) {
                            break;
                        }
                        td_event = bytes_to_dvs_event(bytes, header);
                        if (td_event.t >= previous_t && td_event.x < header.width && td_event.y < header.height) {
                            write({td_event.t, td_event.x, td_event.y, false, td_event.is_increase});
                            previous_t = td_event.t;
                        }
                    }
                } else {
                    write({aps_event.t, aps_event.x, aps_event.y, false, aps_event.is_increase});
                    for (;;) {
                        std::array<uint8_t, 8> bytes;
                        aps_stream->read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                        if (aps_stream->eof()) {
                            break;
                        }
                        aps_event = bytes_to_dvs_event(bytes, header);
                        if (aps_event.t >= previous_t && aps_event.x < header.width && aps_event.y < header.height) {
                            write({aps_event.t, aps_event.x, aps_event.y, false, aps_event.is_increase});
                            previous_t = aps_event.t;
                        }
                    }
                }
            }
        });
}
