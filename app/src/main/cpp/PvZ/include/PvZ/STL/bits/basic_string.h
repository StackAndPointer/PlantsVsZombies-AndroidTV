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

#ifndef PVZ_STL_BITS_BASIC_STRING_H
#define PVZ_STL_BITS_BASIC_STRING_H

/**
 * @file bits/basic_string.h
 * @see <a href="https://gcc.gnu.org/onlinedocs/gcc-4.9.4/libstdc++/api/a00745.html">basic_string.h File Reference</a>
 * @see <a href="https://gcc.gnu.org/onlinedocs/gcc-16.1.0/libstdc++/api/a00689.html">cow_string.h File Reference</a>
 */

#ifdef PVZ_VERSION
#include "PvZ/MagicNumbers.h"
#endif

#include "PvZ/STL/bits/ranges_base.h"

#include "PvZ/STL/ext/string_conversions.h"

#include <cassert>

#include <atomic>
#include <stdexcept>
#include <string>
#include <type_traits>

#ifdef PVZ_VERSION
extern uintptr_t gLibGameMainBaseAddr;
#endif

namespace pvzstl {

namespace detail {
    template <typename SV, typename CharT>
    concept convertible_to_string_view = std::is_convertible_v<const SV &, std::basic_string_view<CharT>> && !std::is_convertible_v<const SV &, const CharT *>;
} // namespace detail

/**
 * @class basic_string basic_string.h <string>
 * @brief Managing sequences of characters and character-like objects.
 */
template <typename CharT>
class basic_string {
    using sv_type = std::basic_string_view<CharT>;

public:
    using traits_type = std::char_traits<CharT>;
    using value_type = CharT;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = CharT *;
    using const_pointer = const CharT *;
    using reference = CharT &;
    using const_reference = const CharT &;

#ifdef __cpp_lib_ranges_as_const
    using const_iterator = std::basic_const_iterator<const_pointer>;
#else
    using const_iterator = const_pointer;
#endif
    using iterator = const_iterator;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator = const_reverse_iterator;

#ifdef PVZ_VERSION
    static_assert(std::is_same_v<CharT, char> || std::is_same_v<CharT, wchar_t> || std::is_same_v<CharT, char32_t>);
    static_assert(sizeof(size_type) == sizeof(uint32_t));
#else
    static_assert(!std::is_array_v<value_type>, "Character type of basic_string must not be an array");
    static_assert(std::is_standard_layout_v<value_type>, "Character type of basic_string must be standard-layout");
    static_assert(std::is_trivially_default_constructible_v<value_type>, "Character type of basic_string must be trivially default constructible");
    static_assert(std::is_trivially_copyable_v<value_type>, "Character type of basic_string must be trivially copyable");
    static_assert(std::is_same_v<CharT, typename traits_type::char_type>, "traits_type::char_type must be the same type as CharT");
    static_assert(std::is_same_v<typename allocator_type::value_type, value_type>, "Allocator::value_type must be same type as value_type");
#endif

    static constexpr size_type npos = static_cast<size_type>(-1);

    basic_string() noexcept // strengthened
        : m_dataplus{rep::empty_rep().m_data} {}

    basic_string(const basic_string &other)
        : m_dataplus{other.get_rep()->grab()} {}

    basic_string(basic_string &&other) noexcept
        : m_dataplus{other.m_dataplus} {
        other.m_dataplus = rep::empty_rep().m_data;
    }

    basic_string(const basic_string &str, size_type pos, size_type n)
        : m_dataplus{construct(str.c_str() + str.check_range(pos, "basic_string"), str.c_str() + std::min(n, str.size() - pos))} {}

    basic_string(const basic_string &str, size_type pos)
        : m_dataplus{construct(str.c_str() + str.check_range(pos, "basic_string"), str.c_str() + str.size() - pos)} {}

    basic_string(basic_string &&str, size_type pos, size_type n)
        : basic_string(std::move(str.assign(str, pos, n))) {}

    basic_string(basic_string &&str, size_type pos)
        : basic_string(std::move(str.assign(str, pos))) {}

    template <typename SV>
        requires std::is_convertible_v<const SV &, std::basic_string_view<CharT>>
    basic_string(const SV &t, size_type pos, size_type n) {
        const sv_type sv0 = t;
        const sv_type sv = sv0.substr(pos, n);
        m_dataplus = construct(sv.begin(), sv.end());
    }

    template <detail::convertible_to_string_view<CharT> SV>
    explicit basic_string(const SV &t) {
        const sv_type sv = t;
        m_dataplus = construct(sv.begin(), sv.end());
    }

    basic_string(const CharT *s, size_type n)
        : m_dataplus{construct(s, s + n)} {}

    basic_string(const CharT *s)
        : m_dataplus{construct(s, s + (s != nullptr ? traits_type::length(s) : npos))} {}

    basic_string(std::nullptr_t) = delete;

    basic_string(size_type n, CharT c)
        : m_dataplus{construct(n, c)} {}

    template <std::input_iterator InputIt>
    basic_string(InputIt first, InputIt last)
        : m_dataplus{construct(first, last)} {}

    template <detail::container_compatible_range<CharT> Range>
    basic_string(std::from_range_t, Range &&range)
        : m_dataplus{construct(std::ranges::begin(range), std::ranges::end(range))} {}

    basic_string(std::initializer_list<CharT> il)
        : m_dataplus{construct(il.begin(), il.end())} {}

    ~basic_string() {
        get_rep()->dispose();
    }

    /* implicit */ operator sv_type() const noexcept {
        return sv_type{c_str(), size()};
    }

    basic_string &operator=(const basic_string &other) {
        if (get_rep() != other.get_rep()) {
            get_rep()->dispose();
            m_dataplus = other.get_rep()->grab();
        }
        return *this;
    }

