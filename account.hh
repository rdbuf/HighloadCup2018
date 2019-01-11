#pragma once

#include "set.hh"
#include "like.hh"
#include "common-types.hh"

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
};