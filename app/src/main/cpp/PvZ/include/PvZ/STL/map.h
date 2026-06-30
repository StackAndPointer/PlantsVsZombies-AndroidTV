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

#ifndef PVZ_STL_MAP_H
#define PVZ_STL_MAP_H

#include <cstdint>
#include <functional>
#include <utility>

namespace pvzstl {

namespace detail {

    struct rb_tree_node_base {
        std::int32_t mColor;        // +0x00
        rb_tree_node_base *mParent; // +0x04
        rb_tree_node_base *mLeft;   // +0x08
        rb_tree_node_base *mRight;  // +0x0C
    };

    // ARM32 libstdc++ std::_Rb_tree footprint used by the game.
    // The header/end sentinel starts at mHeaderColor. mRoot is the pointer passed
    // to _M_erase(), and mLeftmost is begin().
    struct rb_tree_32 {
        std::uint32_t mCompareOrPadding; // +0x00
        std::int32_t mHeaderColor;       // +0x04
        void *mRoot;                     // +0x08
        void *mLeftmost;                 // +0x0C
        void *mRightmost;                // +0x10
        std::uint32_t mNodeCount;        // +0x14

        void reset() {
            mCompareOrPadding = 0;
            mHeaderColor = 0;
            mRoot = nullptr;
            mLeftmost = &mHeaderColor;
            mRightmost = &mHeaderColor;
            mNodeCount = 0;
        }
    };

    inline rb_tree_node_base *rb_tree_increment(rb_tree_node_base *theNode) {
        if (theNode->mRight != nullptr) {
            theNode = theNode->mRight;

            while (theNode->mLeft != nullptr) {
                theNode = theNode->mLeft;
            }
        } else {
            rb_tree_node_base *aParent = theNode->mParent;

            while (theNode == aParent->mRight) {
                theNode = aParent;
                aParent = aParent->mParent;
            }

            if (theNode->mRight != aParent) {
                theNode = aParent;
            }
        }

        return theNode;
    }

} // namespace detail

template <typename Key, typename T, typename Compare = std::less<Key>>
struct map : detail::rb_tree_32 {
    using key_type = Key;
    using mapped_type = T;
    using key_compare = Compare;
};

template <typename Key, typename Compare = std::less<Key>>
struct set : detail::rb_tree_32 {
    using key_type = Key;
    using key_compare = Compare;
};

static_assert(sizeof(detail::rb_tree_node_base) == 0x10);
static_assert(sizeof(detail::rb_tree_32) == 0x18);

} // namespace pvzstl

#endif // PVZ_STL_MAP_H
