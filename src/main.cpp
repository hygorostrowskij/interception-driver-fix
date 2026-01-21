// Copyright (c) 2025 Hygor Ostrowskij de Morais <hygor.o.morais@gmail.com>

#include <hy_windows.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "cli.hpp"
#include "core.hpp"
#include "service.hpp"
#include "install_uninstall.hpp"


using namespace hy;


int wmain(int argc, wchar_t** argv) {
    try {
        auto log_path = (std::filesystem::path(get_program_data_folder()) / MY_DATA_DIR_NAME / "logs/interception-driver-fix.log").lexically_normal();
        // std::filesystem::create_directories(log_path.parent_path());

        std::vector<spdlog::sink_ptr> sinks;

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sinks.emplace_back(console_sink);

        try {
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), false);
            sinks.emplace_back(file_sink);
        } catch (const spdlog::spdlog_ex& e) {
            spdlog::error(e.what());
        }

        auto logger = std::make_shared<spdlog::logger>("main", sinks.begin(), sinks.end());
        spdlog::set_default_logger(logger);

        spdlog::info("Starting application.");

        auto [app, main_cfg, install_service_cfg, uninstall_service_cfg] = parse_cli(argc, argv);

        spdlog::set_level(spdlog::level::info);

        if (app->got_subcommand("install-service")) {
            if (install_service_cfg.verbose) {
                spdlog::set_level(spdlog::level::debug);
            }

            install_service();
            return 0;
        }

        if (app->got_subcommand("uninstall-service")) {
            if (uninstall_service_cfg.verbose) {
                spdlog::set_level(spdlog::level::debug);
            }

            uninstall_service();
            return 0;
        }

        if (main_cfg.verbose) {
            spdlog::set_level(spdlog::level::debug);
        }

        SERVICE_TABLE_ENTRYW service_table[] = {
            { const_cast<LPWSTR>(L""), reinterpret_cast<LPSERVICE_MAIN_FUNCTIONW>(ServiceMain) },
            { nullptr, nullptr }
        };

        if (StartServiceCtrlDispatcherW(service_table)) {
            return 0;
        }

        if (auto err = GetLastError(); err != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            throw std::runtime_error(fmt::format("StartServiceCtrlDispatcherW error ({}).", err));
        }

        return real_main(main_cfg);
    } catch (const std::exception& e) {
        spdlog::error("Exception: {}", e.what());
        return 1;
    } catch (...) {
        spdlog::error("Unknown error.");
        return 1;
    }
}


// The different kinds of entrypoint normalization
// - UTF-8 argv, defined always as char**
// - argc and argv always present, instead of pCmdLine. This may be char** (UTF-8) or wchar_t** (~UTF-16), depending on cross platform needs.
// - hInstance can be trivially retrieved at runtime, instead of relied on.
// - nCmdShow can be trivially retrieved at runtime, along with all of STARTUPINFOW.
// - For Windows, on a non-console subsystem, a console may be dynamically allocated instead, if required.
// - For Windows, on a console subsystem, a console may be deallocated and its window hidden, if requested.
// - For Windows, services aren't able to fully rely on GetCommandLineA/W, and should rely on argc and argv.
//
// - To provide for these, UTF-16 to UTF-8 narrowing must be provided. Widening is not necessary for this use case.
//   Also, the narrowing must be able to work on wchar_t** and generate a char**
//
//
// - For Windows DLLs, hInstance can't be trivially retrieved at runtime, so passing it somehow is desired.
//   nCmdShow does not apply to DLLs at all, since it can't be passed.
//   The reason integer is important, and should be passed too.
//   The "reserved" value is somewhat important too, despite having been originally reserved, and should be passed too.
//   If argc and argv compatibility for DLLs is desired, they should be emulated with environment variables, configuration files or something similar. Shared or injected memory could work too.
//
//
// - For modding DLLs, they could also read and change argv, in order to accept command line parameters as usual, and then passing a normal command line to the wrapped application.
