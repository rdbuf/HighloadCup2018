#include <string>
#include <iostream>

#include <tao/pegtl.hpp>

#define FMT_STRING_ALIAS 1
#include <fmt/format.h>

namespace query_grammar {
	namespace pegtl = tao::pegtl::utf8;

	// std::vector<std::string_view> column_names;
	// std::vector<string_with_hole> query;
	// int limit;

	struct sex : pegtl::string<'s', 'e', 'x'> {};
	struct email : pegtl::string<'e', 'm', 'a', 'i', 'l'> {};
	struct status : pegtl::string<'s', 't', 'a', 't', 'u', 's'> {};
	struct fname : pegtl::string<'f', 'n', 'a', 'm', 'e'> {};
	struct sname : pegtl::string<'s', 'n', 'a', 'm', 'e'> {};
	struct phone : pegtl::string<'p', 'h', 'o', 'n', 'e'> {};
	struct country : pegtl::string<'c', 'o', 'u', 'n', 't', 'r', 'y'> {};
	struct city : pegtl::string<'c', 'i', 't', 'y'> {};
	struct birth : pegtl::string<'b', 'i', 'r', 't', 'h'> {};
	struct interests : pegtl::string<'i', 'n', 't', 'e', 'r', 'e', 's', 't', 's'> {};
	struct likes : pegtl::string<'l', 'i', 'k', 'e', 's'> {};
	struct premium : pegtl::string<'p', 'r', 'e', 'm', 'i', 'u', 'm'> {};
	// those manually unrolled strings could possibly be replaced with TAO_PEGTL_STRING invocations - TBI
	struct eq : pegtl::string<'e', 'q'> {};
	struct domain : pegtl::string<'d', 'o', 'm', 'a', 'i', 'n'> {};
	struct lt : pegtl::string<'l', 't'> {};
	struct gt : pegtl::string<'g', 't'> {};
	struct neq : pegtl::string<'n', 'e', 'q'> {};
	struct any : pegtl::string<'a', 'n', 'y'> {};
	struct null : pegtl::string<'n', 'u', 'l', 'l'> {};
	struct starts : pegtl::string<'s', 't', 'a', 'r', 't', 's'> {};
	struct code : pegtl::string<'c', 'o', 'd', 'e'> {};
	struct year : pegtl::string<'y', 'e', 'a', 'r'> {};
	struct contains : pegtl::string<'c', 'o', 'n', 't', 'a', 'i', 'n', 's'> {};
	struct now : pegtl::string<'n', 'o', 'w'> {};

	struct sex_eq : pegtl::seq<sex, pegtl::one<'_'>, eq> {};
	struct email_domain : pegtl::seq<email, pegtl::one<'_'>, domain> {};
	struct email_lt : pegtl::seq<email, pegtl::one<'_'>, lt> {};
	struct email_gt : pegtl::seq<email, pegtl::one<'_'>, gt> {};
	struct status_eq : pegtl::seq<status, pegtl::one<'_'>, eq> {};
	struct status_neq : pegtl::seq<status, pegtl::one<'_'>, neq> {};
	struct fname_eq : pegtl::seq<fname, pegtl::one<'_'>, eq> {};
	struct fname_any : pegtl::seq<fname, pegtl::one<'_'>, any> {};
	struct fname_null : pegtl::seq<fname, pegtl::one<'_'>, null> {};
	struct sname_eq : pegtl::seq<sname, pegtl::one<'_'>, eq> {};
	struct sname_starts : pegtl::seq<sname, pegtl::one<'_'>, starts> {};
	struct sname_null : pegtl::seq<sname, pegtl::one<'_'>, null> {};
	struct phone_code : pegtl::seq<phone, pegtl::one<'_'>, code> {};
	struct phone_null : pegtl::seq<phone, pegtl::one<'_'>, null> {};
	struct country_eq : pegtl::seq<country, pegtl::one<'_'>, eq> {};
	struct country_null : pegtl::seq<country, pegtl::one<'_'>, null> {};
	struct city_eq : pegtl::seq<city, pegtl::one<'_'>, eq> {};
	struct city_any : pegtl::seq<city, pegtl::one<'_'>, any> {};
	struct city_null : pegtl::seq<city, pegtl::one<'_'>, null> {};
	struct birth_lt : pegtl::seq<birth, pegtl::one<'_'>, lt> {};
	struct birth_gt : pegtl::seq<birth, pegtl::one<'_'>, gt> {};
	struct birth_year : pegtl::seq<birth, pegtl::one<'_'>, year> {};
	struct interests_contains : pegtl::seq<interests, pegtl::one<'_'>, contains> {};
	struct interests_any : pegtl::seq<interests, pegtl::one<'_'>, any> {};
	struct likes_contains : pegtl::seq<likes, pegtl::one<'_'>, contains> {};
	struct premium_now : pegtl::seq<premium, pegtl::one<'_'>, now> {};
	struct premium_null : pegtl::seq<premium, pegtl::one<'_'>, null> {};

