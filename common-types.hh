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
	operator uint64_t() const { return *reinterpret_cast<const uint64_t*>(this); }
	bool operator==(phone_t rhs) const { return (uint64_t)*this == (uint64_t)rhs; }
};
using country_t = fixwstr<50, 2>;
using city_t = fixwstr<50, 2>;
using interest_t = fixwstr<100, 2>;
struct Like {
	uint32_t ts;
	Id likee;

	bool operator<(Like rhs) const { return likee < rhs.likee || likee == rhs.likee && ts < rhs.ts; }
	bool operator==(Like rhs) const { return likee == rhs.likee && ts == rhs.ts; }
	bool operator!=(Like rhs) const { return likee != rhs.likee || ts != rhs.ts; }
};

using CoolHash = std::hash<std::string_view>::result_type;
CoolHash coolhash(const char* str, size_t n) { return std::hash<std::string_view>{}(std::string_view(str, n)); }

using field_name_t = fixwstr<50, 2>;