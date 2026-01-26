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
    spdlog::debug("Symlinking {} to {}", link, target);

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
    spdlog::debug("Removing symlink {}", link);

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
    if (ret == STATUS_OBJECT_NAME_NOT_FOUND  // No symlink to delete
        || ret == STATUS_OBJECT_TYPE_MISMATCH  // Not a symlink
    ) {
        return;
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


inline void set_interception_device_permissions(int idx, bool lockdown) {
    NTSTATUS ret;

    constexpr auto standard_sddl = "D:(A;;FRFW;;;WD)(A;;FR;;;RC)(A;;FA;;;SY)(A;;FA;;;BA)";
    constexpr auto lockdown_sddl = "D:(A;;FA;;;SY)(A;;FA;;;BA)";

    auto sddl = lockdown ? lockdown_sddl : standard_sddl;

    PSECURITY_DESCRIPTOR psd;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
        widen(sddl).data(),
        SDDL_REVISION_1,
        &psd,
        nullptr
    )) {
        throw std::runtime_error("ConvertStringSecurityDescriptorToSecurityDescriptorW error.");
    }

    auto device_path_str = fmt::format("\\Device\\Interception{:02}", idx);
    auto device_path_buffer = widen(device_path_str);
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

    spdlog::debug("Setting {} SDDL to {}", device_path_str, sddl);
}


inline int real_main(AppMainConfig cfg) {
    spdlog::info("Lockdown mode: {}", cfg.lockdown ? "enabled" : "disabled");
    for (int i = 0; i < cfg.n_max_interception_devices; i++) {
        set_interception_device_permissions(i, cfg.lockdown);
    }

    for (int i = 10; i < cfg.n_keyboard_symlinks; i += 10) {
        for (int j = 0; j < 10; j++) {
            auto link   = fmt::format("\\Device\\KeyboardClass{}", i+j);
            auto target = fmt::format("\\Device\\KeyboardClass{}", j);

            create_symlink(link, target);
        }
    }

    for (int i = 10; i < cfg.n_pointer_symlinks; i += 10) {
        for (int j = 0; j < 10; j++) {
            auto link   = fmt::format("\\Device\\PointerClass{}", i+j);
            auto target = fmt::format("\\Device\\PointerClass{}", j);

            create_symlink(link, target);
        }
    }

    spdlog::info("Success");

    return 0;
}


}  // namespace
