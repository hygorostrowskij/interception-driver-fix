// © 2025 Hygor Ostrowskij de Morais <hygor.o.morais@gmail.com>

#pragma once

#include <hy_windows.h>
#include <phnt.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <iostream>
#include <filesystem>
#include <source_location>




namespace hy {


inline std::string narrow(std::wstring_view wsv);


inline std::wstring widen(std::string_view sv) {
    if (sv.empty()) {
        return L"";
    }

    int size_needed = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS,
        sv.data(), sv.size(),
        nullptr, 0
    );

    if (size_needed == 0) {
        throw std::runtime_error("widen failed.");
    }

    std::wstring widened_str(size_needed, L'\0');
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS,
        sv.data(), sv.size(),
        &widened_str[0], size_needed
    );
    widened_str.resize(size_needed);

    return widened_str;
}


inline std::string narrow(std::wstring_view wsv) {
    if (wsv.empty()) {
        return "";
    }

    int size_needed = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS,
        wsv.data(), wsv.size(),
        nullptr, 0,
        nullptr, nullptr
    );

    if (size_needed == 0) {
        throw std::runtime_error("narrow failed.");
    }

    std::string narrowed_str(size_needed, '\0');
    WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS,
        wsv.data(), wsv.size(),
        &narrowed_str[0], size_needed,
        nullptr, nullptr
    );
    narrowed_str.resize(size_needed);

    return narrowed_str;
}


inline size_t get_os_pointer_size() {
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    size_t osPointerSize = sizeof (void*);

    switch(si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
        case PROCESSOR_ARCHITECTURE_ARM64:
        case PROCESSOR_ARCHITECTURE_IA64:
            osPointerSize = 8;
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
        case PROCESSOR_ARCHITECTURE_ARM:
            osPointerSize = 4;
            break;
    }

    return osPointerSize;
}


inline int aligned_to(int value, int alignment_value) {
    return alignment_value * (int)std::ceil((double)value / (double)alignment_value);
}


inline bool is_path_relative_to(const std::filesystem::path& target, const std::filesystem::path& base) {
    auto target_it = target.wstring();
    auto base_it = base.wstring();

    auto mismatch_pair = std::mismatch(base_it.begin(), base_it.end(), target_it.begin(),
        [](WCHAR a, WCHAR b) {
            return RtlUpcaseUnicodeChar(a) == RtlUpcaseUnicodeChar(b);
        }
    );

    if (mismatch_pair.first == base_it.end()) {
        if (mismatch_pair.second == target_it.end() || *mismatch_pair.second == '\\') {
            return true;
        }
    }
    return false;
}


std::string get_program_data_folder() {
    HRESULT hr;
    PWSTR raw_path = nullptr;
    hr = SHGetKnownFolderPath(
        FOLDERID_ProgramData,
        0,
        nullptr,
        &raw_path
    );
    if (FAILED(hr)) {
        throw std::runtime_error("SHGetKnownFolderPath error.");
    }
    auto path = narrow(raw_path);
    CoTaskMemFree(raw_path);

    return path;
}


// template <typename T1>
// void errcheck_bool(
//     T1 value,
//     const std::source_location& location = std::source_location::current()
// ) {
//     if (!value) {
//         auto code = GetLastError();
//         auto formatted_code_message = std::wstring(512, '\0');
//         auto size = FormatMessageW(
//             FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
//             nullptr,
//             code,
//             0,
//             formatted_code_message.data(),
//             formatted_code_message.size(),
//             nullptr
//         );  // NOTE: Error checking omitted here.
//         formatted_code_message.resize(size);
//         formatted_code_message.erase(formatted_code_message.find_last_not_of(L" \n\r\t") + 1);
//         auto file_name = std::string(location.file_name());
//         // file_name.find_first_not_of("./\\");
//         auto message = fmt::format("[WinError {}] {}; file: {}:{}:{}; in function: {};", code, narrow(formatted_code_message), file_name, location.line(), location.column(), location.function_name());
//         spdlog::error(message);
//         throw std::runtime_error(message);
//     }
// }


// template <typename T1>
// [[nodiscard]] auto errcheck_null(
//     T1 value,
//     const std::source_location& location = std::source_location::current()
// ) {
//     if (!value) {
//         auto code = GetLastError();
//         spdlog::error("code: {}; file: {}; line: {}; column: {}; function: {};", code, location.file_name(), location.line(), location.column(), location.function_name());
//         throw std::runtime_error("Error");
//     }
//     return value;
// }


