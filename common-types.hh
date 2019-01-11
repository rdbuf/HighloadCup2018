#pragma once

#include "string.hh"

using Id = uint32_t;
using EpochSecs = int64_t;
enum sex_t { male, female };
enum status_t { single, relationship, undecided };
using email_t = fixwstr<100, 1>;
using domain_t = fixwstr<100, 1>;
using name_t = fixwstr<50, 2>;
struct phone_t {
	uint64_t prefix : 8, code : 16, num_len : 4, num : 36;
};
using country_t = fixwstr<50, 2>;
using city_t = fixwstr<50, 2>;
using interest_t = fixwstr<100, 2>;

using CoolHash = std::hash<std::string_view>::result_type;
CoolHash coolhash(const char* str, size_t n) { return std::hash<std::string_view>{}(std::string_view(str, n)); }

using field_name_t = fixwstr<50, 2>;