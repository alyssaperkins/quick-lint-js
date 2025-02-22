// Copyright (C) 2020  Matthew "strager" Glazar
// See end of file for extended copyright information.

#include <array>
#include <cstring>
#include <quick-lint-js/assert.h>
#include <quick-lint-js/container/c-string-list.h>
#include <quick-lint-js/i18n/locale.h>
#include <quick-lint-js/port/warning.h>
#include <quick-lint-js/util/narrow-cast.h>
#include <string>
#include <vector>

namespace quick_lint_js {
namespace {
constexpr const char locale_part_separators[] = "_.@";

struct Locale_Parts {
  // language, territory, codeset, modifier
  std::array<std::string_view, 4> parts;

  static constexpr int language_index = 0;
  static constexpr int territory_index = 1;
  static constexpr int codeset_index = 2;
  static constexpr int modifier_index = 3;

  std::string_view& language() { return this->parts[this->language_index]; }
};

Locale_Parts parse_locale(const char* locale_name) {
  struct Found_Separator {
    std::size_t length;
    std::size_t which_separator;
  };
  auto find_next_separator = [](const char* c,
                                const char* separators) -> Found_Separator {
    std::size_t length = std::strcspn(c, separators);
    if (c[length] == '\0') {
      return Found_Separator{.length = length,
                             .which_separator = static_cast<std::size_t>(-1)};
    }
    const char* separator = std::strchr(separators, c[length]);
    QLJS_ASSERT(separator);
    return Found_Separator{
        .length = length,
        .which_separator = narrow_cast<std::size_t>(separator - separators)};
  };

  Locale_Parts parts;

  const char* current_separators = &locale_part_separators[0];
  std::string_view* current_part = &parts.language();
  const char* c = locale_name;
  for (;;) {
    Found_Separator part = find_next_separator(c, current_separators);
    *current_part = std::string_view(c, part.length);
    c += part.length;
    if (*c == '\0') {
      break;
    }

    QLJS_ASSERT(part.which_separator != static_cast<std::size_t>(-1));
    current_separators += part.which_separator + 1;
    current_part += part.which_separator + 1;
    c += 1;
  }
  return parts;
}

template <class Func>
void locale_name_combinations(const char* locale_name, Func&& callback);
}

std::optional<int> find_locale(const char* locales, const char* locale_name) {
  // NOTE[locale-list-null-terminator]: The code generator guarantees that there
  // is an empty string at the end of the list (i.e. two null bytes at the end).
  C_String_List_View locales_list(locales);
  std::optional<int> found_entry = std::nullopt;
  locale_name_combinations(locale_name,
                           [&](std::string_view current_locale_name) -> bool {
                             int i = 0;
                             for (std::string_view l : locales_list) {
                               if (l == current_locale_name) {
                                 found_entry = i;
                                 return false;
                               }
                               ++i;
                             }
                             return true;
                           });
  return found_entry;
}

std::vector<std::string> locale_name_combinations(const char* locale_name) {
  std::vector<std::string> locale_names;
  locale_name_combinations(locale_name,
                           [&](std::string_view current_locale) -> bool {
                             locale_names.emplace_back(current_locale);
                             return true;
                           });
  return locale_names;
}

namespace {
QLJS_WARNING_PUSH
QLJS_WARNING_IGNORE_GCC("-Wzero-as-null-pointer-constant")

template <class Func>
void locale_name_combinations(const char* locale_name, Func&& callback) {
  Locale_Parts parts = parse_locale(locale_name);

  std::vector<char> locale;
  std::size_t max_locale_size = std::strlen(locale_name);
  locale.reserve(max_locale_size);
  locale.insert(locale.end(), parts.language().begin(), parts.language().end());

  unsigned present_parts_mask = 0;
  for (std::size_t part_index = 1; part_index < 4; ++part_index) {
    std::string_view part = parts.parts[part_index];
    present_parts_mask |= (part.empty() ? 0U : 1U) << part_index;
  }

  enum {
    TERRITORY = 1 << Locale_Parts::territory_index,
    CODESET = 1 << Locale_Parts::codeset_index,
    MODIFIER = 1 << Locale_Parts::modifier_index,
  };
  // clang-format off
  unsigned char masks[] = {
      TERRITORY | CODESET | MODIFIER,
      TERRITORY           | MODIFIER,
                  CODESET | MODIFIER,
                            MODIFIER,
      TERRITORY | CODESET,
      TERRITORY,
                  CODESET,
      0,
  };
  // clang-format on
  for (unsigned char mask : masks) {
    if ((present_parts_mask & mask) != mask) {
      continue;
    }

    locale.resize(parts.language().size());
    for (std::size_t part_index = 1; part_index < 4; ++part_index) {
      if (mask & (1 << part_index)) {
        std::string_view part = parts.parts[part_index];
        locale.push_back(locale_part_separators[part_index - 1]);
        locale.insert(locale.end(), part.begin(), part.end());
      }
    }
    QLJS_ASSERT(locale.size() <= max_locale_size);

    bool keep_going = callback(std::string_view(locale.data(), locale.size()));
    if (!keep_going) {
      break;
    }
  }
}

QLJS_WARNING_POP
}
}

// quick-lint-js finds bugs in JavaScript programs.
// Copyright (C) 2020  Matthew "strager" Glazar
//
// This file is part of quick-lint-js.
//
// quick-lint-js is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// quick-lint-js is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with quick-lint-js.  If not, see <https://www.gnu.org/licenses/>.
