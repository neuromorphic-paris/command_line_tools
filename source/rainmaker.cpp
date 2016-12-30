#include "html.hpp"
#include "lodepng.hpp"

#include <pontella.hpp>
#include <sepia.hpp>
#include <tarsier/stitch.hpp>
#include <tarsier/maskIsolated.hpp>

#include <iostream>
#include <array>

struct ExposureMeasurement {
    uint16_t x;
    uint16_t y;
    uint64_t timestamp;
    uint64_t timeDelta;
};

std::string encodedCharactersFromBytes(const std::vector<unsigned char>& bytes) {
    const auto characters = std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
    auto encodedCharacters = std::string();
    auto length = bytes.size();
    auto data = static_cast<std::size_t>(0);
    auto index = static_cast<std::size_t>(0);
    while (length > 2) {
        data = bytes[index++] << 16;
        data |= bytes[index++] << 8;
        data |= bytes[index++];
        encodedCharacters.append(1, characters[(data & (63 << 18)) >> 18]);
        encodedCharacters.append(1, characters[(data & (63 << 12)) >> 12]);
        encodedCharacters.append(1, characters[(data & (63 << 6)) >> 6]);
        encodedCharacters.append(1, characters[data & 63]);
        length -= 3;
    }
    if (length == 2) {
        data = bytes[index++] << 16;
        data |= bytes[index++] << 8;
        encodedCharacters.append(1, characters[(data & (63 << 18)) >> 18]);
        encodedCharacters.append(1, characters[(data & (63 << 12)) >> 12]);
        encodedCharacters.append(1, characters[(data & (63 << 6)) >> 6]);
        encodedCharacters.append(1, '=');
    } else if (length == 1) {
        data = bytes[index++] << 16;
        encodedCharacters.append(1, characters[(data & (63 << 18)) >> 18]);
        encodedCharacters.append(1, characters[(data & (63 << 12)) >> 12]);
        encodedCharacters.append("==", 2);
    }
    return encodedCharacters;
}

