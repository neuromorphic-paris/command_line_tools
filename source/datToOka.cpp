#include <pontella.hpp>
#include <opalKellyAtisSepia.hpp>

#include <array>
#include <iostream>

/// Status represents the reading state.
enum class Status {
    tdLate,
    apsLate,
    tdEndOfFile,
    apsEndOfFile,
};

int main(int argc, char* argv[]) {
    auto showHelp = false;
    try {
        const auto result = pontella::parse(argc, argv, 3, {}, {{"help", "h"}});
        if (result.flags.find("help") != result.flags.end()) {
            showHelp = true;
        } else {
            if (result.arguments[0] == result.arguments[1]) {
                throw std::runtime_error("The td and aps inputs must be different files");
            }
            if (result.arguments[0] == result.arguments[2]) {
                throw std::runtime_error("The td input and the oka output must be different files");
            }
            if (result.arguments[1] == result.arguments[2]) {
                throw std::runtime_error("The aps input and the oka output must be different files");
            }

            std::ifstream tdFile(result.arguments[0]);
            if (!tdFile.good()) {
                throw sepia::UnreadableFile(result.arguments[0]);
            }
            std::ifstream apsFile(result.arguments[1]);
            if (!apsFile.good()) {
                throw sepia::UnreadableFile(result.arguments[1]);
            }

            opalKellyAtisSepia::Log opalKellyAtisSepiaLog;
            opalKellyAtisSepiaLog.writeTo(result.arguments[2]);

            auto tdBytes = std::array<unsigned char, 8>();
            auto apsBytes = std::array<unsigned char, 8>();
            tdFile.read(const_cast<char*>(reinterpret_cast<const char*>(tdBytes.data())), tdBytes.size());
            apsFile.read(const_cast<char*>(reinterpret_cast<const char*>(apsBytes.data())), apsBytes.size());

            if (tdFile.eof() && apsFile.eof()) {
                throw std::runtime_error("Both the TD file and the APS file are empty");
            }

            auto tdEvent = sepia::Event{};
            auto apsEvent = sepia::Event{};
            auto status = Status::tdLate;
            if (tdFile.eof()) {
                status = Status::tdEndOfFile;
            } else {
                tdEvent = sepia::Event{
                    static_cast<uint16_t>(((static_cast<uint16_t>(tdBytes[5]) & 0x01) << 8) | tdBytes[4]),
                    static_cast<uint16_t>(((tdBytes[6] & 0x01) << 7) | ((tdBytes[5] & 0xfe) >> 1)),
                    static_cast<int64_t>(tdBytes[0])
                    | (static_cast<int64_t>(tdBytes[1]) << 8)
                    | (static_cast<int64_t>(tdBytes[2]) << 16)
                    | (static_cast<int64_t>(tdBytes[3]) << 24),
                    false,
                    ((tdBytes[6] & 0x2) >> 1) == 0x01,
                };
            }
            if (apsFile.eof()) {
                status = Status::apsEndOfFile;
            } else {
                apsEvent = sepia::Event{
                    static_cast<uint16_t>(((static_cast<uint16_t>(apsBytes[5]) & 0x01) << 8) | apsBytes[4]),
                    static_cast<uint16_t>(((apsBytes[6] & 0x01) << 7) | ((apsBytes[5] & 0xfe) >> 1)),
                    static_cast<uint32_t>(apsBytes[0])
                    | (static_cast<uint32_t>(apsBytes[1]) << 8)
                    | (static_cast<uint32_t>(apsBytes[2]) << 16)
                    | (static_cast<uint32_t>(apsBytes[3]) << 24),
                    true,
                    ((apsBytes[6] & 0x2) >> 1) == 0x01,
                };
            }
            if (!tdFile.eof() && !apsFile.eof()) {
                if (tdEvent.timestamp > apsEvent.timestamp) {
                    status = Status::apsLate;
                } else {
                    status = Status::tdLate;
                }
            }

            for (auto done = false; !done;) {
                switch (status) {
                    case Status::tdLate: {
                        opalKellyAtisSepiaLog(tdEvent);
                        tdFile.read(const_cast<char*>(reinterpret_cast<const char*>(tdBytes.data())), tdBytes.size());
                        if (tdFile.eof()) {
                            status = Status::tdEndOfFile;
                        } else {
                            tdEvent = sepia::Event{
                                static_cast<uint16_t>(((static_cast<uint16_t>(tdBytes[5]) & 0x01) << 8) | tdBytes[4]),
                                static_cast<uint16_t>(((tdBytes[6] & 0x01) << 7) | ((tdBytes[5] & 0xfe) >> 1)),
                                static_cast<int64_t>(tdBytes[0])
                                | (static_cast<int64_t>(tdBytes[1]) << 8)
                                | (static_cast<int64_t>(tdBytes[2]) << 16)
                                | (static_cast<int64_t>(tdBytes[3]) << 24),
                                false,
                                ((tdBytes[6] & 0x2) >> 1) == 0x01,
                            };
                            if (tdEvent.timestamp > apsEvent.timestamp) {
                                status = Status::apsLate;
                            }
                        }
                        break;
                    }
                    case Status::apsLate: {
                        opalKellyAtisSepiaLog(apsEvent);
                        apsFile.read(const_cast<char*>(reinterpret_cast<const char*>(apsBytes.data())), apsBytes.size());
                        if (apsFile.eof()) {
                            status = Status::apsEndOfFile;
                        } else {
                            apsEvent = sepia::Event{
                                static_cast<uint16_t>(((static_cast<uint16_t>(apsBytes[5]) & 0x01) << 8) | apsBytes[4]),
                                static_cast<uint16_t>(((apsBytes[6] & 0x01) << 7) | ((apsBytes[5] & 0xfe) >> 1)),
                                static_cast<uint32_t>(apsBytes[0])
                                | (static_cast<uint32_t>(apsBytes[1]) << 8)
                                | (static_cast<uint32_t>(apsBytes[2]) << 16)
                                | (static_cast<uint32_t>(apsBytes[3]) << 24),
                                true,
                                ((apsBytes[6] & 0x2) >> 1) == 0x01,
                            };
                            if (apsEvent.timestamp >= tdEvent.timestamp) {
                                status = Status::tdLate;
                            }
                        }
                        break;
                    }
                    case Status::tdEndOfFile: {
                        opalKellyAtisSepiaLog(apsEvent);
                        apsFile.read(const_cast<char*>(reinterpret_cast<const char*>(apsBytes.data())), apsBytes.size());
                        if (apsFile.eof()) {
                            done = true;
                        } else {
                            apsEvent = sepia::Event{
                                static_cast<uint16_t>(((static_cast<uint16_t>(apsBytes[5]) & 0x01) << 8) | apsBytes[4]),
                                static_cast<uint16_t>(((apsBytes[6] & 0x01) << 7) | ((apsBytes[5] & 0xfe) >> 1)),
                                static_cast<uint32_t>(apsBytes[0])
                                | (static_cast<uint32_t>(apsBytes[1]) << 8)
                                | (static_cast<uint32_t>(apsBytes[2]) << 16)
                                | (static_cast<uint32_t>(apsBytes[3]) << 24),
                                true,
                                ((apsBytes[6] & 0x2) >> 1) == 0x01,
                            };
                        }
                        break;
                    }
                    case Status::apsEndOfFile: {
                        opalKellyAtisSepiaLog(tdEvent);
                        tdFile.read(const_cast<char*>(reinterpret_cast<const char*>(tdBytes.data())), tdBytes.size());
                        if (tdFile.eof()) {
                            done = true;
                        } else {
                            tdEvent = sepia::Event{
                                static_cast<uint16_t>(((static_cast<uint16_t>(tdBytes[5]) & 0x01) << 8) | tdBytes[4]),
                                static_cast<uint16_t>(((tdBytes[6] & 0x01) << 7) | ((tdBytes[5] & 0xfe) >> 1)),
                                static_cast<int64_t>(tdBytes[0])
                                | (static_cast<int64_t>(tdBytes[1]) << 8)
                                | (static_cast<int64_t>(tdBytes[2]) << 16)
                                | (static_cast<int64_t>(tdBytes[3]) << 24),
                                false,
                                ((tdBytes[6] & 0x2) >> 1) == 0x01,
                            };
                        }
                        break;
                    }
                }
            }
        }
    } catch (const std::runtime_error& exception) {
        showHelp = true;
        if (!pontella::test(argc, argv, {"help", "h"})) {
            std::cout << "\e[31m" << exception.what() << "\e[0m" << std::endl;
        }
    }

    if (showHelp) {
        std::cout
            << "Syntax: ./datToOka [options] /path/to/input_td.dat /path/to/input_aps.dat /path/to/output.oka\n"
            << "Available options:\n"
            << "    -h, --help    show this help message\n"
        << std::endl;
    }

    return 0;
}
