// SPDX-License-Identifier: AGPL-3.0-only
#include "yolo/cli.hpp"
#include "yolo/config.hpp"
#include "yolo/log.hpp"
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " CONFIG.yaml\n";
        return 2;
    }
    try {
        yolo::run_cli(yolo::load_config(argv[1]));
        return 0;
    } catch (const std::exception& error) {
        yolo::log(yolo::LogLevel::Error, "cli", error.what());
        return 1;
    }
}
