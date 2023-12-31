//
// Created by Andrej Redeky.
// Copyright © 2015-2023 Feldarian Softworks. All rights reserved.
// SPDX-License-Identifier: EUPL-1.2
//

#pragma once

#include "Common.hpp"

#include "GlyphRangesBuilder.hpp"

namespace Feldarian::Base::UTF
{

class LocalizationInstance
{
public:
  inline static const String8 Empty;

  LocalizationInstance(GlyphRangesBuilder& glyphRangesBuilder)
    : glyphRangesBuilder(glyphRangesBuilder)
  { }

  void Clear()
  {
    localizationMap.clear();
  }

  bool Load(const toml::table &localizationTable)
  {
    for (const auto &[key, value] : localizationTable)
    {
      if (!value.is_string())
        return false;
    }

    for (const auto &[key, value] : localizationTable)
    {
      assert(value.is_string());

      String8CI keyUTF{key.str()};
      String8 valueUTF{**value.as_string()};

      glyphRangesBuilder.AddText(keyUTF);
      glyphRangesBuilder.AddText(valueUTF);

      localizationMap.insert_or_assign(std::move(keyUTF), std::move(valueUTF));
    }

    return true;
  }

  const String8 &Localize(const StringView8CI &key) const
  {
    const auto localizationIt = localizationMap.find(key);
    if (localizationIt == localizationMap.end())
      return Empty;

    return localizationIt->second;
  }

  template <typename UTFCharType, bool CaseSensitive = false, typename UTFCharTraits = std::char_traits<UTFCharType>>
  const String8 &Localize(const StringViewWrapper<UTFCharType, CaseSensitive, UTFCharTraits> key) const
  {
    const auto localizationIt = localizationMap.find(key);
    if (localizationIt == localizationMap.end())
      return Empty;

    return localizationIt->second;
  }

  template <typename... Args>
    requires StringViewConstructible<Args...>
  const String8 &Localize(const Args &...args) const
  {
    if constexpr (StringView8Constructible<Args...>)
      return Localize(StringView8CI(args...));
    else if constexpr (StringView16Constructible<Args...>)
      return Localize(StringView16CI(args...));
    else if constexpr (StringView32Constructible<Args...>)
      return Localize(StringView32CI(args...));
  }

private:
  OrderedMap<String8CI, String8> localizationMap;
  GlyphRangesBuilder& glyphRangesBuilder;
};

class LocalizationManager
{
public:
  LocalizationManager(GlyphRangesBuilder& glyphRangesBuilder)
    : glyphRangesBuilder(glyphRangesBuilder)
  { }

  bool LoadLocalization(const StringView8CI &localizationPathView)
  {
    const auto localizationPath = localizationPathView.path();
    if (!exists(localizationPath))
      return false;

    const auto localizationFile = toml::parse_file(localizationPathView.native());
    if (localizationFile.failed())
      return false;

    String8CI languageName(localizationPath.stem());
    const auto languageNameSeparatorPosition = languageName.native().rfind('_');
    if (languageNameSeparatorPosition != String8CI::npos)
      languageName = languageName.native().substr(languageNameSeparatorPosition + 1);

    const auto languageGroup = localizationFile["language"];
    if (languageGroup.is_table())
    {
      const auto &languageNameValue = languageGroup["name"];
      if (languageNameValue.is_string())
        languageName = **languageNameValue.as_string();
    }

    if (languageName.empty())
      return false;

    glyphRangesBuilder.AddText(languageName);

    const auto localizationTableValue = localizationFile["localization"];
    if (!localizationTableValue.is_table())
      return false;

    const auto localizationTable = *localizationTableValue.as_table();

    std::lock_guard lock(dataMutex);

    const auto [localizationInstanceIt, localizationInstanceEmplaced] =
        localizationInstancesMap.try_emplace(languageName, LocalizationInstance{glyphRangesBuilder});
    const auto localizationLoaded = localizationInstanceIt->second.Load(localizationTable);

    if (localizationInstanceEmplaced)
    {
      if (!localizationLoaded)
        localizationInstancesMap.erase(localizationInstanceIt);
    }

    return localizationLoaded;
  }

