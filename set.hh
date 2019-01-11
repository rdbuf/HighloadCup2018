#pragma once

template<bool>
struct enriched {};

// template<>
// struct enriched<true> {
// 	field_name_t field_name;
// }

#include <vector>
#include <algorithm>
#include <functional>
template<class T, class Comparator = std::less<T>, bool Enrich = true>
struct set : enriched<Enrich> {
	std::vector<T> elements;
	Comparator cmp;
	bool universe = false;
	bool invariant_holds = true;

	set() = default;
    // set(std::initializer_list<T>&& il) : elements(il.begin(), il.end()) {}
	// set(typename std::vector<T>::iterator it1, typename std::vector<T>::iterator it2) : elements(it1, it2) {}

	set& ensure_guarantees() {
		if (!invariant_holds) {
			std::sort(elements.begin(), elements.end(), cmp);
			elements.erase(std::unique(elements.begin(), elements.end()), elements.end());
			elements.shrink_to_fit();
		}
		return *this;
	}

	// set& insert(T value) {
	// 	ensure_guarantees();
	// 	auto it = std::lower(elements.begin(), elements.end(), value);
	// 	if (*it != value) elements.insert(it, value);
	// 	return *this;
	// }
	set& insert(T value) { // invariant assurance is postponed
		elements.push_back(value);
		invariant_holds = false;
		return *this;
	}

	set& intersect(const set<T>& rhs) {
		assert(elements.size() > rhs.elements.size()); // perf reasons
		if (universe) { *this = rhs; }
		else {
			ensure_guarantees(), rhs.ensure_guarantees();
			auto left_it = rhs.elements.begin(), right_it = rhs.elements.end();
			const auto removed_range_begin = std::remove_if(elements.begin(), elements.end(), [&left_it, &right_it, this](const T& x) {
				left_it = std::lower_bound(left_it, right_it, x, cmp);
				return *left_it != x;
			});
			elements.resize(removed_range_begin - elements.begin());
		}
		return *this;
	}
	set& unite(const set<T>& rhs) {
		if (universe) return *this;
		ensure_guarantees(), rhs.ensure_guarantees();
		elements.reserve(elements.size() + rhs.elements.size());
		auto left_it = elements.begin();
		for (auto it = rhs.elements.begin(); it != rhs.elements.end(); ++it) {
			left_it = std::lower_bound(left_it, elements.end(), *it, cmp);
			if (left_it == elements.end() || *left_it != *it) elements.insert(left_it, *it);
		}
		return *this;
	}

	size_t size() const noexcept { return elements.size(); }

	typename decltype(elements)::iterator begin() noexcept { return elements.begin(); }
	typename decltype(elements)::iterator end() noexcept { return elements.end(); }
	typename decltype(elements)::const_iterator begin() const noexcept { return elements.begin(); }
	typename decltype(elements)::const_iterator end() const noexcept { return elements.end(); }
};
