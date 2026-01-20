// Copyright (c) 2025 Hygor Ostrowskij de Morais <hygor.o.morais@gmail.com>

#pragma once

#include <CLI/CLI.hpp>
#include <hy_windows.h>
#include <phnt.h>
#include <stdexcept>
#include <vector>
#include <boost/algorithm/string.hpp>
#include "utils.hpp"


namespace hy {


inline std::string expand_normalize_path(std::string path) {
    DWORD size;

    size = ExpandEnvironmentStringsW(
        widen(path).data(),
        nullptr,
        0
    );
    auto expanded_path = std::wstring(size-1, L'\0');
    size = ExpandEnvironmentStringsW(
        widen(path).data(),
        expanded_path.data(),
        size
    );

    // Ensure no trailing slash by appending an empty path component, and then calling parent_path after normalization.
    const auto normalized_path = (std::filesystem::path(expanded_path) / "").lexically_normal().parent_path().wstring();

    auto upper_normalized_path = std::wstring(normalized_path.size(), L'\0');

    UNICODE_STRING dst = {};
    dst.Buffer        = upper_normalized_path.data();
    dst.MaximumLength = upper_normalized_path.size() * sizeof(wchar_t);

    UNICODE_STRING src = {};
    RtlInitUnicodeString(&src, normalized_path.data());

    RtlUpcaseUnicodeString(&dst, &src, false);

    return narrow(upper_normalized_path);
}


inline bool nt_compare_paths(std::string path1, std::string path2) {
    auto expanded1 = expand_normalize_path(path1);
    auto expanded2 = expand_normalize_path(path2);

    fmt::print("0: {}\n", expanded1);
    fmt::print("1: {}\n", expanded2);

    return expanded1 == expanded2;
}


inline void ensure_path_variable(
    std::vector<std::string> append_entries,
    std::vector<std::string> prepend_entries,
    std::vector<std::string> remove_entries,
    HKEY root_key
) {
    LSTATUS status = ERROR_MORE_DATA;
    DWORD dwType;
    DWORD cbData = sizeof(wchar_t);
    std::wstring value_value;

    if (!(root_key == HKEY_LOCAL_MACHINE || root_key == HKEY_CURRENT_USER)) {
        throw std::runtime_error("Invalid arguments.");
    }

    auto sub_key = root_key == HKEY_LOCAL_MACHINE ? L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment" : L"Environment";

    while (true) {
        status = RegGetValueW(
            root_key,
            sub_key,
            L"PATH",
            // RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND,
            RRF_RT_ANY | RRF_NOEXPAND,
            &dwType,
            value_value.data(),
            &cbData
        );

        if (!(status == ERROR_SUCCESS || status == ERROR_MORE_DATA)) {
            throw std::runtime_error(fmt::format("RegGetValueW error: {}", status));
        }

        value_value.resize(cbData / sizeof(wchar_t) - 1);

        if (status == ERROR_SUCCESS) {
            break;
        }
    }

    std::string path = narrow(value_value);
    std::vector<std::string> path_entries;

    boost::split(path_entries, path, boost::is_any_of(";"));

    for (auto& item : path_entries) {
        boost::trim(item);
    }

    std::erase_if(path_entries, [](const auto& item) {
        return item.empty();
    });

    for (const auto& entry : remove_entries) {
        std::erase_if(path_entries, [&](const auto& item) {
            return nt_compare_paths(entry, item);
        });
    }
    for (const auto& entry : append_entries) {
        std::erase_if(path_entries, [&](const auto& item) {
            return nt_compare_paths(entry, item);
        });

        path_entries.emplace_back(entry);
    }
    for (const auto& entry : prepend_entries) {
        std::erase_if(path_entries, [&](const auto& item) {
            return nt_compare_paths(entry, item);
        });

        path_entries.emplace(path_entries.begin(), entry);
    }

    path = boost::algorithm::join(path_entries, ";");

    auto wpath = widen(path);
    status = RegSetKeyValueW(
        HKEY_CURRENT_USER,  // root_key,
        L"Environment",     // sub_key,
        L"__tmp.PATH",      // L"PATH",
        REG_EXPAND_SZ,
        wpath.data(),
        wpath.size() * sizeof(wchar_t) + sizeof(wchar_t)
    );

    if (status) {
        throw std::runtime_error(fmt::format("RegSetKeyValueW error: {}", status));
    }
}


class InternalSubcommand {
public:
    InternalSubcommand(CLI::App* app) {
        sub = app->add_subcommand("__internal__", "")->group("");

        sub->add_option("--ensure-path", ensure_path, "")->group("");
        sub->add_option("--remove-from-path", remove_from_path, "")->group("");
        sub->add_flag("--hklm", hklm_flag, "")->group("");

        sub->callback([this]() {
            run();
        });
    }

private:
    void run() {
        HKEY root_key = hklm_flag ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
        if (!ensure_path.empty()) {
            ensure_path_variable({}, {ensure_path}, {}, root_key);

            std::exit(0);
        } else if (!remove_from_path.empty()) {
            ensure_path_variable({}, {}, {remove_from_path}, root_key);

            std::exit(0);
        }

        std::exit(1);
    }

private:
    CLI::App* sub = nullptr;

    std::string ensure_path;
    std::string remove_from_path;
    bool hklm_flag = false;
};


}  // namespace
