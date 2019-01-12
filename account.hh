#pragma once

#include "set.hh"
#include "like.hh"
#include "common-types.hh"
#include "globals.hh"

#define FMT_STRING_ALIAS 1
#include <fmt/format.h>
#include <fmt/ostream.h>
#undef FMT_STRING_ALIAS

#include <bitset>

#include <optional>
#include <string_view>

struct Account { // size: 352
	email_t email;
	set<Like> likes;
	set<Id> likers;
	set<CoolHash> interest_idcs;
	std::optional<CoolHash> fname_idx, sname_idx;
	std::optional<CoolHash> country_idx;
	std::optional<CoolHash> city_idx;
	std::optional<phone_t> phone;
	std::optional<EpochSecs> premium_start, premium_end;
	EpochSecs birth;
	EpochSecs joined;
	Id id;
	status_t status;
	sex_t sex;

	enum {
		print_fname,
		print_sname,
		print_country,
		print_city,
		print_phone,
		print_premium,
		print_birth,
		print_joined,
		print_status,
		print_sex,
		printable_n
	};

	constexpr static const char* status_str[] = {"свободны", "заняты", "всё сложно"};

	template<class OutputIt>
	void serialize_to(OutputIt dest, std::bitset<printable_n> fields) { // no checks performed
		fmt::format_to(dest, fmt(R"({{"email":"{:s}",)"), email);
		if (fields[print_fname]) fmt::format_to(dest, fmt(R"("fname":"{:s}",)"), fnames[*fname_idx]);
		if (fields[print_sname]) fmt::format_to(dest, fmt(R"("sname":"{:s}",)"), snames[*sname_idx]);
		if (fields[print_country]) fmt::format_to(dest, fmt(R"("country":"{:s}",)"), countries[*country_idx]);
		if (fields[print_city]) fmt::format_to(dest, fmt(R"("city":"{:s}",)"), cities[*city_idx]);
		if (fields[print_phone]) fmt::format_to(dest, fmt(R"("phone":"{0:d}({1:d}){3:{2}d}",)"), phone->prefix, phone->code, phone->num_len, phone->num);
		if (fields[print_premium]) fmt::format_to(dest, fmt(R"("premium":{{"finish":{:d},"start":{:d}}},)"), *premium_end, *premium_start);
		if (fields[print_birth]) fmt::format_to(dest, fmt(R"("birth":{:d},)"), birth);
		if (fields[print_joined]) fmt::format_to(dest, fmt(R"("joined":{:d},)"), joined);
		if (fields[print_status]) fmt::format_to(dest, fmt(R"("status":"{:s}",)"), status_str[status]);
		if (fields[print_sex]) fmt::format_to(dest, fmt(R"("sex":"{:c}",)"), "mf"[sex]);
		fmt::format_to(dest, fmt(R"("id":{:d}}})"), id);
	}
};