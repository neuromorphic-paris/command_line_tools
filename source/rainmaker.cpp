#include "html.hpp"

#include <pontella.hpp>
#include <tarsier/stitch.hpp>
#include <tarsier/maskIsolated.hpp>

#include <iostream>
#include <array>
#include <algorithm>
#include <limits>

/// ExposureMeasurement represents an exposure measurement as a time delta.
struct ExposureMeasurement {
    uint16_t x;
    uint16_t y;
    uint64_t timestamp;
    uint64_t timeDelta;
};

/// Mode represents the input mode.
enum class Mode {
    grey,
    change,
    color,
};

int main(int argc, char* argv[]) {
    auto showHelp = false;
    try {
        const auto command = pontella::parse(argc, argv, 2, {
            {"timestamp", "t"},
            {"duration", "d"},
            {"mode", "m"},
            {"decay", "e"},
            {"ratio", "r"},
            {"frametime", "f"},
        },
        {
            {"help", "h"},
        });

        if (command.flags.find("help") != command.flags.end()) {
            showHelp = true;
        } else {

            // retrieve the first timestamp
            uint64_t firstTimestamp = 0;
            {
                const auto firstTimestampCandidate = command.options.find("timestamp");
                if (firstTimestampCandidate != command.options.end()) {
                    try {
                        firstTimestamp = std::stoull(firstTimestampCandidate->second);
                    } catch (const std::invalid_argument&) {
                        throw std::runtime_error("[timestamp] must be a positive integer (got '" + firstTimestampCandidate->second + "')");
                    }
                }
            }

            // retrieve the last timestamp
            uint64_t lastTimestamp = firstTimestamp + 1000000;
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

            // retrieve the decay
            uint64_t decay = 10000;
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

            // check the output file
            {
                std::ofstream output(command.arguments[1]);
                if (!output.good()) {
                    throw sepia::UnwritableFile(command.arguments[1]);
                }
            }

            // retrieve the input mode
            auto mode = Mode::grey;
            {
                const auto modeCandidate = command.options.find("mode");
                if (modeCandidate != command.options.end()) {

                    if (modeCandidate->second == "grey") {
                        // default
                    } else if (modeCandidate->second == "change") {
                        mode = Mode::change;
                    } else if (modeCandidate->second == "color") {
                        mode = Mode::color;
                    } else {
                        throw std::runtime_error("unsupported mode '" + modeCandidate->second + "'");
                    }
                }
            }

            // common variables
            auto colorEvents = std::vector<sepia::ColorEvent>();
            auto baseFrame = std::array<uint8_t, 304 * 240 * 4>();
            std::mutex lock;
            lock.lock();
            auto eventStreamObservableException = std::current_exception();

            switch (mode) {
                case Mode::grey: {

                    // retrieve the tone mapping discard ratio
                    auto ratio = 0.05;
                    {
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

                    // retrieve exposure measurements (non-tone-mapped yet)
                    auto exposureMeasurements = std::vector<ExposureMeasurement>();
                    auto timeDeltasBaseFrame = std::array<uint64_t, 304 * 240>();
                    timeDeltasBaseFrame.fill(std::numeric_limits<uint64_t>::max());
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
                                tarsier::make_maskIsolated<ExposureMeasurement>(
                                    304,
                                    240,
                                    decay,
                                    [
                                        firstTimestamp,
                                        lastTimestamp,
                                        &exposureMeasurements,
                                        &timeDeltasBaseFrame
                                    ](ExposureMeasurement exposureMeasurement) -> void {
                                        if (exposureMeasurement.timestamp >= lastTimestamp) {
                                            throw sepia::EndOfFile(); // throw to stop the event stream observable
                                        } else if (exposureMeasurement.timestamp >= firstTimestamp) {
                                            exposureMeasurements.push_back(exposureMeasurement);
                                        } else {
                                            timeDeltasBaseFrame[exposureMeasurement.x + 304 * (240 - 1 - exposureMeasurement.y)] = exposureMeasurement.timeDelta;
                                        }
                                    }
                                )
                            )
                        ),
                        [&lock, &eventStreamObservableException](std::exception_ptr exception) -> void {
                            eventStreamObservableException = exception;
                            lock.unlock();
                        },
                        command.arguments[0],
                        sepia::EventStreamObservable::Dispatch::asFastAsPossible
                    );
                    lock.lock();
                    lock.unlock();
                    try {
                        std::rethrow_exception(eventStreamObservableException);
                    } catch (const sepia::EndOfFile&) {
                        if (exposureMeasurements.empty()) {
                            throw std::runtime_error("no threshold crossings are present in the given time interval");
                        }

                        // compute tone-mapping slope and intercept
                        auto slope = 0.0;
                        auto intercept = 128.0;
                        {
                            auto timeDeltas = std::vector<uint64_t>();
                            timeDeltas.resize(exposureMeasurements.size());
                            std::transform(
                                exposureMeasurements.begin(),
                                exposureMeasurements.end(),
                                timeDeltas.begin(),
                                [](ExposureMeasurement exposureMeasurement) -> uint64_t {
                                    return exposureMeasurement.timeDelta;
                                }
                            );
                            timeDeltas.reserve(timeDeltas.size() + timeDeltasBaseFrame.size());
                            for (auto&& timeDelta : timeDeltasBaseFrame) {
                                if (timeDelta < std::numeric_limits<uint64_t>::max()) {
                                    timeDeltas.push_back(timeDelta);
                                }
                            }
                            auto discardedTimeDeltas = std::vector<uint64_t>(
                                static_cast<std::size_t>((exposureMeasurements.size() + 304 * 240) * ratio)
                            );
                            std::partial_sort_copy(
                                timeDeltas.begin(),
                                timeDeltas.end(),
                                discardedTimeDeltas.begin(),
                                discardedTimeDeltas.end()
                            );
                            auto whiteDiscard = discardedTimeDeltas.back();
                            const auto whiteDiscardFallback = discardedTimeDeltas.front();
                            std::partial_sort_copy(
                                timeDeltas.begin(),
                                timeDeltas.end(),
                                discardedTimeDeltas.begin(),
                                discardedTimeDeltas.end(),
                                std::greater<uint64_t>()
                            );
                            auto blackDiscard = discardedTimeDeltas.back();
                            const auto blackDiscardFallback = discardedTimeDeltas.front();

                            if (blackDiscard <= whiteDiscard) {
                                whiteDiscard = whiteDiscardFallback;
                                blackDiscard = blackDiscardFallback;
                            }
                            if (blackDiscard > whiteDiscard) {
                                const auto delta = std::log(static_cast<double>(blackDiscard) / static_cast<double>(whiteDiscard));
                                slope = -255.0 / delta;
                                intercept = 255.0 * std::log(static_cast<double>(blackDiscard)) / delta;
                            }
                        }
                        for (auto timeDeltaIterator = timeDeltasBaseFrame.begin(); timeDeltaIterator != timeDeltasBaseFrame.end(); ++timeDeltaIterator) {
                            const auto exposureCandidate = slope * std::log(*timeDeltaIterator) + intercept;
                            const auto exposure = static_cast<uint8_t>(exposureCandidate > 255 ? 255 : (exposureCandidate < 0 ? 0 : exposureCandidate));
                            const auto index = (timeDeltaIterator - timeDeltasBaseFrame.begin()) * 4;
                            baseFrame[index] = exposure;
                            baseFrame[index + 1] = exposure;
                            baseFrame[index + 2] = exposure;
                            baseFrame[index + 3] = 0xff;
                        }
                        colorEvents.resize(exposureMeasurements.size());
                        std::transform(
                            exposureMeasurements.begin(),
                            exposureMeasurements.end(),
                            colorEvents.begin(),
                            [slope, intercept](const ExposureMeasurement& exposureMeasurement) -> sepia::ColorEvent {
                                const auto exposureCandidate = slope * std::log(exposureMeasurement.timeDelta) + intercept;
                                const auto exposure = static_cast<uint8_t>(exposureCandidate > 255 ? 255 : (exposureCandidate < 0 ? 0 : exposureCandidate));
                                return sepia::ColorEvent{
                                    exposureMeasurement.x,
                                    exposureMeasurement.y,
                                    exposureMeasurement.timestamp,
                                    exposure,
                                    exposure,
                                    exposure
                                };
                            }
                        );
                    }
                    break;
                }
                case Mode::change: {

                    // retrieve change detections
                    auto eventStreamObservable = sepia::make_eventStreamObservable(
                        sepia::make_split(
                            tarsier::make_maskIsolated<sepia::ChangeDetection>(
                                304,
                                240,
                                decay,
                                [firstTimestamp, lastTimestamp, &colorEvents](sepia::ChangeDetection changeDetection) -> void {
                                    if (changeDetection.timestamp >= lastTimestamp) {
                                        throw sepia::EndOfFile(); // throw to stop the event stream observable
                                    } else if (changeDetection.timestamp >= firstTimestamp) {
                                        if (changeDetection.isIncrease) {
                                            colorEvents.push_back(sepia::ColorEvent{changeDetection.x, changeDetection.y, changeDetection.timestamp, 0xdd, 0xdd, 0xdd});
                                        } else {
                                            colorEvents.push_back(sepia::ColorEvent{changeDetection.x, changeDetection.y, changeDetection.timestamp, 0x22, 0x22, 0x22});
                                        }
                                    }
                                }
                            ),
                            [](sepia::ThresholdCrossing) -> void {}
                        ),
                        [&lock, &eventStreamObservableException](std::exception_ptr exception) -> void {
                            eventStreamObservableException = exception;
                            lock.unlock();
                        },
                        command.arguments[0],
                        sepia::EventStreamObservable::Dispatch::asFastAsPossible
                    );
                    lock.lock();
                    lock.unlock();
                    try {
                        std::rethrow_exception(eventStreamObservableException);
                    } catch (const sepia::EndOfFile&) {
                        if (colorEvents.empty()) {
                            throw std::runtime_error("no change detections are present in the given time interval");
                        }
                    }
                    break;
                }
                case Mode::color: {

                    // retrieve color events
                    auto eventStreamObservable = sepia::make_colorEventStreamObservable(
                        tarsier::make_maskIsolated<sepia::ColorEvent>(
                            304,
                            240,
                            decay,
                            [firstTimestamp, lastTimestamp, &colorEvents, &baseFrame](sepia::ColorEvent colorEvent) -> void {
                                if (colorEvent.timestamp >= lastTimestamp) {
                                    throw sepia::EndOfFile(); // throw to stop the event stream observable
                                } else if (colorEvent.timestamp >= firstTimestamp) {
                                    colorEvents.push_back(colorEvent);
                                } else {
                                    const auto index = (colorEvent.x + 304 * (240 - 1 - colorEvent.y)) * 4;
                                    baseFrame[index] = colorEvent.r;
                                    baseFrame[index + 1] = colorEvent.g;
                                    baseFrame[index + 2] = colorEvent.b;
                                    baseFrame[index + 3] = 0xff;
                                }
                            }
                        ),
                        [&lock, &eventStreamObservableException](std::exception_ptr exception) -> void {
                            eventStreamObservableException = exception;
                            lock.unlock();
                        },
                        command.arguments[0],
                        sepia::EventStreamObservable::Dispatch::asFastAsPossible
                    );
                    lock.lock();
                    lock.unlock();
                    try {
                        std::rethrow_exception(eventStreamObservableException);
                    } catch (const sepia::EndOfFile&) {
                        if (colorEvents.empty()) {
                            throw std::runtime_error("no color events are present in the given time interval");
                        }
                    }
                    break;
                }
            }

            // retrieve the frametime
            uint64_t frametime = 0;
            if (mode != Mode::change) {
                const auto frametimeCandidate = command.options.find("frametime");
                if (frametimeCandidate == command.options.end() || frametimeCandidate->second == "auto") {
                    frametime = (lastTimestamp - firstTimestamp) / (colorEvents.size() / (304 * 240));
                    if (frametime == 0) {
                        frametime = 1;
                    }
                } else if (frametimeCandidate != command.options.end() && frametimeCandidate->second != "none") {
                    try {
                        const auto unvalidatedFrametime = std::stoull(frametimeCandidate->second);
                        if (unvalidatedFrametime == 0) {
                            throw std::runtime_error("[frametime] must be either auto, none or a strictly positive integer (got '"
                                + frametimeCandidate->second
                                + "')"
                            );
                        }
                        frametime = static_cast<uint64_t>(unvalidatedFrametime);
                    } catch (const std::invalid_argument&) {
                        throw std::runtime_error("[frametime] must be either auto, none or a strictly positive integer (got '"
                            + frametimeCandidate->second
                            + "')"
                        );
                    }
                }
            }

            // generate the frames
            auto frames = std::vector<std::array<uint8_t, 304 * 240 * 4>>();
            if (frametime > 0) {
                frames.push_back(baseFrame);
                for (auto&& colorEvent : colorEvents) {
                    if (colorEvent.timestamp >= lastTimestamp) {
                        break;
                    } else {
                        const auto index = (colorEvent.x + 304 * (240 - 1 - colorEvent.y)) * 4;
                        frames.back()[index] = colorEvent.r;
                        frames.back()[index + 1] = colorEvent.g;
                        frames.back()[index + 2] = colorEvent.b;
                        frames.back()[index + 3] = 0xff;
                        if (colorEvent.timestamp >= firstTimestamp && colorEvent.timestamp - firstTimestamp >= frametime * (frames.size() - 1)) {
                            if (frametime * frames.size() >= lastTimestamp) {
                                break;
                            } else {
                                frames.push_back(frames.back());
                            }
                        }
                    }
                }
            }

            // write the html output
            writeHtml(
                command.arguments[1],
                "Events",
                firstTimestamp,
                lastTimestamp,
                colorEvents,
                frames,
                frametime
            );
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
            "    -m [mode], --mode [mode]                     sets the mode (one of 'grey', 'change', 'color', defaults to 'grey')\n"
            "                                                     grey: generates points from exposure measurements, requires an ATIS event stream\n"
            "                                                     change: generates points from change detections, requires an ATIS event stream\n"
            "                                                     color: generates points from color events, requires a color event stream\n"
            "    -r [ratio], --ratio [ratio]                  sets the discard ratio for logarithmic tone mapping (default to 0.05)\n"
            "                                                     ignored if the mode is not 'grey'\n"
            "    -f [frame time], --frametime [frame time]    sets the time between two frames (defaults to auto)\n"
            "                                                     auto calculates the time between two frames so that\n"
            "                                                     there is the same amount of raw data in events and frames,\n"
            "                                                     a duration in microseconds can be provided instead,\n"
            "                                                     none disables the frames\n"
            "                                                     ignored if the mode is 'change'\n"
            "    -h, --help                                   shows this help message\n"
        << std::endl;
    }

    return 0;
}