    basic_string &operator=(basic_string &&other) noexcept {
        swap(other);
        return *this;
    }

    template <detail::convertible_to_string_view<CharT> SV>
    basic_string &operator=(const SV &t) {
        const sv_type sv = t;
        return assign(sv);
    }

    basic_string &operator=(const CharT *s) {
        return assign(s);
    }

    basic_string &operator=(std::nullptr_t) = delete;

    basic_string &operator=(CharT c) {
        return assign(1, c);
    }

    basic_string &operator=(std::initializer_list<CharT> il) {
        return assign(il);
    }

    basic_string &assign(const basic_string &str, size_type pos, size_type n = npos) {
        str.check_range(pos, "basic_string::assign");
        return assign(str.c_str() + pos, std::min(n, str.size() - pos));
    }

    basic_string &assign(const basic_string &str) {
        return *this = str;
    }

    basic_string &assign(basic_string &&str) noexcept {
        return *this = std::move(str);
    }

    template <detail::convertible_to_string_view<CharT> SV>
    basic_string &assign(const SV &t, size_type pos, size_type n = npos) {
        const sv_type sv = t;
        return assign(sv.substr(pos, n));
    }

    template <detail::convertible_to_string_view<CharT> SV>
    basic_string &assign(const SV &t) {
        const sv_type sv = t;
        return assign(sv.data(), sv.size());
    }

    basic_string &assign(const CharT *s, size_type n) {
        assert((s != nullptr || n == 0) && "basic_string::assign received nullptr");
        check_length(0, n, "basic_string::assign");
        if (disjunct(s) || get_rep()->is_shared()) {
            return replace_safe(0, size(), s, n);
        }
        const size_type pos = static_cast<size_type>(s - c_str());
        if (pos >= n) {
            traits_type::copy(m_dataplus, s, n);
        } else if (pos > 0) {
            traits_type::move(m_dataplus, s, n);
        }
        get_rep()->set_size_and_sharable(n);
        return *this;
    }

    basic_string &assign(const CharT *s) {
        assert((s != nullptr) && "basic_string::assign received nullptr");
        return assign(s, traits_type::length(s));
    }

    basic_string &assign(size_type n, CharT c) {
        return replace_aux(0, size(), n, c);
    }

    basic_string &assign(std::initializer_list<CharT> il) {
        return assign(il.begin(), il.size());
    }

    [[nodiscard]] const CharT &at(size_type pos) const {
        if (pos >= size()) {
            throw std::out_of_range{"basic_string::at"};
        }
        return c_str()[pos];
    }

    [[nodiscard]] CharT &at(size_type pos) {
        if (pos >= size()) {
            throw std::out_of_range{"basic_string::at"};
        }
        leak();
        return m_dataplus[pos];
    }

    // 不提供非 const 重载以满足 noexcept 要求,
    // 需要修改请使用 `at()`.
    [[nodiscard]] const CharT &operator[](size_type pos) const noexcept {
        assert((pos <= size()) && "string index out of bounds");
        return c_str()[pos];
    }

    [[nodiscard]] const CharT &front() const noexcept {
        assert(!empty() && "basic_string::front(): string is empty");
        return *c_str();
    }

    [[nodiscard]] const CharT &back() const noexcept {
        assert(!empty() && "basic_string::back(): string is empty");
        return *(c_str() + size() - 1);
    }

    [[nodiscard]] const CharT *data() const noexcept {
        return m_dataplus;
    }

    [[nodiscard]] const CharT *c_str() const noexcept {
        return m_dataplus;
    }

    [[nodiscard]] const_iterator begin() const noexcept {
        return cbegin();
    }

    [[nodiscard]] const_iterator end() const noexcept {
        return cend();
    }

    [[nodiscard]] const_iterator cbegin() const noexcept {
        return const_iterator{c_str()};
    }

    [[nodiscard]] const_iterator cend() const noexcept {
        return const_iterator{c_str() + size()};
    }

    [[nodiscard]] const_reverse_iterator rbegin() const noexcept {
        return crbegin();
    }

    [[nodiscard]] const_reverse_iterator rend() const noexcept {
        return crend();
    }

    [[nodiscard]] const_reverse_iterator crbegin() const noexcept {
        return const_reverse_iterator{cend()};
    }

    [[nodiscard]] const_reverse_iterator crend() const noexcept {
        return const_reverse_iterator{cbegin()};
    }

    [[nodiscard]] bool empty() const noexcept {
        return size() == 0;
    }

    [[nodiscard]] size_type size() const noexcept {
        return get_rep()->m_size;
    }

    [[nodiscard]] size_type length() const noexcept {
        return get_rep()->m_size;
    }

    [[nodiscard]] constexpr size_type max_size() const noexcept {
        return rep::max_size();
    }

    void reserve(size_type new_cap) {
        const size_type old_cap = capacity();
        if ((new_cap <= old_cap) && !get_rep()->is_shared()) {
            return;
        }
        CharT *tmp = get_rep()->clone(std::max(new_cap, old_cap) - size());
        get_rep()->dispose();
        m_dataplus = tmp;
    }

    [[nodiscard]] size_type capacity() const noexcept {
        return get_rep()->m_capacity;
    }

    void shrink_to_fit() {
        if (capacity() > size() || get_rep()->is_shared()) {
            CharT *tmp = get_rep()->clone();
            get_rep()->dispose();
            m_dataplus = tmp;
        }
    }

    void clear() noexcept {
        if (get_rep()->is_shared()) {
            get_rep()->dispose();
            m_dataplus = rep::empty_rep().m_data;
        } else {
            get_rep()->set_size_and_sharable(0);
        }
    }

    basic_string &insert(size_type pos1, const basic_string &str, size_type pos2, size_type n = npos) {
        str.check_range(pos2, "basic_string::insert");
        return insert(pos1, str.c_str() + pos2, std::min(n, str.size() - pos2));
    }

