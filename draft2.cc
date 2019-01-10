#include <string>
#include <iostream>

#define FMT_STRING_ALIAS 1
#include <fmt/format.h>

/* Definition of domain specific types */

#include <string_view>
template<size_t Len, size_t BytesPerSymbol>
struct fixwstr { // unsafe fixed-width string without boundary checks
	char str[Len*BytesPerSymbol+1];
	char* ptr = str;

	fixwstr() = default;
	fixwstr(const char* a, size_t sz) noexcept {
		std::memcpy(ptr, a, sz);
		ptr += sz;
	}

	operator std::string_view() { return std::string_view(str, ptr - str); }

	char* begin() noexcept { return str; }
	char* end() noexcept { return ptr; }
	const char* begin() const noexcept { return str; }
	const char* end() const noexcept { return ptr; }

	size_t size() const noexcept { return ptr - str; }

	fixwstr& operator+=(char a) noexcept { *ptr++ = a; return *this; }
	fixwstr& operator+=(const char* a) noexcept {
		const int sz = std::strlen(a);
		std::memcpy(ptr, a, sz);
		ptr += sz;
        return *this;
	}

	fixwstr& operator-=(int n) noexcept { ptr -= n; return *this; }

	fixwstr& ensure_zero() noexcept { *ptr = 0; return *this; }
    operator const unsigned char*() const noexcept { *ptr = 0; return reinterpret_cast<const unsigned char*>(str); }
};

using Id = uint32_t;
using EpochSecs = int64_t;
enum sex_t { male, female };
enum status_t { single, relationship, undecided };
using email_t = fixwstr<100, 1>;
using domain_t = fixwstr<100, 1>;
using name_t = fixwstr<50, 2>;
struct phone_t {
	uint64_t prefix : 8, code : 16, num_len : 4, num : 36;
};
// using phone_t = uint64_t;
using country_t = fixwstr<50, 2>;
using city_t = fixwstr<50, 2>;
using interest_t = fixwstr<100, 2>;

#include <vector>
#include <algorithm>
template<class T>
// this shit doesn't pay off.
// needa faster insertions
struct set {
	std::vector<T> elements;
	bool universe = false;

	set() = default;
    set(std::initializer_list<T> il) : elements(il.begin(), il.end()) {}
	set(typename std::vector<T>::iterator it1, typename std::vector<T>::iterator it2) : elements(it1, it2) {}

	set& intersect(const set<T>& rhs) noexcept {
		if (universe) { *this = rhs; }
		else {
			auto left_it = rhs.elements.begin(), right_it = rhs.elements.end();
			const auto removed_range_begin = std::remove_if(elements.begin(), elements.end(), [&left_it, &right_it, this](const T& x) {
				left_it = std::lower_bound(left_it, right_it, x);
				bool result = *left_it != x;
				return result;
			});
			elements.resize(removed_range_begin - elements.begin());
		}
		return *this;
	}
	set& unsafe_mid_insert(T value) noexcept {
		elements.insert(std::upper_bound(elements.begin(), elements.end(), value), value); return *this;
	}
	set& unsafe_end_insert(T value) noexcept {
		elements.push_back(value); return *this;
	}
	set& ensure_guarantees() {
		std::sort(elements.begin(), elements.end());
		elements.erase(std::unique(elements.begin(), elements.end()), elements.end());
		return *this;
	}
	set& unite(const set<T>& rhs) noexcept {
		elements.reserve(elements.size() + rhs.elements.size());
		auto left_it = elements.begin();
		for (auto it = rhs.elements.begin(); it != rhs.elements.end(); ++it) {
			left_it = std::lower_bound(left_it, elements.end(), *it);
			if (left_it == elements.end() || *left_it != *it) elements.insert(left_it, *it);
		}
		return *this;
	}
};


#include "tsl/array_map.h"
#include "tsl/hopscotch_map.h"

using CoolHash = std::hash<std::string_view>::result_type;

CoolHash coolhash(const char* str, size_t n) { return std::hash<std::string_view>{}(std::string_view(str, n)); }
tsl::hopscotch_map<CoolHash, name_t> fnames;
tsl::hopscotch_map<CoolHash, name_t> snames;
tsl::hopscotch_map<CoolHash, country_t> countries;
tsl::hopscotch_map<CoolHash, city_t> cities;
tsl::hopscotch_map<CoolHash, interest_t> interests;

