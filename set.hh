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

/* Copy on write may appear neccesary */

template<class T, class Comparator = std::less<T>>
struct set {
	mutable std::mutex internal_mutex;
	mutable std::vector<T> elements;
	mutable std::vector<T> removal_queue;
	Comparator cmp;
	bool universe = false;
	mutable bool invariant_holds = true;

	set() = default;
	set(typename std::vector<T>::iterator begin, typename std::vector<T>::iterator end) : elements(begin, end) {}
	set(const set<T>& other, const std::scoped_lock<std::mutex>&) :
		elements(other.elements),
		removal_queue(other.removal_queue),
		cmp(other.cmp),
		universe(other.universe),
		invariant_holds(other.invariant_holds),
		internal_mutex() {}
	set(const set<T>& other) : set(other, std::scoped_lock<std::mutex>(other.internal_mutex)) {}
	set(set<T>&& other, const std::scoped_lock<std::mutex>&) :
		elements(std::move(other.elements)),
		removal_queue(std::move(other.removal_queue)),
		cmp(std::move(other.cmp)),
		universe(other.universe),
		invariant_holds(other.invariant_holds),
		internal_mutex() {}
	set(set<T>&& other) : set(std::move(other), std::scoped_lock<std::mutex>(other.internal_mutex)) {}

	set& operator=(const set<T>& rhs) {
		if (&rhs == this) return *this;
		std::scoped_lock<std::mutex> lock(rhs.internal_mutex);
		if (rhs.universe) elements.clear(), removal_queue.clear();
		else elements = rhs.elements, removal_queue = rhs.removal_queue;
		cmp = rhs.cmp;
		universe = rhs.universe;
		invariant_holds = rhs.invariant_holds;
        return *this;
	}
	set& operator=(set<T>&& rhs) {
		if (&rhs == this) return *this;
		std::scoped_lock<std::mutex> lock(rhs.internal_mutex);
		if (rhs.universe) elements.clear(), removal_queue.clear();
		else elements = std::move(rhs.elements), removal_queue = std::move(rhs.removal_queue);
		cmp = std::move(rhs.cmp);
		universe = rhs.universe;
		invariant_holds = rhs.invariant_holds;
        return *this;
	}

	void ensure_guarantees() const {
		if (!invariant_holds) {
			std::sort(elements.begin(), elements.end(), cmp);
			elements.erase(std::unique(elements.begin(), elements.end()), elements.end());
			invariant_holds = true;
		}
		for (auto x : removal_queue) {
      		auto it = std::lower_bound(elements.begin(), elements.end(), x, cmp);
      		assert(*it == x);
      		elements.erase(it);
    	}
		elements.shrink_to_fit();
    	removal_queue.clear();
    	removal_queue.shrink_to_fit();
	}

	template<bool lower, class Arg, class F = Comparator>
	set<T> partition(const Arg& value, F cmp = F{}) {
		std::scoped_lock lk(internal_mutex);
		ensure_guarantees();
		if constexpr (lower) {
			set<T> result(elements.begin(), std::lower_bound(elements.begin(), elements.end(), value, cmp));
			result.invariant_holds = false; // we are returning set<T>, not set<T, Comparator>
			return result;
		} else {
			set<T> result(std::lower_bound(elements.begin(), elements.end(), value, cmp), elements.end());
			result.invariant_holds = false;
			return result;
		}
	}

	template<class Arg, class F = Comparator, class G = F>
	set<T> mid_partition(const Arg& lower_value, const Arg& upper_value, F cmp = F{}) {
		std::scoped_lock lk(internal_mutex);
		ensure_guarantees();
		auto lower_it = std::lower_bound(elements.begin(), elements.end(), lower_value, cmp);
		auto upper_it = std::lower_bound(lower_it, elements.end(), upper_value, cmp);
		set<T> result(lower_it, upper_it);
		result.invariant_holds = false;
		return result;
	}

	set& insert(T value) { // invariant assurance is postponed
		std::scoped_lock lk(internal_mutex);
		invariant_holds = false;
		if (!elements.size() || value != elements.back()) elements.push_back(value);
		return *this;
	}
	set& remove(T value) { // presence not checked
		std::scoped_lock lk(internal_mutex);
		removal_queue.push_back(value);
		return *this;
	}

	set& intersect(const set<T>& other) {
		// assert(elements.size() <= other.elements.size()); // perf reasons
		if (universe) {
			{
				std::scoped_lock lk(other.internal_mutex);
				other.ensure_guarantees();
			}
			*this = other;
		}
		else {
			std::scoped_lock lk(other.internal_mutex);
			ensure_guarantees(), other.ensure_guarantees();
			auto left_it = other.elements.begin(), right_it = other.elements.end();
			const auto removed_range_begin = std::remove_if(elements.begin(), elements.end(), [&left_it, &right_it, this](const T& x) {
				left_it = std::lower_bound(left_it, right_it, x, cmp);
				return left_it == right_it || *left_it != x;
			});
			elements.resize(removed_range_begin - elements.begin());
		}
		return *this;
	}

	set& unite(const set<T>& other) {
		// lazy strategy beats: http://quick-bench.com/t7a10l8URRr2AoTpN9IBblDwXD8
		if (universe) return *this;
		std::scoped_lock lk(other.internal_mutex);
		invariant_holds = invariant_holds;
		elements.insert(elements.end(), other.elements.begin(), other.elements.end());
		return *this;
	}

	void clear() {
		universe = false;
		elements.clear();
		removal_queue.clear();
		invariant_holds = true;
	}

	size_t size() const noexcept { std::scoped_lock lk(internal_mutex); ensure_guarantees(); return elements.size(); }

	typename decltype(elements)::iterator begin() noexcept { ensure_guarantees(); return elements.begin(); }
	typename decltype(elements)::iterator end() noexcept { ensure_guarantees(); return elements.end(); }
	typename decltype(elements)::const_iterator begin() const noexcept { ensure_guarantees(); return elements.begin(); }
	typename decltype(elements)::const_iterator end() const noexcept { ensure_guarantees(); return elements.end(); }
};