// // template <typename T1>
// // [[nodiscard]] decltype(auto) errcheck_null(
// //     T1&& value,
// //     const std::source_location& location = std::source_location::current()
// // ) {
// //     return errcheck_bool(std::forward<T1>(value), location);
// // }


// template <typename T1>
// [[nodiscard]] auto errcheck_invalid_handle(
//     T1 value,
//     const std::source_location& location = std::source_location::current()
// ) {
//     if (value == INVALID_HANDLE_VALUE) {
//         auto code = GetLastError();
//         spdlog::error("code: {}; file: {}; line: {}; column: {}; function: {};", code, location.file_name(), location.line(), location.column(), location.function_name());
//         throw std::runtime_error("Error");
//     }
//     return value;
// }


// template <typename T1>  // TODO: Passthrough specific given codes, without raising an exception.
// [[maybe_unused]] auto errcheck_ntstatus(
//     T1 value,
//     const std::source_location& location = std::source_location::current()
// ) {
//     if (value < 0) {
//         auto code = value;
//         spdlog::error("code: {}; file: {}; line: {}; column: {}; function: {};", code, location.file_name(), location.line(), location.column(), location.function_name());
//         throw std::runtime_error("Error");
//     } else if (value > 0) {

//     }
//     return value;
// }


// template <typename T1>  // TODO: Passthrough specific given codes, without raising an exception.
// [[maybe_unused]] auto errcheck_status(
//     T1 value,
//     const std::source_location& location = std::source_location::current()
// ) {
//     if (!value) {
//         auto code = value;
//         spdlog::error("code: {}; file: {}; line: {}; column: {}; function: {};", code, location.file_name(), location.line(), location.column(), location.function_name());
//         throw std::runtime_error("Error");
//     }
//     return value;
// }


}  // namespace




// template <typename T1>
// using errcheck_null = errcheck_bool<T1>;


// template <typename T1, typename T2>
// [[nodiscard]] auto errcheck(T1 value, T2 invalid_value, std::string error_message = "Error") {
//     if (value == invalid_value) {
//         auto code = GetLastError();
//         spdlog::error(error_message);
//         throw std::runtime_error(error_message);
//     }
//     return value;
// }


// template <typename T1, typename T2>
// using errcheck_null = errcheck<T1, T2>;



// template <typename T1, typename T2, typename T3>
// [[nodiscard]] auto resource_errcheck(T1 value, T2 invalid_value, T3 deleter, std::string error_message = "Error") {
//     if (value == invalid_value) {
//         auto code = GetLastError();
//         spdlog::error(error_message);
//         throw std::runtime_error(error_message);
//     }

//     // []() { CloseHandle(); spdlog::error() }

//     auto resource = sr::unique_resource(
//         value,
//         deleter
//     );

//     // TODO: Default to CloseHandle deleter?

//     return resource;  // Is this correct? Will this move ownership upwards properly?
// }


// auto resource = sr::unique_resource(
//     errcheck_bool(value),
//     deleter
// );


// errcheck_status
// errcheck_bool
// errcheck_nt_status
// errcheck_cr_status
// errcheck_callback
// errcheck_buffer_size
// errcheck_negative
// resource_errcheck_handle_null
// resource_errcheck_handle_invalid
// resource_errcheck_svc_handle
// resource_errcheck_reg_handle
// resource_errcheck_library_handle
// resource_errcheck_find_handle

// errcheck_errno  (Partially capable of checking errors, but unlike "return value wrappers",
//                  it can't set the errno to a sane value before the call.
//                  To do so, a macro, or a more direct function wrapper would probably be required.)
// /OR/
// errcheck_errno_lambda  (Solves the above problem, by calling the lambda, which will return a single value, but giving us the chance to reset errno to 0 in advance.)


// TODO: Errcheck returning unique resource, sharing the invalid_value between both, and allowing lambdas for invalid_value.
// TODO: Version without [[nodiscard]] too
// TODO: Passing multiple arguments to template, for use with spdlog::error. Actually, using a fmt format will be better here, maybe.
// TODO: Sometimes when applicable, return an error code, maybe in a tuple, to be destructured along with a legitimate value, and to avoid certain exceptions.
// TODO: std exception system_error for GetLastError
// TODO: For NTSTATUS, throw an exception on negative codes, and log a message on non-zero positive codes


// Consider using one of the ALIGN_UP* macros in phnt/phnt_ntdef.h
// #define ALIGN_UP(p) ((PVOID)(((ULONG_PTR)p + sizeof(PVOID) - 1) & ~(sizeof(PVOID) - 1)))


// https://github.com/microsoft/wil
