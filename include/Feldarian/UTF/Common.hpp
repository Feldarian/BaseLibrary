//
// Created by Andrej Redeky.
// Copyright © 2015-2023 Feldarian Softworks. All rights reserved.
// SPDX-License-Identifier: EUPL-1.2
//

#pragma once

// TODO - these should not be in the header! including ICU...
#include <toml++/toml.h>
#include <unicode/uchar.h>
#include <unicode/ustring.h>
#include <xxhash.h>

#if FBL_VENDOR_USE_STD_FORMAT
#include <format>
#endif

#if FBL_VENDOR_USE_FMT
#include <fmt/core.h>
#endif

#if FBL_VENDOR_USE_RANGE_V3
#include <range/v3/all.hpp>
#elif FBL_VENDOR_USE_STD_RANGES
#include <ranges>

namespace ranges
{

using namespace std::ranges;

}
#endif

#include <cassert>
#include <cstdint>
#include <bitset>
#include <compare>
#include <concepts>
#include <filesystem>
#include <map>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>

namespace Feldarian::Base::UTF
{

template <typename TypeInput, typename... TypesToCompareWith>
concept IsAnyOfTypes = (std::same_as<TypeInput, TypesToCompareWith> || ...);

template <typename UTFCharType>
concept IsUTFBasicCharType = IsAnyOfTypes<UTFCharType, char, wchar_t>;

template <typename UTFCharType>
concept IsUTF8CharType = IsAnyOfTypes<UTFCharType, char, char8_t, int8_t, uint8_t>;

static_assert(std::same_as<char16_t, UChar>, "ICU UChar typedef is not char16_t!");

#ifdef _WIN32

template <typename UTFCharType>
concept IsUTF16CharType = IsAnyOfTypes<UTFCharType, char16_t, int16_t, uint16_t, UChar, wchar_t>;

template <typename UTFCharType>
concept IsUTF32CharType = IsAnyOfTypes<UTFCharType, char32_t, int32_t, uint32_t, UChar32>;

#else // _WIN32

static_assert(std::same_as<wchar_t, UChar32>, "STD wchar_t had unexpected size!");

template <typename UTFCharType>
concept IsUTF16CharType = IsAnyOfTypes<UTFCharType, char16_t, int16_t, uint16_t, UChar>;

template <typename UTFCharType>
concept IsUTF32CharType = IsAnyOfTypes<UTFCharType, char32_t, int32_t, uint32_t, UChar32, wchar_t>;

#endif // _WIN32

template <typename UTFCharType>
concept IsUTFCharType = IsUTF8CharType<UTFCharType> || IsUTF16CharType<UTFCharType> || IsUTF32CharType<UTFCharType> || IsUTFBasicCharType<UTFCharType>;

template <typename UTFCharTypeLeft, typename UTFCharTypeRight>
concept IsSameUTFCharType = IsUTFCharType<UTFCharTypeLeft> && IsUTFCharType<UTFCharTypeRight>
	&& IsUTF8CharType<UTFCharTypeLeft> == IsUTF8CharType<UTFCharTypeRight>
	&& IsUTF16CharType<UTFCharTypeLeft> == IsUTF16CharType<UTFCharTypeRight>
	&& IsUTF32CharType<UTFCharTypeLeft> == IsUTF32CharType<UTFCharTypeRight>;

static constexpr auto CodepointInvalid{ 0ul };
static constexpr auto CodepointMax{ 0x10FFFFul };

}

template <typename KeyType, typename ValueType, typename ComparatorLessType = std::less<>, typename AllocatorType = std::allocator<std::pair<const KeyType, ValueType>>>
using OrderedMap = std::map<KeyType, ValueType, ComparatorLessType, AllocatorType>;

template <typename ValueType, typename ComparatorLessType = std::less<>, typename AllocatorType = std::allocator<ValueType>>
using OrderedSet = std::set<ValueType, ComparatorLessType, AllocatorType>;

template <typename ValueType>
using OptionalReference = std::optional<std::reference_wrapper<ValueType>>;
