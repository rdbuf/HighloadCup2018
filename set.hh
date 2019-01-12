#pragma once

#include <vector>
#include <algorithm>
#include <functional>

/* Our set has the following properties:
    1. Sortedness
    2. Low memory overhead
    3. Fast insertions
   Mostly used for storing integers.
*/

template<class T, class Comparator = std::less<T>>
struct set {
	mutable std::vector<T> elements;
	mutable std::vector<T> removal_queue;
	Comparator cmp;
	bool universe = false;
	mutable bool invariant_holds = true;

	set() = default;
    // set(std::initializer_list<T>&& il) : elements(il.begin(), il.end()) {}
	// set(typename std::vector<T>::iterator it1, typename std::vector<T>::iterator it2) : elements(it1, it2) {}

	void ensure_guarantees() const {
		if (!invariant_holds) {
			std::sort(elements.begin(), elements.end(), cmp);
			elements.erase(std::unique(elements.begin(), elements.end()), elements.end());
		}
		for (auto x : removal_queue) {
      		auto it = std::lower_bound(elements.begin(), elements.end(), x);
      		assert(*it == x);
      		elements.erase(it);
    	}
		elements.shrink_to_fit();
    	removal_queue.clear();
    	removal_queue.shrink_to_fit();
	}

	set& insert(T value) { // invariant assurance is postponed
		if (elements.size() && value < elements.back()) invariant_holds = false;
		if (!elements.size() || value != elements.back()) elements.push_back(value);
		return *this;
	}
	set& remove(T value) { // presence not checked
		removal_queue.push_back(value);
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

	typename decltype(elements)::iterator begin() noexcept { ensure_guarantees(); return elements.begin(); }
	typename decltype(elements)::iterator end() noexcept { ensure_guarantees(); return elements.end(); }
	typename decltype(elements)::const_iterator begin() const noexcept { ensure_guarantees(); return elements.begin(); }
	typename decltype(elements)::const_iterator end() const noexcept { ensure_guarantees(); return elements.end(); }
};