#include <optional>
#include <string_view>
struct Account { // size: 784 -> 368 -> 272
	email_t email;
	set<CoolHash> interest_idcs;
	std::optional<CoolHash> fname_idx, sname_idx;
	std::optional<CoolHash> country_idx;
	std::optional<CoolHash> city_idx;
	std::optional<phone_t> phone;
	EpochSecs birth, joined, premium_start, premium_end;
	Id id;
	status_t status;
	sex_t sex;
};

struct Like {
	Id other;
	__int128_t net_ts;
	int64_t num_tss;

	bool operator<(Like rhs) const noexcept { return other < rhs.other; }
	bool operator==(Like rhs) const noexcept { return other == rhs.other; }
	Like& operator+=(Like rhs) noexcept { net_ts += rhs.net_ts; num_tss += rhs.num_tss; return *this; }
};

template<>
set<Like>& set<Like>::ensure_guarantees() {
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
	return *this;
}

/* Definition of indices */

#include <tsl/array_map.h>
#include <tsl/htrie_map.h>
#include <unordered_map>
std::array<set<Id>, 2> ids_by_sex; // to be accessed via sex_t
tsl::array_map<char, set<Id>> ids_by_domain; // to be used with insert_ks
std::vector<Id> ids_sorted_by_email;
std::array<set<Id>, 3> ids_by_status; // to be accessed via status_t
tsl::array_map<char, set<Id>> ids_by_fname;
tsl::htrie_map<char, set<Id>> ids_by_sname;
tsl::hopscotch_map<uint16_t, set<Id>> ids_by_code;
std::array<set<Id>, 2> ids_by_code_presence; // to be accessed via presence_t
tsl::array_map<char, set<Id>> ids_by_country;
tsl::array_map<char, set<Id>> ids_by_city;
std::vector<Id> ids_sorted_by_birth;
tsl::array_map<char, set<Id>> ids_by_interest;
std::unordered_map<Id, set<Id>> liked_ids_by_id;
std::array<set<Id>, 2> ids_by_premium_now;
std::array<set<Id>, 2> ids_by_premium_presence;
std::array<std::optional<Account>, 1600000> accounts_by_id; // ids are considered mostly contiguous
tsl::hopscotch_map<Id, Account> accounts_by_id_fallback;
std::array<set<Like>, 1600000> likes_by_id;

/* Helpers for parsing */

#include <cstdint>
#include <cstddef>
namespace intparsing {
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
		return a * intparsing::pow(a, b - 1);
	}

	template<unsigned char base>
	constexpr uint64_t readint(const char* sym, size_t num_digits) {
		uint64_t result = 0;
		for (size_t i = 0; i < num_digits; ++i) result += from_digit<base>(sym[num_digits-1 - i]) * intparsing::pow(16, i);
		return result;
	}

	#include <array>
	template<unsigned char base, size_t num_digits>
	constexpr std::array<char, num_digits> showint(uint64_t value) {
		std::array<char, num_digits> result;
		for (int i = 0; i < num_digits; ++i) result[i] = to_digit<base>((value / intparsing::pow(base, num_digits-1 - i)) % base);
		return result;
	}
}

/* Parsers */

#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/icu/utf8.hpp>
#include <utf8_encode.c>
namespace data_grammar {
	namespace pegtl = tao::pegtl;

	struct nat : pegtl::seq<pegtl::range<'1', '9'>, pegtl::star<pegtl::digit>> {};

	struct unicode_escaped_char : pegtl::seq<TAO_PEGTL_STRING(R"(\u)"), pegtl::rep<4, pegtl::xdigit>> {};
	struct unicode_regular_char : pegtl::sor<pegtl::utf8::icu::alphabetic, pegtl::digit, pegtl::ranges<0x20, 0x21, 0x23, 0x2f, 0x3a, 0x40, 0x5b, 0x60, 0x7b, 0x7e>> {};
	template<bool nonempty = false>
	struct unicode_string : std::conditional<nonempty,
		pegtl::plus<pegtl::sor<unicode_escaped_char, unicode_regular_char>>,
		pegtl::star<pegtl::sor<unicode_escaped_char, unicode_regular_char>>
	>::type {};

	struct wss : pegtl::star<pegtl::one<' ', '\n', '\t'>> {};
	template<char Opening, class Rule, char Closing>
	struct list : pegtl::seq<
		pegtl::one<Opening>, wss,
		pegtl::opt<pegtl::list<Rule, pegtl::one<','>, pegtl::space>>,
		wss, pegtl::one<Closing>
	> {};
	template<class... Fields>
	struct object : list<'{', pegtl::sor<Fields...>, '}'> {};
	template<class Element>
	struct array : list<'[', Element, ']'> {};
	template<class Rule, char quotation_mark = '"'>
	struct quoted : pegtl::seq<pegtl::one<quotation_mark>, Rule, pegtl::one<quotation_mark>> {};
	template<class Name, class Value>
	struct field : pegtl::seq<quoted<Name>, wss, pegtl::one<':'>, wss, Value> {};