    basic_string &insert(size_type pos, const basic_string &str) {
        return insert(pos, str.c_str(), str.size());
    }

    template <detail::convertible_to_string_view<CharT> SV>
    basic_string &insert(size_type pos1, const SV &t, size_type pos2, size_type n = npos) {
        const sv_type sv = t;
        return insert(pos1, sv.substr(pos2, n));
    }

    template <detail::convertible_to_string_view<CharT> SV>
    basic_string &insert(size_type pos, const SV &t) {
        const sv_type sv = t;
        return insert(pos, sv.data(), sv.size());
    }

    basic_string &insert(size_type pos, const CharT *s, size_type n) {
        assert((s != nullptr || n == 0) && "basic_string::insert received nullptr");
        check_range(pos, "basic_string::insert");
        check_length(0, n, "basic_string::insert");
        if (disjunct(s) || get_rep()->is_shared()) {
            return replace_safe(pos, 0, s, n);
        }
        const size_type off = static_cast<size_type>(s - c_str());
        mutate(pos, 0, n);
        s = c_str() + off;
        CharT *p = m_dataplus + pos;
        if (s + n <= p) {
            traits_type::copy(p, s, n);
        } else if (s >= p) {
            traits_type::copy(p, s + n, n);
        } else {
            const size_type nleft = p - s;
            traits_type::copy(p, s, nleft);
            traits_type::copy(p + nleft, p + n, n - nleft);
        }
        return *this;
    }

    basic_string &insert(size_type pos, const CharT *s) {
        assert((s != nullptr) && "basic_string::insert received nullptr");
        return insert(pos, s, traits_type::length(s));
    }

    basic_string &insert(size_type pos, size_type n, CharT c) {
        return replace_aux(check_range(pos, "basic_string::insert"), 0, n, c);
    }

    basic_string &erase(size_type pos = 0, size_type n = npos) {
        mutate(check_range(pos, "basic_string::erase"), std::min(n, size() - pos), 0);
        return *this;
    }

    void push_back(CharT c) {
        const size_type len = size() + 1;
        reserve(len);
        m_dataplus[size()] = c;
        get_rep()->set_size_and_sharable(len);
    }

    void pop_back() {
        assert(!empty() && "basic_string::pop_back(): string is already empty");
        erase(size() - 1, 1);
    }

    basic_string &append(const basic_string &str, size_type pos, size_type n = npos) {
        str.check_range(pos, "basic_string::append");
        return append(str.c_str() + pos, std::min(n, str.size() - pos));
    }

    basic_string &append(const basic_string &str) {
        return append(str.c_str(), str.size());
    }

    template <detail::convertible_to_string_view<CharT> SV>
    basic_string &append(const SV &t, size_type pos, size_type n = npos) {
        const sv_type sv = t;
        return append(sv.substr(pos, n));
    }

    template <detail::convertible_to_string_view<CharT> SV>
    basic_string &append(const SV &t) {
        const sv_type sv = t;
        return append(sv.data(), sv.size());
    }

    basic_string &append(const CharT *s, size_type n) {
        assert((s != nullptr || n == 0) && "basic_string::append received nullptr");
        if (n == 0) {
            return *this;
        }
        check_length(0, n, "basic_string::append");
        const size_type len = n + size();
        if (disjunct(s)) {
            reserve(len);
        } else {
            const size_type off = static_cast<size_type>(s - c_str());
            reserve(len);
            s = c_str() + off;
        }
        traits_type::copy(m_dataplus + size(), s, n);
        get_rep()->set_size_and_sharable(len);
        return *this;
    }

    basic_string &append(const CharT *s) {
        assert((s != nullptr) && "basic_string::append received nullptr");
        return append(s, traits_type::length(s));
    }

    basic_string &append(size_type n, CharT c) {
        if (n > 0) {
            check_length(0, n, "basic_string::append");
            const size_type len = n + size();
            reserve(len);
            traits_type::assign(m_dataplus + size(), n, c);
            get_rep()->set_size_and_sharable(len);
        }
        return *this;
    }

    basic_string &append(std::initializer_list<CharT> il) {
        return append(il.begin(), il.size());
    }

    basic_string &operator+=(const basic_string &str) {
        return append(str);
    }

    template <detail::convertible_to_string_view<CharT> SV>
    basic_string &operator+=(const SV &t) {
        const sv_type sv = t;
        return append(sv);
    }

    basic_string &operator+=(const CharT *s) {
        return append(s);
    }

    basic_string &operator+=(CharT c) {
        push_back(c);
        return *this;
    }

    basic_string &operator+=(std::initializer_list<CharT> il) {
        return append(il);
    }

    basic_string &replace(size_type pos1, size_type n1, const basic_string &str, size_type pos2, size_type n2 = npos) {
        str.check_range(pos2, "basic_string::replace");
        return replace(pos1, n1, str.c_str() + pos2, std::min(n2, str.size() - pos2));
    }

    basic_string &replace(size_type pos, size_type n, const basic_string &str) {
        return replace(pos, n, str.c_str(), str.size());
    }

    template <detail::convertible_to_string_view<CharT> SV>
    basic_string &replace(size_type pos1, size_type n1, const SV &t, size_type pos2, size_type n2 = npos) {
        const sv_type sv = t;
        return replace(pos1, n1, sv.substr(pos2, n2));
    }

    template <detail::convertible_to_string_view<CharT> SV>
    basic_string &replace(size_type pos, size_type n, const SV &t) {
        const sv_type sv = t;
        return replace(pos, n, sv.data(), sv.size());
    }

