#include <string>
#include <iostream>

#define FMT_STRING_ALIAS 1
#include <fmt/format.h>
#include <fmt/ostream.h>

/* Definition of domain specific types */

#include "set.hh"
#include <tsl/array_map.h>
#include <tsl/hopscotch_map.h>

#include "common-types.hh"

tsl::hopscotch_map<CoolHash, name_t> fnames;
tsl::hopscotch_map<CoolHash, name_t> snames;
tsl::hopscotch_map<CoolHash, country_t> countries;
tsl::hopscotch_map<CoolHash, city_t> cities;
tsl::hopscotch_map<CoolHash, interest_t> interests;

#include "account.hh"
#include "like.hh"
#include "string.hh"

/* Indices */

#include <tsl/array_map.h>
#include <tsl/htrie_map.h>
#include <unordered_map>
// currently we store at least 1600000 * 352 + (12 + 3) * N
std::array<std::optional<Account>, 1600000> accounts_by_id; // ids are considered mostly contiguous
tsl::hopscotch_map<Id, Account> accounts_by_id_fallback;
// candidate for set<Id> ids_with_female_gender;
std::array<set<Id>, 2> ids_by_sex; // to be accessed via sex_t
tsl::array_map<char, set<Id>> ids_by_domain; // to be used with insert_ks
struct cmp_id_by_email { bool operator()(Id a, Id b) { // all optionals are assumed to be present
	const Account& a_acc = (a < accounts_by_id.size() ? *accounts_by_id[a] : accounts_by_id_fallback[a]);
	const Account& b_acc = (b < accounts_by_id.size() ? *accounts_by_id[b] : accounts_by_id_fallback[b]);
	return *(a_acc.email) < *(b_acc.email);
}};
set<Id, cmp_id_by_email> ids_sorted_by_email;
std::array<set<Id>, 3> ids_by_status; // to be accessed via status_t
tsl::array_map<char, set<Id>> ids_by_fname;
tsl::htrie_map<char, set<Id>> ids_by_sname;
tsl::hopscotch_map<uint16_t, set<Id>> ids_by_code;
// candidate for set<Id> ids_with_phone
std::array<set<Id>, 2> ids_by_phone_presence; // to be accessed via presence_t
tsl::array_map<char, set<Id>> ids_by_country;
tsl::array_map<char, set<Id>> ids_by_city;
struct cmp_id_by_birth { bool operator()(Id a, Id b) { // all optionals are assumed to be present
	const Account& a_acc = (a < accounts_by_id.size() ? *accounts_by_id[a] : accounts_by_id_fallback[a]);
	const Account& b_acc = (b < accounts_by_id.size() ? *accounts_by_id[b] : accounts_by_id_fallback[b]);
	return a_acc.birth < b_acc.birth;
}};
set<Id, cmp_id_by_birth> ids_sorted_by_birth;
tsl::array_map<char, set<Id>> ids_by_interest;
// candidate for set<Id> ids_with_premium_now
set<Id> ids_by_premium_now;
// candidate for set<Id> ids_with_premium
std::array<set<Id>, 2> ids_by_premium_presence;

