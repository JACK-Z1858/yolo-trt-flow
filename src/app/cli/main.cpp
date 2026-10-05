// SPDX-License-Identifier: AGPL-3.0-only
#include "app/cli/cli.hpp"
#include "app/cli/config.hpp"
#include "common/log.hpp"
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " CONFIG.yaml\n";
        return 2;
    }
    try {
        app::cli::run_cli(app::cli::load_config(argv[1]));
        return 0;
    } catch (const std::exception& error) {
        common::log(common::LogLevel::Error, "cli", error.what());
        return 1;
    }
}