    basic_string &replace(size_type pos, size_type n1, const CharT *s, size_type n2) {
        assert((s != nullptr || n2 == 0) && "basic_string::replace received nullptr");
        check_range(pos, "basic_string::replace");
        n1 = std::min(n1, size() - pos);
        check_length(n1, n2, "basic_string::replace");
        bool left;
        if (disjunct(s) || get_rep()->is_shared()) {
            return replace_safe(pos, n1, s, n2);
        } else if ((left = (s + n2 <= c_str() + pos)) || (c_str() + pos + n1 <= s)) {
            size_type off = static_cast<size_type>(s - c_str());
            if (!left) {
                off += n2 - n1;
            }
            mutate(pos, n1, n2);
            traits_type::copy(m_dataplus + pos, c_str() + off, n2);
            return *this;
        } else {
            const basic_string tmp(s, n2);
            return replace_safe(pos, n1, tmp.c_str(), n2);
        }
    }

    basic_string &replace(size_type pos, size_type n, const CharT *s) {
        assert((s != nullptr) && "basic_string::replace received nullptr");
        return replace(pos, n, s, traits_type::length(s));
    }

    basic_string &replace(size_type pos, size_type n1, size_type n2, CharT c) {
        return replace_aux(check_range(pos, "basic_string::replace"), std::min(n1, size() - pos), n2, c);
    }

    size_type copy(CharT *dest, size_type n, size_type pos = 0) const {
        check_range(pos, "basic_string::copy");
        n = std::min(n, size() - pos);
        assert((dest != nullptr || n == 0) && "basic_string::copy received nullptr");
        if (n > 0) {
            traits_type::copy(dest, c_str() + pos, n);
        }
        return n;
    }

    void resize(size_type n, CharT c) {
        const size_type sz = size();
        check_length(sz, n, "basic_string::resize");
        if (n > sz) {
            append(n - sz, c);
        } else if (n < sz) {
            erase(n);
        }
    }

    void resize(size_type n) {
        resize(n, CharT{});
    }

    template <typename Operation>
    void resize_and_overwrite(size_type n, Operation op) {
        reserve(n);
        CharT *p = m_dataplus;
        struct Terminator {
            ~Terminator() {
                _this->get_rep()->set_size_and_sharable(_r);
            }
            basic_string *_this;
            size_type _r;
        };
        Terminator term{this, 0};
        auto r = std::move(op)(p, n);
        static_assert(std::is_integral_v<decltype(r)>, "resize_and_overwrite operation must return an integer");
        assert((r >= 0) && (size_type(r) <= n));
        term._r = size_type(r);
#if __has_cpp_attribute(assume)
        [[assume(term._r <= n)]];
#endif
    }

    void swap(basic_string &other) noexcept /* strengthened */ {
        if (get_rep()->is_leaked()) {
            get_rep()->set_sharable();
        }
        if (other.get_rep()->is_leaked()) {
            other.get_rep()->set_sharable();
        }
        std::swap(m_dataplus, other.m_dataplus);
    }

    [[nodiscard]] size_type find(const basic_string &str, size_type pos = 0) const noexcept {
        return find(sv_type{str}, pos);
    }

    template <detail::convertible_to_string_view<CharT> SV>
    [[nodiscard]] size_type find(const SV &t, size_type pos = 0) const noexcept(std::is_nothrow_convertible_v<const SV &, sv_type>) {
        const sv_type sv = t;
        return sv_type{*this}.find(sv, pos);
    }

    [[nodiscard]] size_type find(const CharT *s, size_type pos, size_type n) const {
        assert((s != nullptr || n == 0) && "basic_string::find received nullptr");
        return find(sv_type{s, n}, pos);
    }

    [[nodiscard]] size_type find(const CharT *s, size_type pos = 0) const {
        assert((s != nullptr) && "basic_string::find received nullptr");
        return find(sv_type{s}, pos);
    }

    [[nodiscard]] size_type find(CharT c, size_type pos = 0) const noexcept {
        return sv_type{*this}.find(c, pos);
    }

    [[nodiscard]] size_type rfind(const basic_string &str, size_type pos = npos) const noexcept {
        return rfind(sv_type{str}, pos);
    }

    template <detail::convertible_to_string_view<CharT> SV>
    [[nodiscard]] size_type rfind(const SV &t, size_type pos = npos) const noexcept(std::is_nothrow_convertible_v<const SV &, sv_type>) {
        const sv_type sv = t;
        return sv_type{*this}.rfind(sv, pos);
    }

    [[nodiscard]] size_type rfind(const CharT *s, size_type pos, size_type n) const {
        assert((s != nullptr || n == 0) && "basic_string::rfind received nullptr");
        return rfind(sv_type{s, n}, pos);
    }

    [[nodiscard]] size_type rfind(const CharT *s, size_type pos = npos) const {
        assert((s != nullptr) && "basic_string::rfind received nullptr");
        return rfind(sv_type{s}, pos);
    }

    [[nodiscard]] size_type rfind(CharT c, size_type pos = npos) const noexcept {
        return sv_type{*this}.rfind(c, pos);
    }

    [[nodiscard]] size_type find_first_of(const basic_string &str, size_type pos = 0) const noexcept {
        return find_first_of(sv_type{str}, pos);
    }

    template <detail::convertible_to_string_view<CharT> SV>
    [[nodiscard]] size_type find_first_of(const SV &t, size_type pos = 0) const noexcept(std::is_nothrow_convertible_v<const SV &, sv_type>) {
        const sv_type sv = t;
        return sv_type{*this}.find_first_of(sv, pos);
    }

    [[nodiscard]] size_type find_first_of(const CharT *s, size_type pos, size_type n) const {
        assert((s != nullptr || n == 0) && "basic_string::find_first_of received nullptr");
        return find_first_of(sv_type{s, n}, pos);
    }