  std::vector<String8CI> GetAvailableLanguages() const
  {
    std::shared_lock lock(dataMutex);

    const auto languagesRange = localizationInstancesMap | ranges::views::keys;
    return {languagesRange.begin(), languagesRange.end()};
  }

  bool SetDefaultLanguage(const StringView8CI &language)
  {
    if (language.empty())
      return false;

    std::lock_guard lock(dataMutex);

    const auto localizationInstanceIt = localizationInstancesMap.find(language);
    if (localizationInstanceIt == localizationInstancesMap.cend())
      return false;

    defaultLocalizationLanguage = localizationInstanceIt->first;
    defaultLocalizationInstance = &localizationInstanceIt->second;
    return true;
  }

  String8CI GetDefaultLanguage() const
  {
    std::shared_lock lock(dataMutex);

    return defaultLocalizationLanguage;
  }

  bool SetLanguage(const StringView8CI &language)
  {
    if (language.empty())
      return false;

    std::lock_guard lock(dataMutex);

    const auto localizationInstanceIt = localizationInstancesMap.find(language);
    if (localizationInstanceIt == localizationInstancesMap.cend())
      return false;

    localizationLanguage = localizationInstanceIt->first;
    localizationInstance = &localizationInstanceIt->second;
    return true;
  }

  String8CI GetLanguage() const
  {
    std::shared_lock lock(dataMutex);

    return localizationLanguage.empty() ? defaultLocalizationLanguage : localizationLanguage;
  }

  template <typename... Args>
    requires StringViewConstructible<Args...> && !(StringView8Constructible<Args...>)
  String8 Localize(const Args &...args) const
  {
    std::shared_lock lock(dataMutex);
    const auto& localized = LocalizeInternal(args...);
    return localized.empty() ? String8(args...) : localized;
  }

  template <typename... Args>
    requires StringView8Constructible<Args...>
  StringView8 Localize(const Args &...args) const
  {
    std::shared_lock lock(dataMutex);
    const auto& localized = LocalizeInternal(args...);
    return localized.empty() ? StringView8(args...) : StringView8(localized);
  }

#if FBL_VENDOR_USE_STD_FORMAT || FBL_VENDOR_USE_FMT
  template <typename Type, typename... FormatArgs>
    requires StringViewConstructible<Type>
  String8 &LocalizeFormatTo(String8 &buffer, const Type &key, FormatArgs &&...args) const
  {
    std::shared_lock lock(dataMutex);

    const auto &localizedFormat = LocalizeInternal(key);
    if (localizedFormat.empty())
      return buffer = key;

    return FormatTo(buffer, localizedFormat,
                    std::forward<FormatArgs>(args)...);
  }

  template <typename Type, typename... FormatArgs>
    requires StringViewConstructible<Type>
  String8 LocalizeFormat(const Type &key, FormatArgs &&...args) const
  {
    String8 result;
    LocalizeFormatTo(result, key, std::forward<FormatArgs>(args)...);
    return result;
  }
#endif

private:

  template <typename... Args>
    requires StringViewConstructible<Args...>
  const String8 &LocalizeInternal(const Args &...args) const
  {
    const auto &localized =
        localizationInstance ? localizationInstance->Localize(args...) : LocalizationInstance::Empty;
    if (!localized.empty())
      return localized;

    const auto &defaultLocalized =
        defaultLocalizationInstance ? defaultLocalizationInstance->Localize(args...) : LocalizationInstance::Empty;
    if (!defaultLocalized.empty())
      return defaultLocalized;

    assert(!defaultLocalized.empty());
    return LocalizationInstance::Empty;
  }

  OrderedMap<String8CI, LocalizationInstance, std::less<>> localizationInstancesMap;
  StringView8CI defaultLocalizationLanguage;
  LocalizationInstance *defaultLocalizationInstance = nullptr;
  StringView8CI localizationLanguage;
  LocalizationInstance *localizationInstance = nullptr;

  mutable std::shared_mutex dataMutex;

  GlyphRangesBuilder& glyphRangesBuilder;
};

} // namespace Feldarian::Base::UTF