	struct query_id : pegtl::string<'q', 'u', 'e', 'r', 'y', '_', 'i', 'd'> {};
	struct limit : pegtl::string<'l', 'i', 'm', 'i', 't'> {};

	struct percent_encoded_ws : pegtl::one<'+'> {};
	struct percent_encoded_byte : pegtl::seq<pegtl::one<'%'>, pegtl::two<pegtl::xdigit>> {};
	struct percent_encoded_string : pegtl::star<pegtl::sor<percent_encoded_ws, percent_encoded_byte>> {};

	struct phone_code : pegtl::three<pegtl::digit> {};
	struct phone_number : pegtl::seq<pegtl::plus<pegtl::digit>, pegtl::one<'('>, phone_code, pegtl::one<')'>, pegtl::plus<pegtl::digit>> {};

	struct email_domain : pegtl::plus<pegtl::sor<pegtl::alnum, pegtl::one<'.'>>> {};
	struct email : pegtl::seq<pegtl::plus<pegtl::sor<pegtl::alnum, pegtl::one<'.', '-', '_'>>>>, pegtl::one<'@'>> {};

	struct birth_year_ : pegtl::seq<pegtl::digit, pegtl::digit, pegtl::digit, pegtl::digit> {};

	struct city_list
	struct interests_list
	struct likes_list

	// we just write a distinct rhs for every field-selector pair and act correspondingly in their apply methods

	template<class Rule>
	struct action : pegtl::nothing<Rule> {};
}

template<class T>
struct set {
	bool universe;
	std::vector<Id> elements;

	set& intersect(const set<T>& rhs) noexcept {
		if (universe) { *this = rhs; }
		else {
			auto left_it = rhs.elements.begin(), right_it = rhs.elements.end();
			const auto removed_range_begin = std::remove_if(elements.begin(), elements.end(), [&left_it, &right_it](const T& x) {
				left_it = std::lower_bound(left_it, right_it, x);
				return *left_it == x;
			});
			elements.resize(removed_range_begin - elements.begin());
		}
		return *this;
	}
	set& insert(T value) { elements.insert(std::upper_bound(elements.begin(), elements.end(), value), value); return *this; }
};

#include <cstdint>
#include <cstddef>
template<unsigned char base>
constexpr uint8_t from_digit(char d) {
	uint8_t result = d - '0';
	if constexpr (base > 10) {
		if (result >= base) result -= ('A' - '0'), result += 10;
		if (result >= base) result -= ('a' - 'A');
	}
	return result;
}

template<unsigned char base, bool upper = false>
constexpr char to_digit(uint8_t d) {
	char result = d + '0';
	if constexpr (base > 10) {
		if (result > '9') {
			result -= 10;
			if constexpr (upper) result += ('A' - '0');
			else result += ('a' - '0');
		}
	}
	return result;
}

constexpr int64_t pow(int64_t a, int64_t b) {
	if (b == 0) return 1;
	return a * pow(a, b - 1);
}

template<size_t num_digits, unsigned char base>
constexpr uint64_t readint(const char* sym) {
	uint64_t result = 0;
	for (size_t i = 0; i < num_digits; ++i) result += from_digit<base>(sym[num_digits-1 - i]) * pow(16, i);
	return result;
}

#inclue <array>
template<size_t num_digits, unsigned char base>
constexpr std::array<char, num_digits> showint(uint64_t value) {
	std::array<char, num_digits> result;
	for (int i = 0; i < num_digits; ++i) result[i] = to_digit<base>((value / pow(base, num_digits-1 - i)) % base);
	return result;
}

// all the per-symbol decoding is to be done via parser actions

/* Define indices */
#include "tsl/array_map"
std::array<set<Id>, 2> sex; // to be accessed via sex_t
// std::unordered_map<domain_t, set<Id>> ids_by_domain;
tsl::array_map<char, set<Id>> ids_by_domain; // to be used with insert_ks
std::map<email_t, set<Id>>