    [[nodiscard]] size_type find_first_of(const CharT *s, size_type pos = 0) const {
        assert((s != nullptr) && "basic_string::find_first_of received nullptr");
        return find_first_of(sv_type{s}, pos);
    }

    [[nodiscard]] size_type find_first_of(CharT c, size_type pos = 0) const noexcept {
        return find(c, pos);
    }

    [[nodiscard]] size_type find_first_not_of(const basic_string &str, size_type pos = 0) const noexcept {
        return find_first_not_of(sv_type{str}, pos);
    }

    template <detail::convertible_to_string_view<CharT> SV>
    [[nodiscard]] size_type find_first_not_of(const SV &t, size_type pos = 0) const noexcept(std::is_nothrow_convertible_v<const SV &, sv_type>) {
        const sv_type sv = t;
        return sv_type{*this}.find_first_not_of(sv, pos);
    }

    [[nodiscard]] size_type find_first_not_of(const CharT *s, size_type pos, size_type n) const {
        assert((s != nullptr || n == 0) && "basic_string::find_first_not_of received nullptr");
        return find_first_not_of(sv_type{s, n}, pos);
    }

    [[nodiscard]] size_type find_first_not_of(const CharT *s, size_type pos = 0) const {
        assert((s != nullptr) && "basic_string::find_first_not_of received nullptr");
        return find_first_not_of(sv_type{s}, pos);
    }

    [[nodiscard]] size_type find_first_not_of(CharT c, size_type pos = 0) const noexcept {
        return sv_type{*this}.find_first_not_of(c, pos);
    }

    [[nodiscard]] size_type find_last_of(const basic_string &str, size_type pos = npos) const noexcept {
        return find_last_of(sv_type{str}, pos);
    }

    template <detail::convertible_to_string_view<CharT> SV>
    [[nodiscard]] size_type find_last_of(const SV &t, size_type pos = npos) const noexcept(std::is_nothrow_convertible_v<const SV &, sv_type>) {
        const sv_type sv = t;
        return sv_type{*this}.find_last_of(sv, pos);
    }

    [[nodiscard]] size_type find_last_of(const CharT *s, size_type pos, size_type n) const {
        assert((s != nullptr || n == 0) && "basic_string::find_last_of received nullptr");
        return find_last_of(sv_type{s, n}, pos);
    }

    [[nodiscard]] size_type find_last_of(const CharT *s, size_type pos = npos) const {
        assert((s != nullptr) && "basic_string::find_last_of received nullptr");
        return find_last_of(sv_type{s}, pos);
    }

    [[nodiscard]] size_type find_last_of(CharT c, size_type pos = npos) const noexcept {
        return rfind(c, pos);
    }

    [[nodiscard]] size_type find_last_not_of(const basic_string &str, size_type pos = npos) const noexcept {
        return find_last_not_of(sv_type{str}, pos);
    }

    template <detail::convertible_to_string_view<CharT> SV>
    [[nodiscard]] size_type find_last_not_of(const SV &t, size_type pos = npos) const noexcept(std::is_nothrow_convertible_v<const SV &, sv_type>) {
        const sv_type sv = t;
        return sv_type{*this}.find_last_not_of(sv, pos);
    }

    [[nodiscard]] size_type find_last_not_of(const CharT *s, size_type pos, size_type n) const {
        assert((s != nullptr || n == 0) && "basic_string::find_last_not_of received nullptr");
        return find_last_not_of(sv_type{s, n}, pos);
    }

    [[nodiscard]] size_type find_last_not_of(const CharT *s, size_type pos = npos) const {
        assert((s != nullptr) && "basic_string::find_last_not_of received nullptr");
        return find_last_not_of(sv_type{s}, pos);
    }

    [[nodiscard]] size_type find_last_not_of(CharT c, size_type pos = npos) const noexcept {
        return sv_type{*this}.find_last_not_of(c, pos);
    }

    [[nodiscard]] bool starts_with(sv_type sv) const noexcept {
        return sv_type{*this}.starts_with(sv);
    }

    [[nodiscard]] bool starts_with(const CharT *s) const {
        assert((s != nullptr) && "basic_string::starts_with received nullptr");
        return starts_with(sv_type{s});
    }

    [[nodiscard]] bool starts_with(CharT c) const noexcept {
        return !empty() && traits_type::eq(front(), c);
    }

    [[nodiscard]] bool ends_with(sv_type sv) const noexcept {
        return sv_type{*this}.ends_with(sv);
    }

    [[nodiscard]] bool ends_with(const CharT *s) const {
        assert((s != nullptr) && "basic_string::ends_with received nullptr");
        return ends_with(sv_type{s});
    }

    [[nodiscard]] bool ends_with(CharT c) const noexcept {
        return !empty() && traits_type::eq(back(), c);
    }

    [[nodiscard]] bool contains(sv_type sv) const noexcept {
        return sv_type{*this}.contains(sv);
    }

    [[nodiscard]] bool contains(const CharT *s) const {
        assert((s != nullptr) && "basic_string::contains received nullptr");
        return sv_type{*this}.contains(s);
    }

    [[nodiscard]] bool contains(CharT c) const noexcept {
        return sv_type{*this}.contains(c);
    }

    [[nodiscard]] basic_string substr(size_type pos = 0, size_type n = npos) const & {
        return basic_string(*this, check_range(pos, "basic_string::substr"), n);
    }

    [[nodiscard]] basic_string substr(size_type pos = 0, size_type n = npos) && {
        return basic_string(std::move(*this), check_range(pos, "basic_string::substr"), n);
    }

private:
    struct rep {
        // C++ 标准中未明确在静态成员变量的初始化器中自身是否为完整类型 (`sizeof` 运算符需要完整类型),
        // 故将 `max_size` 定义为静态成员函数.
        [[nodiscard]] static consteval size_type max_size() noexcept {
            // npos = (m + 1) * sizeof(CharT) + sizeof(rep)
            constexpr size_type m = ((npos - sizeof(rep)) / sizeof(CharT)) - 1;
            return m / 4;
        }

