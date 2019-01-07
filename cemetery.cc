template<class It>
auto iterator_comparator = [](It it1, It it2) { return *it1 < *it2; }

#include <functional>
namespace std {
	template<class T>
	struct hash<T::iterator> {
		using argument_type = T::iterator;
		using result_type = size_t;
		result_type operator(argument_type it) const noexcept {
			return hash<decltype(*it)*>{}(std::addressof(*it));
		}
	};
}

#include <functional>
namespace std {
	template<size_t N>
	struct hash<const char[N]> {
		using argument_type = const char[N];
		using result_type = size_t;
		result_type operator()(argument_type arr) {
			result_type result = 0;
			for (int i = 0; i < N; ++i) { result = result * 31 + arr[i]; }
			return result;
		}
	};
}

// probably not to be used
#include <cassert>
void urlunquote_rus(const char* src, size_t n) {
	assert(n % 6 == 0);
	static constexpr uint64_t base = readint<4, 16>("D090");
	for (int i = 0; i < n/6; ++i) {
		src[i] = readint<2, 16>(src + i*6 + 1) * pow(16, 2) + readint<2, 16>(src + i*6 + 4) - base;
	}
}

/* Define string types */
template<size_t N>
using utf8str = unsigned char[N*2];
template<size_t N>
using asciistr = unsigned char[N];
template<size_t N>
using russtr = unsigned char[N];

/* Define domain specific types */
using Id = uint32_t;
enum sex_t { male, female };
using email_t = asciistr<100>;
using domain_t = asciistr<100>;
using name_t = russtr<50>;
using phone_t = asciistr<16>;
using country_t = russtr<50>;
using city_t = russtr<50>;
using interest_t = utf8str<100>;

namespace util {
	// basically when parsing field+predicate, create a format string, and fill it once the argument becomes available
	struct string_with_hole {
		// std::vector<const char*> parts; // in order to avoid allocations, we gonna use our own allocator
		// std::array<const char*, 4> begin;
		const char* begin[4];
		const char** end = begin;
		const char* filler = "";
		std::string format() const {
			std::string s;
			for (const char** p = begin; p < end - 1; ++p) { s += *p; s += filler; }
			s += *p;
			return s;
		}
	};
	// string_with_hole operator%(const char* lhs, const char* rhs) { return string_with_hole{.parts = {lhs, rhs}}; }
	// string_with_hole operator%(string_with_hole&& lhs, const char* rhs) { lhs.parts.push_back(rhs); return string_with_hole; }
	string_with_hole operator%(const char* lhs, const char* rhs) {
		string_with_hole s;
		*s.end++ = lhs;
		*s.end++ = rhs;
		return s;
	}
	string_with_hole operator%(string_with_hole&& lhs, const char* rhs) {
		*lhs.end++ = rhs;
		return lhs;
	}
}