	struct id : nat {};
	struct email_domain : pegtl::plus<pegtl::sor<pegtl::alnum, pegtl::one<'.'>>> {};
	struct email : pegtl::seq<pegtl::plus<pegtl::sor<pegtl::alnum, pegtl::one<'.', '-', '_'>>>, pegtl::one<'@'>, email_domain> {};
	struct fname : unicode_string<> {};
	struct sname : unicode_string<> {};
	struct phone_prefix : nat {};
	struct phone_code : nat {};
	struct phone_num : pegtl::plus<pegtl::digit> {}; // is not really guaranteed - need to be careful
	struct phone : pegtl::seq<phone_prefix, pegtl::one<'('>, phone_code, pegtl::one<')'>, phone_num> {};
	struct sex_male : pegtl::one<'m'> {};
	struct sex_female : pegtl::one<'f'> {};
	struct sex : pegtl::sor<sex_male, sex_female> {};
	struct birth : nat {};
	struct country : unicode_string<> {};
	struct city : unicode_string<> {};
	struct joined : nat {};
	struct status_single : pegtl::sor<
		pegtl::seq<pegtl::one<0xd1>, pegtl::one<0x81>, pegtl::one<0xd0>, pegtl::one<0xb2>, pegtl::one<0xd0>, pegtl::one<0xbe>, pegtl::one<0xd0>, pegtl::one<0xb1>, pegtl::one<0xd0>, pegtl::one<0xbe>, pegtl::one<0xd0>, pegtl::one<0xb4>, pegtl::one<0xd0>, pegtl::one<0xbd>, pegtl::one<0xd1>, pegtl::one<0x8b>>, // "свободны"
		TAO_PEGTL_STRING(R"(\u0441\u0432\u043e\u0431\u043e\u0434\u043d\u044b)")
	> {};
	struct status_relationship : pegtl::sor<
		pegtl::seq<pegtl::one<0xd0>, pegtl::one<0xb7>, pegtl::one<0xd0>, pegtl::one<0xb0>, pegtl::one<0xd0>, pegtl::one<0xbd>, pegtl::one<0xd1>, pegtl::one<0x8f>, pegtl::one<0xd1>, pegtl::one<0x82>, pegtl::one<0xd1>, pegtl::one<0x8b>>, // "заняты"
		TAO_PEGTL_STRING(R"(\u0437\u0430\u043d\u044f\u0442\u044b)")
	> {};
	struct status_undecided : pegtl::sor<
		pegtl::seq<pegtl::one<0xd0>, pegtl::one<0xb2>, pegtl::one<0xd1>, pegtl::one<0x81>, pegtl::one<0xd1>, pegtl::one<0x91>, pegtl::one<0x20>, pegtl::one<0xd1>, pegtl::one<0x81>, pegtl::one<0xd0>, pegtl::one<0xbb>, pegtl::one<0xd0>, pegtl::one<0xbe>, pegtl::one<0xd0>, pegtl::one<0xb6>, pegtl::one<0xd0>, pegtl::one<0xbd>, pegtl::one<0xd0>, pegtl::one<0xbe>>, // "всё сложно"
		TAO_PEGTL_STRING(R"(\u0432\u0441\u0451 \u0441\u043b\u043e\u0436\u043d\u043e)")
	> {};
	struct status : pegtl::sor<status_single, status_relationship, status_undecided> {};
	struct interest : unicode_string<true> {};
	struct premium_start : nat {};
	struct premium_end : nat {};
	struct like_id : nat {};
	struct like_ts : nat {};
	struct like : object<
		field<TAO_PEGTL_STRING("id"), like_id>,
		field<TAO_PEGTL_STRING("ts"), like_ts>
	> {};