        [[nodiscard]] static rep &empty_rep() noexcept {
#ifdef PVZ_VERSION
            assert(::gLibGameMainBaseAddr != 0);
            constexpr uintptr_t offset = std::is_same_v<CharT, char> ? /* string */ PVZSTL_STRING_EMPTY_REP : /* basic_string<int> */ PVZSTL_WSTRING_EMPTY_REP;
            return *reinterpret_cast<rep *>(::gLibGameMainBaseAddr + offset);
#else
            alignas(rep) static std::byte empty_rep_storage[sizeof(rep) + sizeof(CharT)] = {};
            return *reinterpret_cast<rep *>(&empty_rep_storage);
#endif
        }

        [[nodiscard]] static rep *create(size_type cap, size_type old_cap) {
            if (cap > max_size()) {
                throw std::length_error{"basic_string::rep::create"};
            }
            constexpr size_type pagesize = 4096;
            constexpr size_type malloc_header_size = 4 * sizeof(void *);
            if ((cap > old_cap) && (cap < 2 * old_cap)) {
                cap = 2 * old_cap;
            }
            size_type size = (cap + 1) * sizeof(CharT) + sizeof(rep);
            const size_type adj_size = size + malloc_header_size;
            if (adj_size > pagesize && cap > old_cap) {
                const size_type extra = pagesize - adj_size % pagesize;
                cap += extra / sizeof(CharT);
                if (cap > max_size()) {
                    cap = max_size();
                }
                size = (cap + 1) * sizeof(CharT) + sizeof(rep);
            }
            void *place = ::operator new(size);
            rep *p = ::new (place) rep;
            p->m_capacity = cap;
            return p;
        }

        [[nodiscard]] CharT *ref_copy() noexcept {
            if (this != &empty_rep()) {
                ++m_ref_count;
            }
            return m_data;
        }

        [[nodiscard]] CharT *clone(size_type res = 0) const {
            rep *r = create(m_size + res, m_capacity);
            if (m_size > 0) {
                traits_type::copy(r->m_data, m_data, m_size);
            }
            r->set_size(m_size);
            return r->m_data;
        }

        [[nodiscard]] CharT *grab() {
            return !is_leaked() ? ref_copy() : clone();
        }

        void destroy() noexcept {
            ::operator delete(this);
        }

        void dispose() noexcept {
            if ((this != &empty_rep()) && (m_ref_count-- <= 0)) {
                destroy();
            }
        }

        // 调用过不清楚是否修改自身数据的成员函数, 如 `operator[]`, `begin()`
        [[nodiscard]] bool is_leaked() const noexcept {
            return m_ref_count < 0;
        }

        [[nodiscard]] bool is_shared() const noexcept {
            return m_ref_count > 0;
        }

        void set_leaked() noexcept {
            m_ref_count = -1;
        }

        void set_sharable() noexcept {
            m_ref_count = 0;
        }

        void set_size(size_type sz) noexcept {
            m_size = sz;
            m_data[sz] = CharT{};
        }

        void set_size_and_sharable(size_type sz) noexcept {
            if (this != &empty_rep()) {
                set_sharable();
                set_size(sz);
            }
        }

        size_type m_size;                  // 字符数
        size_type m_capacity;              // 已分配存储空间中可以容纳的字符数
        std::atomic_int m_ref_count;       // 引用计数 (小于等于 0 时释放内存)
        alignas(size_type) CharT m_data[]; // 作为字符存储的底层数组 (柔性数组成员)
    };

    // NB: This is the special case for Input Iterators, used in
    // istreambuf_iterators, etc.
    // Input Iterators have a cost structure very different from
    // pointers, calling for a different coding style.
    template <std::input_iterator InputIt>
    [[nodiscard]] static CharT *construct(InputIt first, InputIt last) {
        if (first == last) {
            return rep::empty_rep().m_data;
        }
        CharT buf[128];
        size_type len = 0;
        while (first != last && len < std::size(buf)) {
            buf[len++] = *first;
            ++first;
        }
        rep *r = rep::create(len, 0);
        traits_type::copy(r->m_data, buf, len);
        try {
            while (first != last) {
                if (len == r->m_capacity) {
                    // Allocate more space.
                    rep *another = rep::create(len + 1, len);
                    traits_type::copy(another->m_data, r->m_data, len);
                    r->destroy();
                    r = another;
                }
                r->m_data[len++] = *first;
                ++first;
            }
        } catch (...) {
            r->destroy();
            throw;
        }
        r->set_size(len);
        return r->m_data;
    }

    template <std::forward_iterator InputIt>
    [[nodiscard]] static CharT *construct(InputIt first, InputIt last) {
        if (first == last) {
            return rep::empty_rep().m_data;
        }
        // NB: Not required, but considered best practice.
        if constexpr (std::is_pointer_v<InputIt>) {
            if (first == nullptr) {
                throw std::logic_error{"basic_string::construct null not valid"};
            }
        }
        const size_type dnew = static_cast<size_type>(std::distance(first, last));
        // Check for out_of_range and length_error exceptions.
        rep *r = rep::create(dnew, 0);
        try {
            for (CharT *p = r->m_data; first != last; ++first) {
                *p++ = *first;
            }
        } catch (...) {
            r->destroy();
            throw;
        }
        r->set_size(dnew);
        return r->m_data;
    }

    [[nodiscard]] static CharT *construct(size_type n, CharT c) {
        if (n == 0) {
            return rep::empty_rep().m_data;
        }
        rep *r = rep::create(n, 0);
        traits_type::assign(r->m_data, n, c);
        r->set_size(n);
        return r->m_data;
    }