int64_t current_ts;

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
		acc.likes.insert(like);
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
		CoolHash h = coolhash(buffer.c_str(), buffer.length());
		acc.interest_idcs.insert(h);
		if (interests.find(h) == interests.end()) interests[h] = interest_t(buffer.c_str(), buffer.length());
		buffer.clear();
	}};

	template<>
	struct action<phone_prefix> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.phone = phone_t{}; // init the optional
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
		// std::cerr << fmt::format("    assigning domain = {}, size = {}\n", std::string_view(in.begin(), in.size()), in.size());
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
		// std::cerr << "[.] processing " << id << '\n';

		if (id < accounts_by_id.size()) accounts_by_id[id] = std::move(acc);
		else accounts_by_id_fallback[id] = std::move(acc);
		acc = Account{};
		const Account& acc_ref = (id < accounts_by_id.size() ? *accounts_by_id[id] : accounts_by_id_fallback[id]);

		// std::cerr << "  index: ids_by_sex" << '\n';
		ids_by_sex[acc_ref.sex].insert(id);
		// std::cerr << fmt::format("  index: ids_by_domain (domain = {}, len = {}, str = {}, ptr = {})\n", domain, domain.size(), (void*)domain.str, (void*)domain.ptr);
		ids_by_domain[domain].insert(id);
		// std::cerr << "  index: ids_sorted_by_email" << '\n';
		if (acc_ref.email) ids_sorted_by_email.insert(id);
		// std::cerr << "  index: ids_by_status" << '\n';
		ids_by_status[acc_ref.status].insert(id);
		// std::cerr << "  index: ids_by_fname" << '\n';
		if (acc_ref.fname_idx) ids_by_fname[fnames[*acc_ref.fname_idx]].insert(id);
		// std::cerr << "  index: ids_by_sname" << '\n';
		if (acc_ref.sname_idx) ids_by_sname[snames[*acc_ref.sname_idx]].insert(id);
		// std::cerr << "  index: ids_by_code" << '\n';
		if (acc_ref.phone) ids_by_code[acc_ref.phone->code].insert(id);
		// std::cerr << "  index: ids_by_phone_presence" << '\n';
		ids_by_phone_presence[acc_ref.phone.has_value()].insert(id);
		// std::cerr << "  index: ids_by_country" << '\n';
		if (acc_ref.country_idx) ids_by_country[countries[*acc_ref.country_idx]].insert(id);
		// std::cerr << "  index: ids_by_city" << '\n';
		if (acc_ref.city_idx) ids_by_city[cities[*acc_ref.city_idx]].insert(id);
		// std::cerr << "  index: ids_by_interest" << '\n';
		for (const CoolHash hash : acc_ref.interest_idcs) ids_by_interest[interests[hash]].insert(id);
		// std::cerr << "  index: ids_by_premium_now" << '\n';
		if (*acc_ref.premium_start < current_ts && *acc_ref.premium_end > current_ts) ids_by_premium_now.insert(id);
		// std::cerr << "  index: ids_by_premium_presence" << '\n';
		ids_by_premium_presence[acc_ref.premium_start.has_value()].insert(id);

		domain.clear();
		// std::cerr << "[+] processed" << '\n';
	}};
}

void build_interests_intersection_index() {
	const auto process = [](Account& acc) {
		for (CoolHash hash : acc.interest_idcs) {
			const set<Id>& ids = ids_by_interest[interests[hash]];
			acc.nonzero_interest_intersection.elements.insert(
				acc.nonzero_interest_intersection.elements.end(),
				ids.begin(), ids.end()
			);
			acc.nonzero_interest_intersection.ensure_guarantees();
		}
		// std::vector<Id> others;
		// for (CoolHash hash : acc.interest_idcs) {
		// 	const set<Id>& ids = ids_by_interest[interests[hash]];
		// 	others.insert(others.end(), ids.begin(), ids.end());
		// }
		// std::sort(others.begin(), others.end());

		// for (
		// 	auto range_beg = others.begin(), range_end = std::equal_range(range_beg, others.end(), *range_beg).second;
		// 	range_beg != others.end();
		// 	range_beg = range_end, range_end = std::equal_range(range_beg, others.end(), *range_beg).second
		// ) {
		// 	acc.nonzero_interest_intersection[range_end - range_beg].insert(*range_beg);
		// }
	};
	for (auto& x : accounts_by_id) if (x) process(*x);
	for (const auto& x : accounts_by_id_fallback) process(const_cast<Account&>(x.second));
}

void report() {
	size_t cnt = 0;

	std::cerr << fmt::format(
		"fnames: {}\n"
		"snames: {}\n"
		"countries: {}\n"
		"cities: {}\n"
		"interests: {}\n",
		fnames.size(), snames.size(),
		countries.size(), cities.size(),
		interests.size()
	);
	std::cerr << fmt::format("accounts_by_id_fallback: {}\n", accounts_by_id_fallback.size());

	std::cerr << fmt::format("ids total: {}\n", ids_sorted_by_email.size());

	cnt = 0; for (const auto& x : ids_by_interest) cnt += x.size();
	std::cerr << fmt::format("ids_by_interest: {}\n", cnt);

	cnt = 0; for (const auto& x : accounts_by_id) if (x) cnt += x->likes.size();
	std::cerr << fmt::format("likes: {}\n", cnt);
}

#include <iostream>
int main(int argc, char** argv) {
	namespace pegtl = tao::pegtl;

	Account acc{};
	email_t domain{};
	std::string buffer;
	Like like{};
	std::vector<Like> likes;
	for (int i = 1; i < argc; ++i) {
		pegtl::file_input in(argv[i]);
		pegtl::parse<pegtl::must<data_grammar::file>, data_grammar::action>(in, acc, domain, buffer, like);
	}

	build_interests_intersection_index();
	report();
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