int main(int argc, char* argv[]) {
    auto showHelp = false;
    try {
        const auto command = pontella::parse(argc, argv, 2, {
            {"timestamp", "t"},
            {"duration", "d"},
            {"decay", "e"},
            {"ratio", "r"},
            {"frametime", "f"},
        },
        {
            {"change", "c"},
            {"help", "h"},
        });

        if (command.flags.find("help") != command.flags.end()) {
            showHelp = true;
        } else {
            auto firstTimestamp = static_cast<int64_t>(0);
            {
                const auto firstTimestampCandidate = command.options.find("timestamp");
                if (firstTimestampCandidate != command.options.end()) {
                    try {
                        firstTimestamp = std::stoll(firstTimestampCandidate->second);
                    } catch (const std::invalid_argument&) {
                        throw std::runtime_error("[timestamp] must be an integer (got '" + firstTimestampCandidate->second + "')");
                    }
                }
            }

            auto lastTimestamp = static_cast<int64_t>(1000000);
            {
                const auto durationCandidate = command.options.find("duration");
                if (durationCandidate != command.options.end()) {
                    try {
                        const auto unvalidatedDuration = std::stoll(durationCandidate->second);
                        if (unvalidatedDuration <= 0) {
                            throw std::runtime_error("[duration] must be a positive integer (got '" + durationCandidate->second + "')");
                        }
                        lastTimestamp = firstTimestamp + unvalidatedDuration;
                    } catch (const std::invalid_argument&) {
                        throw std::runtime_error("[duration] must be a positive integer (got '" + durationCandidate->second + "')");
                    }
                }
            }

            auto decay = static_cast<uint64_t>(10000);
            {
                const auto decayCandidate = command.options.find("decay");
                if (decayCandidate != command.options.end()) {
                    try {
                        const auto unvalidatedDecay = std::stoll(decayCandidate->second);
                        if (unvalidatedDecay < 0) {
                            throw std::runtime_error("[decay] must be a positive integer (got '" + decayCandidate->second + "')");
                        }
                        decay = static_cast<uint64_t>(unvalidatedDecay);
                    } catch (const std::invalid_argument&) {
                        throw std::runtime_error("[decay] must be a positive integer (got '" + decayCandidate->second + "')");
                    }
                }
            }

            auto change = (command.flags.find("change") != command.flags.end());

            auto ratio = static_cast<double>(0.05);
            if (!change) {
                const auto ratioCandidate = command.options.find("ratio");
                if (ratioCandidate != command.options.end()) {
                    try {
                        ratio = std::stod(ratioCandidate->second);
                        if (ratio < 0 || ratio >= 1) {
                            throw std::runtime_error("[ratio] must be a real number in the range [0, 1[ (got '" + ratioCandidate->second + "')");
                        }
                    } catch (const std::invalid_argument&) {
                        throw std::runtime_error("[ratio] must be a real number in the range [0, 1[ (got '" + ratioCandidate->second + "')");
                    }
                }
            }

            auto autoFrametime = true;
            auto frametime = static_cast<std::size_t>(0);
            if (!change) {
                const auto frametimeCandidate = command.options.find("frametime");
                if (frametimeCandidate != command.options.end()) {
                    if (frametimeCandidate->second == "auto") {
                        // default
                    } else if (frametimeCandidate->second == "none") {
                        autoFrametime = false;
                    } else {
                        try {
                            const auto unvalidatedFrametime = std::stoll(frametimeCandidate->second);
                            if (unvalidatedFrametime < 1) {
                                throw std::runtime_error("[frametime] must be either auto, none or a strictly positive integer (got '"
                                    + frametimeCandidate->second
                                    + "')"
                                );
                            }
                            autoFrametime = false;
                            frametime = static_cast<std::size_t>(unvalidatedFrametime);
                        } catch (const std::invalid_argument&) {
                            throw std::runtime_error("[frametime] must be either auto, none or a strictly positive integer (got '"
                                + frametimeCandidate->second
                                + "')"
                            );
                        }
                    }
                }
            }

            std::ofstream htmlFile(command.arguments[1]);
            if (!htmlFile.good()) {
                throw sepia::UnreadableFile(command.arguments[1]);
            }

            writeHtmlBeforeFramesTo(htmlFile, firstTimestamp, lastTimestamp, change);

            std::mutex lock;
            lock.lock();
            auto logObservableException = std::current_exception();
            auto base = std::vector<uint64_t>(304 * 240);
            auto exposureMeasurements = std::vector<ExposureMeasurement>();

            if (change) {
                auto eventStreamObservable = sepia::make_eventStreamObservable(
                    sepia::make_split(
                        [firstTimestamp, lastTimestamp, &exposureMeasurements](sepia::ChangeDetection changeDetection) -> void {
                            if (changeDetection.timestamp >= firstTimestamp) {
                                if (changeDetection.timestamp > lastTimestamp) {
                                    throw sepia::EndOfFile(); // throw to stop the log observable
                                } else {
                                    exposureMeasurements.push_back(ExposureMeasurement{
                                        changeDetection.x,
                                        changeDetection.y,
                                        changeDetection.timestamp,
                                        static_cast<uint64_t>(changeDetection.isIncrease ? 2 : 1)
                                    });
                                }
                            }
                        },
                        [](sepia::ThresholdCrossing) -> void {}
                    ),
                    [&lock, &logObservableException](std::exception_ptr exception) -> void {
                        logObservableException = exception;
                        lock.unlock();
                    },
                    command.arguments[0],
                    sepia::EventStreamObservable::Dispatch::asFastAsPossible
                );
                lock.lock();
                lock.unlock();
            } else {
                auto eventStreamObservable = sepia::make_eventStreamObservable(
                    sepia::make_split(
                        [](sepia::ChangeDetection) -> void {},
                        tarsier::make_stitch<sepia::ThresholdCrossing, ExposureMeasurement>(
                            304,
                            240,
                            [](sepia::ThresholdCrossing secondThresholdCrossing, uint64_t timeDelta) -> ExposureMeasurement {
                                return ExposureMeasurement{
                                    secondThresholdCrossing.x,
                                    secondThresholdCrossing.y,
                                    secondThresholdCrossing.timestamp,
                                    timeDelta,
                                };
                            },
                            [firstTimestamp, lastTimestamp, &base, &exposureMeasurements](ExposureMeasurement exposureMeasurement) -> void {
                                if (exposureMeasurement.timestamp < firstTimestamp) {
                                    base[exposureMeasurement.x + exposureMeasurement.y * 304] = exposureMeasurement.timeDelta;
                                } else if (exposureMeasurement.timestamp > lastTimestamp) {
                                    throw sepia::EndOfFile(); // throw to stop the log observable
                                } else {
                                    exposureMeasurements.push_back(exposureMeasurement);
                                }
                            }
                        )
                    ),
                    [&lock, &logObservableException](std::exception_ptr exception) -> void {
                        logObservableException = exception;
                        lock.unlock();
                    },
                    command.arguments[0],
                    sepia::EventStreamObservable::Dispatch::asFastAsPossible
                );
                lock.lock();
                lock.unlock();
            }

            try {
                std::rethrow_exception(logObservableException);
            } catch (const sepia::EndOfFile&) {
                auto bytes = std::vector<unsigned char>();
                bytes.reserve(exposureMeasurements.size());
                auto timestampOffset = firstTimestamp;
                auto slope = static_cast<double>(0);
                auto intercept = static_cast<double>(128);
                auto maskIsolated = tarsier::make_maskIsolated<ExposureMeasurement>(
                    304,
                    240,
                    decay,
                    [&bytes, &timestampOffset, &slope, &intercept](ExposureMeasurement exposureMeasurement) mutable -> void {
                        auto reducedTimestamp = exposureMeasurement.timestamp - timestampOffset;
                        if (reducedTimestamp >= 0x7f) {
                            const auto offsetOverflow = static_cast<std::size_t>(reducedTimestamp / 0x7f);
                            reducedTimestamp -= offsetOverflow * 0x7f;
                            timestampOffset += offsetOverflow * 0x7f;
                            const auto count = offsetOverflow / 0xffffff;
                            bytes.reserve(bytes.size() + (count + 2) * 4);
                            for (auto&& index = static_cast<std::size_t>(0); index < count; ++index) {
                                bytes.push_back(0xff);
                                bytes.push_back(0xff);
                                bytes.push_back(0xff);
                                bytes.push_back(0xff);
                            }
                            const auto remainder = offsetOverflow - 0xffffff * count;
                            bytes.push_back(0xff);
                            bytes.push_back(remainder & 0xff);
                            bytes.push_back((remainder & 0xff00) >> 8);
                            bytes.push_back((remainder & 0xff0000) >> 16);
                        } else {
                            bytes.reserve(bytes.size() + 4);
                        }
                        bytes.push_back(exposureMeasurement.y);
                        bytes.push_back(exposureMeasurement.x & 0xff);
                        bytes.push_back(((exposureMeasurement.x & 0x100) >> 1) | reducedTimestamp);
                        auto exposure = slope * std::log(exposureMeasurement.timeDelta) + intercept;
                        if (exposure < 0) {
                            exposure = 0;
                        } else if (exposure > 255) {
                            exposure = 255;
                        }
                        bytes.push_back(exposure);
                    }
                );

                if (change) {
                    slope = static_cast<double>(220) / std::log(2);
                    intercept = 0;
                    for (auto&& exposureMeasurement : exposureMeasurements) {
                        maskIsolated(exposureMeasurement);
                    }
                } else {
                    if (autoFrametime) {
                        frametime = static_cast<std::size_t>(
                            static_cast<double>(lastTimestamp - firstTimestamp)
                            / static_cast<double>(exposureMeasurements.size())
                            * 304 * 240
                        );
                    }

                    // calculate the discards for tone-mapping
                    auto timeDeltas = std::vector<uint64_t>();
                    timeDeltas.reserve(base.size() + exposureMeasurements.size());
                    if (frametime > 0) { // the base frame is used for tone-mapping only if at least one frame is used
                        for (auto&& timeDelta : base) {
                            if (timeDelta > 0) {
                                timeDeltas.push_back(timeDelta);
                            }
                        }
                    }
                    for (auto&& exposureMeasurement : exposureMeasurements) {
                        timeDeltas.push_back(exposureMeasurement.timeDelta);
                    }
                    std::sort(timeDeltas.begin(), timeDeltas.end());
                    auto blackDiscard = timeDeltas[static_cast<std::size_t>(
                        static_cast<double>(timeDeltas.size()) * (1.0 - ratio)
                    )];
                    auto whiteDiscard = timeDeltas[static_cast<std::size_t>(
                        static_cast<double>(timeDeltas.size()) * ratio + 0.5
                    )];
                    if (blackDiscard <= whiteDiscard) {
                        blackDiscard = timeDeltas.back();
                        whiteDiscard = timeDeltas.front();
                    }
                    if (blackDiscard > whiteDiscard) {
                        const auto delta = std::log(static_cast<double>(blackDiscard) / static_cast<double>(whiteDiscard));
                        slope = -255.0 / delta;
                        intercept = 255.0 * std::log(static_cast<double>(blackDiscard)) / delta;
                    }

                    // generate the base frame
                    auto frame = std::vector<unsigned char>(304 * 240 * 4);
                    if (autoFrametime || frametime > 0) {
                        for (auto timeDeltaIterator = base.begin(); timeDeltaIterator != base.end(); ++timeDeltaIterator) {
                            auto exposure = static_cast<double>(0);
                            if (*timeDeltaIterator != 0) {
                                exposure = slope * std::log(*timeDeltaIterator) + intercept;
                                if (exposure < 0) {
                                    exposure = 0;
                                } else if (exposure > 255) {
                                    exposure = 255;
                                }
                            }
                            const auto index = (
                                ((timeDeltaIterator - base.begin()) % 304)
                                + (
                                    240 - 1 - (timeDeltaIterator - base.begin()) / 304
                                ) * 304
                            ) * 4;
                            frame[index] = static_cast<unsigned char>(exposure);
                            frame[index + 1] = static_cast<unsigned char>(exposure);
                            frame[index + 2] = static_cast<unsigned char>(exposure);
                            frame[index + 3] = static_cast<unsigned char>(0xff);
                        }

                        auto pngImage = std::vector<unsigned char>();
                        lodepng::encode(pngImage, frame, 304, 240);
                        writeHtmlFrame(htmlFile, encodedCharactersFromBytes(pngImage), 0);
                    }

                    // write the tone-mapped events (mask isolated ones)
                    auto nextFrameTimestamp = firstTimestamp + frametime;
                    for (auto&& exposureMeasurement : exposureMeasurements) {
                        maskIsolated(exposureMeasurement);
                        if (frametime > 0) {
                            if (exposureMeasurement.timestamp > nextFrameTimestamp) {
                                auto pngImage = std::vector<unsigned char>();
                                lodepng::encode(pngImage, frame, 304, 240);
                                writeHtmlFrame(
                                    htmlFile,
                                    encodedCharactersFromBytes(pngImage),
                                    static_cast<double>(10) / static_cast<double>(lastTimestamp - firstTimestamp) * (nextFrameTimestamp - firstTimestamp)
                                );
                                nextFrameTimestamp += frametime;
                            }
                            const auto index = (
                                exposureMeasurement.x + (240 - 1 - exposureMeasurement.y) * 304
                            ) * 4;
                            auto exposure = slope * std::log(exposureMeasurement.timeDelta) + intercept;
                            if (exposure < 0) {
                                exposure = 0;
                            } else if (exposure > 255) {
                                exposure = 255;
                            }
                            frame[index] = exposure;
                            frame[index + 1] = exposure;
                            frame[index + 2] = exposure;
                        }
                    }
                    if (nextFrameTimestamp <= lastTimestamp) {
                        auto pngImage = std::vector<unsigned char>();
                        lodepng::encode(pngImage, frame, 304, 240);
                        writeHtmlFrame(
                            htmlFile,
                            encodedCharactersFromBytes(pngImage),
                            static_cast<double>(10) / static_cast<double>(lastTimestamp - firstTimestamp) * (nextFrameTimestamp - firstTimestamp)
                        );
                    }
                }
                writeHtmlBetweenFramesAndEventsTo(htmlFile, firstTimestamp, lastTimestamp);
                const auto encodedCharacters = encodedCharactersFromBytes(bytes);
                htmlFile.write(reinterpret_cast<const char*>(encodedCharacters.data()), encodedCharacters.size());
                writeHtmlAfterEventsTo(htmlFile);
            }
        }
    } catch (const std::runtime_error& exception) {
        showHelp = true;
        if (!pontella::test(argc, argv, {"help", "h"})) {
            std::cout << "\e[31m" << exception.what() << "\e[0m" << std::endl;
        }
    }

    if (showHelp) {
        std::cout <<
            "Rainmaker generates a standalone html file with a 3D representation of events from an Event Stream file\n"
            "Syntax: ./rainmaker [options] /path/to/input.es /path/to/output.html\n"
            "Available options:\n"
            "    -t [timestamp], --timestamp [timestamp]      sets the initial timestamp for the point cloud (defaults to 0)\n"
            "    -d [duration], --duration [duration]         sets the duration (in microseconds) for the point cloud (defaults to 1000000)\n"
            "    -e [decay], --decay [decay]                  sets the decay used by the maskIsolated handler (defaults to 10000)\n"
            "    -r [ratio], --ratio [ratio]                  sets the discard ratio for logarithmic tone mapping (default to 0.05)\n"
            "    -f [frame time], --frametime [frame time]    sets the time between two frames (defaults to auto)\n"
            "                                                     auto calculates the time between two frames so that\n"
            "                                                     there is the same amount of raw data in events and frames,\n"
            "                                                     a duration in microseconds can be provided instead,\n"
            "                                                     none disables the frames\n"
            "    -c, --change                                 displays change detections instead of exposure measurements\n"
            "                                                     the ratio and frame time options are ignored\n"
            "    -h, --help                                   shows this help message\n"
        << std::endl;
    }

    return 0;
}
