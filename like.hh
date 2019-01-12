#pragma once

#include "common-types.hh"

struct Like {
	uint32_t net_ts;
	Id other;
	uint8_t num_tss;

	bool operator<(Like rhs) const noexcept { return other < rhs.other; }
	bool operator==(Like rhs) const noexcept { return other == rhs.other; }
	Like& operator+=(Like rhs) noexcept { net_ts += rhs.net_ts; num_tss += rhs.num_tss; return *this; }
};

#include "set.hh"
template<>
void set<Like>::ensure_guarantees() const {
	if (invariant_holds) return;
	std::sort(elements.begin(), elements.end());
	auto it1 = elements.begin(); const auto it2 = elements.end();
	while (it1 != it2) {
		auto r = std::equal_range(it1, it2, *it1).second;
		for (auto inner_it = it1 + 1; inner_it != r; ++inner_it) {
			*it1 += *inner_it;
			inner_it->other = 0;
		}
		elements.erase(std::remove(elements.begin(), elements.end(), Like{0, 0, 0}), elements.end());
		it1 = r;
	}
}
template<>
set<Like>& set<Like>::insert(Like value) {
	invariant_holds = false;
	elements.push_back(value);
	return *this;
}