    [[nodiscard]] rep *get_rep() const noexcept {
        return reinterpret_cast<rep *>(m_dataplus) - 1;
    }

    size_type check_range(size_type pos, const char *msg) const {
        if (pos > size()) {
            throw std::out_of_range{msg};
        }
        return pos;
    }

    void check_length(size_type n1, size_type n2, const char *msg) const {
        if (max_size() - (size() - n1) < n2) {
            throw std::length_error{msg};
        }
    }

    [[nodiscard]] bool disjunct(const CharT *s) const noexcept {
        return (s < c_str()) || (c_str() + size() < s);
    }

    // for use in begin() & non-const op[]
    void leak() {
        if (get_rep()->is_leaked() || get_rep() == &rep::empty_rep()) {
            return;
        }
        if (get_rep()->is_shared()) {
            reserve(capacity());
        }
        get_rep()->set_leaked();
    }

    // 清空范围 [ `begin() + pos`, `begin() + pos + len1` ) 中的字符,
    // 并在原位置预留大小为 `len2` 的空间.
    void mutate(size_type pos, size_type len1, size_type len2) {
        const size_type cap = capacity();
        const size_type old_sz = size();
        const size_type new_sz = old_sz + len2 - len1;
        const size_type how_much = old_sz - pos - len1;
        if (new_sz > cap || get_rep()->is_shared()) {
            rep *r = rep::create(new_sz, cap);
            if (pos > 0) {
                traits_type::copy(r->m_data, c_str(), pos);
            }
            if (how_much > 0) {
                traits_type::copy((r->m_data + pos + len2), (c_str() + pos + len1), how_much);
            }
            get_rep()->dispose();
            m_dataplus = r->m_data;
        } else if ((how_much > 0) && (len1 != len2)) {
            traits_type::move((m_dataplus + pos + len2), (c_str() + pos + len1), how_much);
        }
        get_rep()->set_size_and_sharable(new_sz);
    }

    basic_string &replace_aux(size_type pos, size_type n1, size_type n2, CharT c) {
        check_length(n1, n2, "basic_string::replace_aux");
        mutate(pos, n1, n2);
        if (n2 > 0) {
            traits_type::assign(m_dataplus + pos, n2, c);
        }
        return *this;
    }

    basic_string &replace_safe(size_type pos, size_type n1, const CharT *s, size_type n2) {
        mutate(pos, n1, n2);
        if (n2 > 0) {
            traits_type::copy(m_dataplus + pos, s, n2);
        }
        return *this;
    }

