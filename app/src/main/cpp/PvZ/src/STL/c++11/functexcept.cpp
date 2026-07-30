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

#include "PvZ/STL/bits/c++config.h"
#include "PvZ/STL/bits/stdexcept_throwfwd.h"

#include <stdexcept>

void pvzstl::detail::throw_logic_error(const char *msg) {
    PVZSTL_THROW_OR_ABORT(std::logic_error(msg));
}

void pvzstl::detail::throw_domain_error(const char *msg) {
    PVZSTL_THROW_OR_ABORT(std::domain_error(msg));
}

void pvzstl::detail::throw_invalid_argument(const char *msg) {
    PVZSTL_THROW_OR_ABORT(std::invalid_argument(msg));
}

void pvzstl::detail::throw_length_error(const char *msg) {
    PVZSTL_THROW_OR_ABORT(std::length_error(msg));
}

void pvzstl::detail::throw_out_of_range(const char *msg) {
    PVZSTL_THROW_OR_ABORT(std::out_of_range(msg));
}

void pvzstl::detail::throw_out_of_range_fmt(const char *msg, ...) {
    throw_out_of_range(msg);
}

void pvzstl::detail::throw_range_error(const char *msg) {
    PVZSTL_THROW_OR_ABORT(std::range_error(msg));
}

void pvzstl::detail::throw_overflow_error(const char *msg) {
    PVZSTL_THROW_OR_ABORT(std::overflow_error(msg));
}

void pvzstl::detail::throw_underflow_error(const char *msg) {
    PVZSTL_THROW_OR_ABORT(std::underflow_error(msg));
}
