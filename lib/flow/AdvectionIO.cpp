#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iterator> // std::distance
#include <cctype>
#include "vapor/AdvectionIO.h"

auto flow::OutputFlowlinesNumSteps(const Advection *adv,
                                   const char *filename,
                                   size_t numSteps,
                                   bool append) -> int {
    std::FILE *f = nullptr;
    if (append) {
        f = std::fopen(filename, "a");
    } else {
        f = std::fopen(filename, "w");
    }
    if (f == nullptr)
        return FILE_ERROR;

    auto propertyNames = adv->GetPropertyVarNames();

    // Write the header
    if (!append) {
        std::fprintf(f, "%s%s", "# ID,  X-position,  Y-position,  Z-position,  Time,   ",
                     adv->GetValueVarName().c_str());

        for (auto &n : propertyNames)
            std::fprintf(f, ",  %s", n.c_str());
        std::fprintf(f, "\n");
    }

    // Write the trajectories
    for (size_t s_idx = 0; s_idx < adv->GetNumberOfStreams(); s_idx++) {
        const auto &stream = adv->GetStreamAt(s_idx);

        size_t step = 0;
        for (const auto &p : stream) {
            if (!p.IsSpecial()) {
                std::fprintf(f, "%lu, %f, %f, %f, %f, %f", s_idx, p.location.x, p.location.y,
                             p.location.z, p.time, p.value);

                auto props = p.GetPropertyList();
                // A quick sanity check
                assert(std::distance(props.cbegin(), props.cend()) == propertyNames.size());
                for (const auto &val : props)
                    std::fprintf(f, ", %f", val);

                std::fprintf(f, "\n"); // end of one line
                step++;
            }
            if (step > numSteps) // when numSteps + 1 particles are printed.
                break;
        }
    }

    std::fclose(f);

    return 0;
}

auto flow::OutputFlowlinesMaxTime(const Advection *adv,
                                  const char *filename,
                                  float maxTime,
                                  bool append) -> int {
    std::FILE *f = nullptr;
    if (append) {
        f = std::fopen(filename, "a");
    } else {
        f = std::fopen(filename, "w");
    }
    if (f == nullptr)
        return FILE_ERROR;

    auto propertyNames = adv->GetPropertyVarNames();

    // Write the header
    if (!append) {
        std::fprintf(f, "%s%s", "# ID,  X-position,  Y-position,  Z-position,  Time,   ",
                     adv->GetValueVarName().c_str());

        for (auto &n : propertyNames)
            std::fprintf(f, ",  %s", n.c_str());
        std::fprintf(f, "\n");
    }

    // Write the trajectories
    for (size_t s_idx = 0; s_idx < adv->GetNumberOfStreams(); s_idx++) {
        const auto &stream = adv->GetStreamAt(s_idx);

        for (const auto &p : stream) {

            if (p.time > maxTime)
                break;

            if (!p.IsSpecial()) {
                std::fprintf(f, "%lu, %f, %f, %f, %f, %f", s_idx, p.location.x, p.location.y,
                             p.location.z, p.time, p.value);

                auto props = p.GetPropertyList();
                // A quick sanity check
                assert(std::distance(props.cbegin(), props.cend()) == propertyNames.size());
                for (const auto &val : props)
                    std::fprintf(f, ", %f", val);

                std::fprintf(f, "\n"); // end of one line
            }
        }
    }

    std::fclose(f);

    return 0;
}

auto flow::InputSeedsCSV(const std::string &filename,
                         Advection *adv) -> int {
    std::ifstream ifs(filename);
    if (!ifs.is_open())
        return FILE_ERROR;

    std::vector<Particle> newSeeds;

    for (std::string line; std::getline(ifs, line);) {
        // remove spaces/tabs in this line
        line.erase(std::remove_if(line.begin(), line.end(),
                                  [](unsigned char c) { return std::isspace(c); }),
                   line.end());

        // skip this line if it's empty
        if (line.empty())
            continue;

        // If leading by a #, then skip it.
        if (line.front() == '#')
            continue;

        // Now try to parse numbers separated by comma
        std::stringstream ss(line);
        std::vector<float> valFloat;
        valFloat.reserve(4);
        for (std::string tmp; std::getline(ss, tmp, ',');) {
            try {
                valFloat.push_back(std::stof(tmp));
            } catch (const std::invalid_argument &e) {
                ifs.close();
                return NO_FLOAT;
            }
            if (valFloat.size() >= 4) // we parse at most 4 values, and discard the rest of this line.
                break;
        }

        if (valFloat.size() < 3) { // less than 3 values provided in this line
            ifs.close();
            return NO_FLOAT;
        }

        newSeeds.emplace_back(valFloat[0], valFloat[1], valFloat[2], valFloat[3]);
    }
    ifs.close();

    if (!newSeeds.empty())
        adv->UseSeedParticles(newSeeds);

    return 0;
}