	struct account : object<
		field<TAO_PEGTL_STRING("id"), id>,
		field<TAO_PEGTL_STRING("email"), quoted<email>>,
		field<TAO_PEGTL_STRING("fname"), quoted<fname>>,
		field<TAO_PEGTL_STRING("sname"), quoted<sname>>,
		field<TAO_PEGTL_STRING("phone"), quoted<phone>>,
		field<TAO_PEGTL_STRING("sex"), quoted<sex>>,
		field<TAO_PEGTL_STRING("birth"), birth>,
		field<TAO_PEGTL_STRING("country"), quoted<country>>,
		field<TAO_PEGTL_STRING("city"), quoted<city>>,
		field<TAO_PEGTL_STRING("joined"), joined>,
		field<TAO_PEGTL_STRING("status"), quoted<status>>,
		field<TAO_PEGTL_STRING("interests"), array<quoted<interest>>>,
		field<TAO_PEGTL_STRING("premium"), object<
			field<TAO_PEGTL_STRING("start"), premium_start>,
			field<TAO_PEGTL_STRING("finish"), premium_end>
		>>,
		field<TAO_PEGTL_STRING("likes"), array<like>>
	> {};
	struct file : pegtl::seq<wss, object<field<TAO_PEGTL_STRING("accounts"), array<account>>>> {};

	template<class Rule>
	struct action : pegtl::nothing<Rule> {};

	template<>
	struct action<id> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.id = intparsing::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<like_id> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		like.other = intparsing::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<like_ts> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		like.net_ts = intparsing::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<like> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		// push it where aimed
		like = Like{};
	}};

	template<>
	struct action<fname> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		CoolHash h = *(acc.fname_idx = coolhash(buffer.c_str(), buffer.length()));
		if (fnames.find(h) == fnames.end()) fnames[h] = name_t(buffer.c_str(), buffer.length());
		buffer.clear();
	}};

	template<>
	struct action<sname> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		CoolHash h = *(acc.sname_idx = coolhash(buffer.c_str(), buffer.length()));
		if (snames.find(h) == snames.end()) snames[h] = name_t(buffer.c_str(), buffer.length());
		buffer.clear();
	}};

	template<>
	struct action<country> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		CoolHash h = *(acc.country_idx = coolhash(buffer.c_str(), buffer.length()));
		if (countries.find(h) == countries.end()) countries[h] = country_t(buffer.c_str(), buffer.length());
		buffer.clear();
	}};

	template<>
	struct action<city> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		CoolHash h = *(acc.city_idx = coolhash(buffer.c_str(), buffer.length()));
		if (cities.find(h) == cities.end()) cities[h] = city_t(buffer.c_str(), buffer.length());
		buffer.clear();
	}};

	template<>
	struct action<interest> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		CoolHash h = *(acc.city_idx = coolhash(buffer.c_str(), buffer.length()));
		if (cities.find(h) == cities.end()) cities[h] = city_t(buffer.c_str(), buffer.length());
		buffer.clear();
	}};

	template<>
	struct action<phone_prefix> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.phone = {}; // init the optional
		acc.phone->prefix = intparsing::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<phone_code> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.phone->code = intparsing::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<phone_num> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.phone->num = intparsing::readint<10>(in.begin(), in.size());
		acc.phone->num_len = in.size();
	}};

	template<>
	struct action<birth> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.birth = intparsing::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<joined> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.joined = intparsing::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<premium_start> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.premium_start = intparsing::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<premium_end> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.premium_end = intparsing::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<status_single> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.status = single;
	}};

	template<>
	struct action<status_relationship> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.status = relationship;
	}};

	template<>
	struct action<status_undecided> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.status = undecided;
	}};

	template<>
	struct action<email_domain> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		domain = email_t(in.begin(), in.size());
	}};

	template<>
	struct action<email> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.email = email_t(in.begin(), in.size());
	}};

	template<>
	struct action<sex_male> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.sex = male;
	}};

	template<>
	struct action<sex_female> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.sex = female;
	}};

	template<>
	struct action<unicode_regular_char> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		buffer.append(in.begin(), in.size());
	}};

	template<>
	struct action<unicode_escaped_char> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		char buf[5];
		int width = utf8_encode(buf, intparsing::readint<16>(in.begin()+2, 4));
		buffer.append(buf, width);
	}};

	template<>
	struct action<account> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		Id id = acc.id;
		if (id < accounts_by_id.size()) accounts_by_id[id] = std::move(acc);
		else accounts_by_id_fallback[id] = std::move(acc);

		if (acc.phone) ids_by_code[acc.phone->code].unsafe_end_insert(id);

		acc = Account{};
		domain = email_t{};
	}};
}

#include <iostream>
int main(int argc, char** argv) {
	namespace pegtl = tao::pegtl;
	pegtl::file_input in(argv[1]);

	Account acc;
	email_t domain;
	std::string buffer;
	Like like;
	pegtl::parse<pegtl::must<data_grammar::file>, data_grammar::action>(in, acc, domain, buffer, like);
}


/*
namespace query_grammar {
	namespace pegtl = tao::pegtl::ascii;

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
*/