    mutable CharT *m_dataplus;
};

template <typename CharT>
[[nodiscard]] bool operator==(const basic_string<CharT> &lhs, const basic_string<CharT> &rhs) noexcept {
    return std::basic_string_view<CharT>{lhs} == std::basic_string_view<CharT>{rhs};
}

template <typename CharT>
[[nodiscard]] bool operator==(const basic_string<CharT> &lhs, const CharT *rhs) {
    return std::basic_string_view<CharT>{lhs} == std::basic_string_view<CharT>{rhs};
}

template <typename CharT>
[[nodiscard]] auto operator<=>(const basic_string<CharT> &lhs, const basic_string<CharT> &rhs) noexcept {
    return std::basic_string_view<CharT>{lhs} <=> std::basic_string_view<CharT>{rhs};
}

template <typename CharT>
[[nodiscard]] auto operator<=>(const basic_string<CharT> &lhs, const CharT *rhs) {
    return std::basic_string_view<CharT>{lhs} <=> std::basic_string_view<CharT>{rhs};
}

template <typename CharT>
[[nodiscard]] basic_string<CharT> operator+(const basic_string<CharT> &lhs, const basic_string<CharT> &rhs) {
    basic_string<CharT> r = lhs;
    r.append(rhs);
    return r;
}

template <typename CharT>
[[nodiscard]] basic_string<CharT> operator+(const CharT *lhs, const basic_string<CharT> &rhs) {
    assert((lhs != nullptr) && "operator+(const CharT *, const basic_string &) received nullptr");
    const auto len = basic_string<CharT>::traits_type::length(lhs);
    basic_string<CharT> r;
    r.reserve(len + rhs.size());
    r.append(lhs, len);
    r.append(rhs);
    return r;
}

template <typename CharT>
[[nodiscard]] basic_string<CharT> operator+(CharT lhs, const basic_string<CharT> &rhs) {
    basic_string<CharT> r;
    r.reserve(1 + rhs.size());
    r.push_back(lhs);
    r.append(rhs);
    return r;
}

template <typename CharT>
[[nodiscard]] basic_string<CharT> operator+(const basic_string<CharT> &lhs, const CharT *rhs) {
    basic_string<CharT> r = lhs;
    r.append(rhs);
    return r;
}

template <typename CharT>
[[nodiscard]] basic_string<CharT> operator+(const basic_string<CharT> &lhs, CharT rhs) {
    basic_string<CharT> r = lhs;
    r.push_back(rhs);
    return r;
}

template <typename CharT>
[[nodiscard]] basic_string<CharT> operator+(basic_string<CharT> &&lhs, const basic_string<CharT> &rhs) {
    return std::move(lhs.append(rhs));
}

template <typename CharT>
[[nodiscard]] basic_string<CharT> operator+(const basic_string<CharT> &lhs, basic_string<CharT> &&rhs) {
    return std::move(rhs.insert(0, lhs));
}

template <typename CharT>
[[nodiscard]] basic_string<CharT> operator+(basic_string<CharT> &&lhs, basic_string<CharT> &&rhs) {
    return std::move(lhs.append(rhs));
}

template <typename CharT>
[[nodiscard]] basic_string<CharT> operator+(const CharT *lhs, basic_string<CharT> &&rhs) {
    return std::move(rhs.insert(0, lhs));
}

template <typename CharT>
[[nodiscard]] basic_string<CharT> operator+(CharT lhs, basic_string<CharT> &&rhs) {
    return std::move(rhs.insert(0, 1, lhs));
}

template <typename CharT>
[[nodiscard]] basic_string<CharT> operator+(basic_string<CharT> &&lhs, const CharT *rhs) {
    return std::move(lhs.append(rhs));
}

template <typename CharT>
[[nodiscard]] basic_string<CharT> operator+(basic_string<CharT> &&lhs, CharT rhs) {
    lhs.push_back(rhs);
    return std::move(lhs);
}

template <typename CharT>
void swap(basic_string<CharT> &lhs, basic_string<CharT> &rhs) noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

using string = basic_string<char>;
using wstring = basic_string<wchar_t>;    // `basic_string<int>` in PvZ
using u32string = basic_string<char32_t>; // `basic_string<int>` in PvZ
#ifndef PVZ_VERSION
using u8string = basic_string<char8_t>;
using u16string = basic_string<char16_t>;
#endif

[[nodiscard]] inline string to_string(int val) {
    return pvzcxx::to_xstring<string, 4 * sizeof(int)>(std::vsnprintf, "%d", val);
}

[[nodiscard]] inline string to_string(unsigned val) {
    return pvzcxx::to_xstring<string, 4 * sizeof(unsigned)>(std::vsnprintf, "%u", val);
}

[[nodiscard]] inline string to_string(long val) {
    return pvzcxx::to_xstring<string, 4 * sizeof(long)>(std::vsnprintf, "%ld", val);
}

[[nodiscard]] inline string to_string(unsigned long val) {
    return pvzcxx::to_xstring<string, 4 * sizeof(unsigned long)>(std::vsnprintf, "%lu", val);
}

[[nodiscard]] inline string to_string(long long val) {
    return pvzcxx::to_xstring<string, 4 * sizeof(long long)>(std::vsnprintf, "%lld", val);
}

[[nodiscard]] inline string to_string(unsigned long long val) {
    return pvzcxx::to_xstring<string, 4 * sizeof(unsigned long long)>(std::vsnprintf, "%llu", val);
}

[[nodiscard]] inline string to_string(float val) {
    return pvzcxx::to_xstring<string, std::numeric_limits<float>::max_exponent10 + 20>(std::vsnprintf, "%f", val);
}

[[nodiscard]] inline string to_string(double val) {
    return pvzcxx::to_xstring<string, std::numeric_limits<double>::max_exponent10 + 20>(std::vsnprintf, "%f", val);
}

[[nodiscard]] inline string to_string(long double val) {
    return pvzcxx::to_xstring<string, std::numeric_limits<long double>::max_exponent10 + 20>(std::vsnprintf, "%Lf", val);
}

[[nodiscard]] inline wstring to_wstring(int val) {
    return pvzcxx::to_xstring<wstring, 4 * sizeof(int)>(std::vswprintf, L"%d", val);
}

[[nodiscard]] inline wstring to_wstring(unsigned val) {
    return pvzcxx::to_xstring<wstring, 4 * sizeof(unsigned)>(std::vswprintf, L"%u", val);
}

[[nodiscard]] inline wstring to_wstring(long val) {
    return pvzcxx::to_xstring<wstring, 4 * sizeof(long)>(std::vswprintf, L"%ld", val);
}

[[nodiscard]] inline wstring to_wstring(unsigned long val) {
    return pvzcxx::to_xstring<wstring, 4 * sizeof(unsigned long)>(std::vswprintf, L"%lu", val);
}

[[nodiscard]] inline wstring to_wstring(long long val) {
    return pvzcxx::to_xstring<wstring, 4 * sizeof(long long)>(std::vswprintf, L"%lld", val);
}

[[nodiscard]] inline wstring to_wstring(unsigned long long val) {
    return pvzcxx::to_xstring<wstring, 4 * sizeof(unsigned long long)>(std::vswprintf, L"%llu", val);
}

[[nodiscard]] inline wstring to_wstring(float val) {
    return pvzcxx::to_xstring<wstring, std::numeric_limits<float>::max_exponent10 + 20>(std::vswprintf, L"%f", val);
}

[[nodiscard]] inline wstring to_wstring(double val) {
    return pvzcxx::to_xstring<wstring, std::numeric_limits<double>::max_exponent10 + 20>(std::vswprintf, L"%f", val);
}

[[nodiscard]] inline wstring to_wstring(long double val) {
    return pvzcxx::to_xstring<wstring, std::numeric_limits<long double>::max_exponent10 + 20>(std::vswprintf, L"%Lf", val);
}

} // namespace pvzstl

template <typename CharT>
struct std::hash<pvzstl::basic_string<CharT>> {
    [[nodiscard]] size_t operator()(const pvzstl::basic_string<CharT> &val) noexcept {
        using StringView = basic_string_view<CharT>;
        return hash<StringView>{}(StringView{val});
    }
};

namespace pvzstl::inline literals::inline string_literals {

[[nodiscard]] inline string operator""_s(const char *str, std::size_t len) {
    return string(str, len);
}

[[nodiscard]] inline wstring operator""_s(const wchar_t *str, std::size_t len) {
    return wstring(str, len);
}

#ifndef PVZ_VERSION
[[nodiscard]] inline u8string operator""_s(const char8_t *str, std::size_t len) {
    return u8string(str, len);
}

[[nodiscard]] inline u16string operator""_s(const char16_t *str, std::size_t len) {
    return u16string(str, len);
}
#endif // PVZ_VERSION

[[nodiscard]] inline u32string operator""_s(const char32_t *str, std::size_t len) {
    return u32string(str, len);
}

} // namespace pvzstl::inline literals::inline string_literals

#endif // PVZ_STL_BITS_BASIC_STRING_H
