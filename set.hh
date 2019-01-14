#pragma once

#include <vector>
#include <algorithm>
#include <functional>
#include <mutex>

/* Our set has the following properties:
    1. Sortedness
    2. Low memory overhead
    3. Fast insertions
   Mostly used for storing integers.
*/

/* Thread safety:
 *  ctors, assignment ops: rhs is synchronized
 *  insert, remove: synchronized
 *  intersect, unite: right operand is synchronized
 *  ensure_guarantees: unsafe
 *  size: synchronized
 *  begin, end: unsafe
*/

template<class T, class Comparator = std::less<T>>
struct set {
	mutable std::vector<T> elements;
	mutable std::vector<T> removal_queue;
	Comparator cmp;
	bool universe = false;
	mutable bool invariant_holds = true;
	mutable std::mutex internal_mutex;

	set() = default;
	set(const set<T>& other, const std::scoped_lock<std::mutex>&) :
		elements(other.elements),
		removal_queue(other.removal_queue),
		cmp(other.cmp),
		universe(other.universe),
		invariant_holds(other.invariant_holds) {}
	set(const set<T>& other) : set(other, std::scoped_lock<std::mutex>(other.internal_mutex)) {}
	set(set<T>&& other, const std::scoped_lock<std::mutex>&) :
		elements(std::move(other.elements)),
		removal_queue(std::move(other.removal_queue)),
		cmp(std::move(other.cmp)),
		universe(other.universe),
		invariant_holds(other.invariant_holds) {}
	set(set<T>&& other) : set(std::move(other), std::scoped_lock<std::mutex>(other.internal_mutex)) {}

	set& operator=(const set<T>& rhs) {
		if (&rhs == this) return *this;
		std::scoped_lock<std::mutex> lock(rhs.internal_mutex);
		if (rhs.universe) elements.clear(), removal_queue.clear();
		else elements = rhs.elements, removal_queue = rhs.removal_queue;
		cmp = rhs.cmp;
		universe = rhs.universe;
		invariant_holds = rhs.invariant_holds;
	}
	set& operator=(set<T>&& rhs) {
		if (&rhs == this) return *this;
		std::scoped_lock<std::mutex> lock(rhs.internal_mutex);
		if (rhs.universe) elements.clear(), removal_queue.clear();
		else elements = std::move(rhs.elements), removal_queue = std::move(rhs.removal_queue);
		cmp = std::move(rhs.cmp);
		universe = rhs.universe;
		invariant_holds = rhs.invariant_holds;
	}

	void ensure_guarantees() const {
		/* Schematically:
		 * we enter with shared_lock
		 * if a modification needed,
		 * and it is not being currently performed,
		 * wait until it's done
		*/

		if (!invariant_holds) {
			std::sort(elements.begin(), elements.end(), cmp);
			elements.erase(std::unique(elements.begin(), elements.end()), elements.end());
			invariant_holds = true;
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
		std::scoped_lock lk(internal_mutex);
		if (elements.size() && value < elements.back()) invariant_holds = false;
		if (!elements.size() || value != elements.back()) elements.push_back(value);
		return *this;
	}
	set& remove(T value) { // presence not checked
		std::scoped_lock lk(internal_mutex);
		removal_queue.push_back(value);
		return *this;
	}

	set& intersect(const set<T>& rhs) {
		assert(elements.size() <= rhs.elements.size()); // perf reasons
		if (universe) { *this = rhs; }
		else {
			std::scoped_lock lk(rhs.internal_mutex);
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
		std::scoped_lock lk(rhs.internal_mutex);
		ensure_guarantees(), rhs.ensure_guarantees();
		elements.reserve(elements.size() + rhs.elements.size());
		auto left_it = elements.begin();
		for (auto it = rhs.elements.begin(); it != rhs.elements.end(); ++it) {
			left_it = std::lower_bound(left_it, elements.end(), *it, cmp);
			if (left_it == elements.end() || *left_it != *it) elements.insert(left_it, *it);
		}
		return *this;
	}

	size_t size() const noexcept { std::scoped_lock lk(internal_mutex); return elements.size(); }

	typename decltype(elements)::iterator begin() noexcept { ensure_guarantees(); return elements.begin(); }
	typename decltype(elements)::iterator end() noexcept { ensure_guarantees(); return elements.end(); }
	typename decltype(elements)::const_iterator begin() const noexcept { ensure_guarantees(); return elements.begin(); }
	typename decltype(elements)::const_iterator end() const noexcept { ensure_guarantees(); return elements.end(); }
};
