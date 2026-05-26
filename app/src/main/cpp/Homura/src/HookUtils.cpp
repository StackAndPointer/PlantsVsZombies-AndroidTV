/*
 * Copyright (C) 2023-2026  PvZ TV Touch Team
 *
 * This file is part of PlantsVsZombies-AndroidTV.
 *
 * PlantsVsZombies-AndroidTV is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * PlantsVsZombies-AndroidTV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * PlantsVsZombies-AndroidTV.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "Homura/HookUtils.h"
#include "Homura/Logger.h"
#include "Homura/MemoryUtils.h"

#include "SubstrateHook.h"

#include <cassert>
#include <cerrno>
#include <cstring>

#include <stdexcept>

// https://itanium-cxx-abi.github.io/cxx-abi/abi.html#member-function-pointers
struct homura::details::CppMemFuncPtr {
    void *ptr;
    std::ptrdiff_t adj;
};

void homura::details::CheckVirtualFunc(const CppMemFuncPtr *ptr) {
#if defined(__arm__) || defined(__aarch64__)
    // https://github.com/ARM-software/abi-aa/blob/main/cppabi32/cppabi32.rst#representation-of-pointer-to-member-function
    // https://github.com/ARM-software/abi-aa/blob/main/cppabi64/cppabi64.rst#representation-of-pointer-to-member-function
    const bool isVirtual = ptr->adj & 1;
#else
    const bool isVirtual = std::uintptr_t(ptr->ptr) & 1;
#endif

    if (isVirtual) [[unlikely]] {
        throw std::logic_error{"Cannot use a virtual function to hook"};
    }
}

void homura::details::HookFuncImpl(void *symbol, void *newFunc, void **oldFuncAddr) {
    assert((symbol != nullptr) && (newFunc != nullptr));
    MSHookFunction(symbol, newFunc, oldFuncAddr);
}

bool homura::details::HookVirtualFuncImpl(void *vTableSymbol, std::size_t index, void *newFunc, void **oldFuncAddr) {
    assert((vTableSymbol != nullptr) && (newFunc != nullptr));

    auto funcPtrAddr = reinterpret_cast<void **>(vTableSymbol) + index;
    if (!SetProtection(std::uintptr_t(funcPtrAddr), sizeof(funcPtrAddr), PROT_READ | PROT_WRITE)) {
        LOG_ERROR("Failed to set protection: {}", std::strerror(errno));
        return false;
    }

    if (oldFuncAddr != nullptr) {
        *oldFuncAddr = *funcPtrAddr;
    }
    *funcPtrAddr = newFunc;

    // Keep writable after patching: our custom vtables may share a page,
    // forcing RO here can unexpectedly make sibling tables/object memory RO.
    SetProtection(std::uintptr_t(funcPtrAddr), sizeof(funcPtrAddr), PROT_READ | PROT_WRITE);
    return true;
}

bool homura::details::HookPltFuncImpl(std::string_view libName, std::uintptr_t offset, void *newFunc, void **oldFuncAddr) {
    assert(offset > 0);
    assert(newFunc != nullptr);

    const std::uintptr_t baseAddr = GetLibBaseAddr(libName);
    if (baseAddr == 0) [[unlikely]] {
        LOG_ERROR("Failed to get base address of {:?}", libName);
        return false;
    }

    auto funcPtrAddr = reinterpret_cast<void **>(baseAddr + offset);
    if (!SetProtection(std::uintptr_t(funcPtrAddr), sizeof(funcPtrAddr), PROT_READ | PROT_WRITE)) {
        LOG_ERROR("Failed to set protection: {}", std::strerror(errno));
        return false;
    }

    if (oldFuncAddr != nullptr) {
        *oldFuncAddr = *funcPtrAddr;
    }
    *funcPtrAddr = newFunc;

    SetProtection(std::uintptr_t(funcPtrAddr), sizeof(funcPtrAddr), PROT_READ);
    return true;
}
