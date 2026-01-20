// Copyright (c) 2025 Hygor Ostrowskij de Morais <hygor.o.morais@gmail.com>

#pragma once

#include <hy_windows.h>
#include <ntstatus.h>
#include <phnt.h>
#include <tlhelp32.h>
#include <sddl.h>
#include <spdlog/spdlog.h>
#include "cli.hpp"


namespace hy {


inline void create_symlink(std::string link, std::string target) {
    NTSTATUS ret;

    auto link_name_buffer   = widen(link);
    auto target_name_buffer = widen(target);
    UNICODE_STRING link_name;
    UNICODE_STRING target_name;
    RtlInitUnicodeString(&link_name,   link_name_buffer.data());
    RtlInitUnicodeString(&target_name, target_name_buffer.data());

    OBJECT_ATTRIBUTES link_obj_attrs;

    InitializeObjectAttributes(
        &link_obj_attrs,
        &link_name,
        OBJ_PERMANENT,
        nullptr,
        nullptr
    );

    HANDLE link_handle;
    ret = NtCreateSymbolicLinkObject(
        &link_handle,
        SYMBOLIC_LINK_ALL_ACCESS,
        &link_obj_attrs,
        &target_name
    );

    // Expected errors for NtCreateSymbolicLinkObject
    // STATUS_OBJECT_NAME_COLLISION  // A symlink object with the same link name already exists.
    // STATUS_OBJECT_TYPE_MISMATCH   // A non-symlink object with the same link name already exists.

    if (ret != STATUS_SUCCESS
        && ret != STATUS_OBJECT_NAME_COLLISION
        && ret != STATUS_OBJECT_TYPE_MISMATCH
    ) {
        throw std::runtime_error(fmt::format("NtCreateSymbolicLinkObject error (0x{:x}).", static_cast<ULONG>(ret)));
    }

    NtClose(link_handle);
}


inline void remove_symlink(std::string link) {
    NTSTATUS ret;

    auto link_name_buffer = widen(link);
    UNICODE_STRING link_name;
    RtlInitUnicodeString(&link_name, link_name_buffer.data());

    OBJECT_ATTRIBUTES link_obj_attrs;

    InitializeObjectAttributes(
        &link_obj_attrs,
        &link_name,
        0,
        nullptr,
        nullptr
    );

    HANDLE link_handle;
    ret = NtOpenSymbolicLinkObject(
        &link_handle,
        DELETE,
        &link_obj_attrs
    );
    if (ret == STATUS_OBJECT_NAME_NOT_FOUND) {
        return;  // No symlink to delete (and no handle to close, I think.)
    }
    if (ret < 0) {
        throw std::runtime_error(fmt::format("NtOpenSymbolicLinkObject error (0x{:x}).", static_cast<ULONG>(ret)));
    }

    ret = NtMakeTemporaryObject(link_handle);
    if (ret < 0) {
        throw std::runtime_error(fmt::format("NtMakeTemporaryObject error (0x{:x}).", static_cast<ULONG>(ret)));
    }

    NtClose(link_handle);
}


inline void lockdown_interception_device(int idx) {
    NTSTATUS ret;

    PSECURITY_DESCRIPTOR psd;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:(A;;FA;;;SY)(A;;FA;;;BA)",  // HARDCODED
        SDDL_REVISION_1,
        &psd,
        nullptr
    )) {
        throw std::runtime_error("ConvertStringSecurityDescriptorToSecurityDescriptorW error.");
    }

    auto device_path_buffer = widen(fmt::format("\\Device\\Interception{:02}", idx));
    UNICODE_STRING device_path;
    RtlInitUnicodeString(&device_path, device_path_buffer.data());
    HANDLE hDevice = nullptr;
    IO_STATUS_BLOCK iosb;
    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, &device_path, OBJ_CASE_INSENSITIVE, nullptr, nullptr);

    ret = NtOpenFile(
        &hDevice,
        READ_CONTROL | WRITE_DAC | WRITE_OWNER,
        &oa,
        &iosb,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0
    );
    if (ret < 0) {
        throw std::runtime_error("NtOpenFile error.");
    }

    ret = NtSetSecurityObject(hDevice, DACL_SECURITY_INFORMATION, psd);
    if (ret < 0) {
        throw std::runtime_error("NtSetSecurityObject error.");
    }
}


inline void ensure_debug_privilege() {
    HANDLE hToken = nullptr;
    LUID luid = {};
    TOKEN_PRIVILEGES tp = {};

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) {
        throw std::runtime_error("OpenProcessToken error.");
    }

    if (!LookupPrivilegeValueW(L"", L"SeDebugPrivilege", &luid)) {  // SE_DEBUG_NAME constant has UNICODE-macro-related issues, I think
        throw std::runtime_error("LookupPrivilegeValueW error.");
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken, false, &tp, 0, nullptr, nullptr)) {
        throw std::runtime_error("AdjustTokenPrivileges error.");
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        throw std::runtime_error("AdjustTokenPrivileges error (ERROR_NOT_ALL_ASSIGNED).");
    }
}


inline int real_main(AppMainConfig cfg) {
    if (cfg.lockdown) {
        for (int i = 0; i < cfg.n_max_interception_devices; i++) {
            lockdown_interception_device(i);
        }
    }

    ensure_debug_privilege();

    DWORD pid = 0;
    BOOL hResult;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("CreateToolhelp32Snapshot error.");
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    hResult = Process32FirstW(hSnapshot, &pe);
    while (hResult) {
        // if (wcsicmp(
        //     L"winlogon.exe",  // SYSTEM account is required for the SeCreatePermanentPrivilege privilege.
        //     pe.szExeFile
        // ) == 0) {
        if (boost::iequals(L"winlogon.exe", pe.szExeFile)) {  // SYSTEM account is required for the SeCreatePermanentPrivilege privilege.
            pid = pe.th32ProcessID;
            break;
        }
        hResult = Process32NextW(hSnapshot, &pe);
    }

    CloseHandle(hSnapshot);

    if (!pid) {
        throw std::runtime_error("PID with SeCreatePermanentPrivilege privilege not found.");
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, TRUE, pid);
    if (!hProcess) {
        throw std::runtime_error("OpenProcess error.");
    }
    HANDLE hToken;
    if (!OpenProcessToken(hProcess, TOKEN_IMPERSONATE | TOKEN_DUPLICATE, &hToken)) {
        throw std::runtime_error("OpenProcessToken error.");
    }
    HANDLE hDuplicatedToken;
    if (!DuplicateToken(hToken, SecurityImpersonation, &hDuplicatedToken)) {
        throw std::runtime_error("DuplicateToken error.");
    }
    if (!SetThreadToken(nullptr, hDuplicatedToken)) {
        throw std::runtime_error("SetThreadToken error.");
    }
    // TODO: Close open handles, if any.

    for (int i = 10; i < cfg.n_keyboard_symlinks; i += 10) {
        for (int j = 0; j < 10; j++) {
            auto link   = fmt::format("\\Device\\KeyboardClass{}", i+j);
            auto target = fmt::format("\\Device\\KeyboardClass{}", j);

            spdlog::debug("Symlinking {} to {}", link, target);

            create_symlink(link, target);
        }
    }

    for (int i = 10; i < cfg.n_pointer_symlinks; i += 10) {
        for (int j = 0; j < 10; j++) {
            auto link   = fmt::format("\\Device\\PointerClass{}", i+j);
            auto target = fmt::format("\\Device\\PointerClass{}", j);

            spdlog::debug("Symlinking {} to {}", link, target);

            create_symlink(link, target);
        }
    }

    spdlog::info("Success");

    return 0;
}


}  // namespace
