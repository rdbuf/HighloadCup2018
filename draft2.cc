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
struct {
	std::array<std::optional<Account>, 1600000> static_; // ids are considered mostly contiguous
	tsl::hopscotch_map<Id, Account> dynamic;

	template<class F>
	void foreach(F f) {
		for (std::optional<Account>& acc : static_) if (acc) f(*acc);
		for (const std::pair<Id, Account>& acc : dynamic) f(const_cast<Account&>(acc.second));
	}

	Account& operator[](Id idx) noexcept {
		if (idx < static_.size()) {
			if (!static_[idx]) static_[idx] = Account{};
			return *static_[idx];
		} else {
			return dynamic[idx];
		}
	}
} accounts_by_id;
// candidate for set<Id> ids_with_female_gender;
std::array<set<Id>, 2> ids_by_sex; // to be accessed via sex_t
tsl::array_map<char, set<Id>> ids_by_domain; // to be used with insert_ks
struct cmp_id_by_email { bool operator()(Id a, Id b) { // all optionals are assumed to be present
	return *accounts_by_id[a].email < *accounts_by_id[b].email;
}};
set<Id, cmp_id_by_email> ids_sorted_by_email;
// candidate for array<set<Id>, 2> ids_by_nonfree_status
std::array<set<Id>, 3> ids_by_status; // to be accessed via status_t
tsl::array_map<char, set<Id>> ids_by_fname;
tsl::htrie_map<char, set<Id>> ids_by_sname;
tsl::hopscotch_map<uint16_t, set<Id>> ids_by_code;
// candidate for set<Id> ids_with_phone
std::array<set<Id>, 2> ids_by_phone_presence; // to be accessed via presence_t
tsl::array_map<char, set<Id>> ids_by_country;
tsl::array_map<char, set<Id>> ids_by_city;
struct cmp_id_by_birth { bool operator()(Id a, Id b) { // all optionals are assumed to be present
	return accounts_by_id[a].birth < accounts_by_id[b].birth;
}};
set<Id, cmp_id_by_birth> ids_sorted_by_birth;
tsl::array_map<char, set<Id>> ids_by_interest;
// candidate for set<Id> ids_with_premium_now
set<Id> ids_by_premium_now;
// candidate for set<Id> ids_with_premium
std::array<set<Id>, 2> ids_by_premium_presence;

int64_t current_ts;

#include "parseint.hh"

/* Parsers */

#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/icu/utf8.hpp>
#include <utf8_encode.c>

namespace account_grammar {
	namespace pegtl = tao::pegtl;

	struct fini {};

	struct wss : pegtl::star<pegtl::one<' ', '\n', '\t'>> {};
	template<char Opening, class Rule, char Closing>
	struct list : pegtl::seq<
		pegtl::one<Opening>, wss,
		pegtl::opt<pegtl::list<Rule, pegtl::one<','>, pegtl::space>>,
		wss, pegtl::one<Closing>
	> {};

	template<class> struct nat : pegtl::seq<pegtl::range<'1', '9'>, pegtl::star<pegtl::digit>> {};

	struct email_domain : pegtl::plus<pegtl::sor<pegtl::alnum, pegtl::one<'.'>>> {};
	struct email : pegtl::seq<pegtl::plus<pegtl::sor<pegtl::alnum, pegtl::one<'.', '-', '_'>>>, pegtl::one<'@'>, email_domain> {};

	struct unicode_escaped_char : pegtl::seq<TAO_PEGTL_STRING(R"(\u)"), pegtl::rep<4, pegtl::xdigit>> {};
	struct unicode_regular_char : pegtl::sor<pegtl::utf8::icu::alphabetic, pegtl::digit, pegtl::ranges<0x20, 0x21, 0x23, 0x2f, 0x3a, 0x40, 0x5b, 0x60, 0x7b, 0x7e>> {};
	template<class tag> struct unicode_string : pegtl::star<pegtl::sor<unicode_escaped_char, unicode_regular_char>> {};

	template<class... Fields>
	struct object : list<'{', pegtl::sor<Fields...>, '}'> {};
	template<class Element>
	struct array : list<'[', Element, ']'> {};
	template<class Rule, char quotation_mark = '"'>
	struct quoted : pegtl::seq<pegtl::one<quotation_mark>, Rule, pegtl::one<quotation_mark>> {};
	template<class Name, class Value>
	struct field : pegtl::seq<quoted<Name>, wss, pegtl::one<':'>, wss, Value> {};

 	struct id_tag {};
	struct email_tag {};
	struct fname_tag {};
	struct sname_tag {};

	template<class> struct integer_sequence : pegtl::plus<pegtl::digit> {};

	struct phone_prefix_tag {};
	struct phone_code_tag {};
	struct phone_num_tag {};
	struct phone : pegtl::seq<
		nat<phone_prefix_tag>,
		pegtl::one<'('>, nat<phone_code_tag>, pegtl::one<')'>,
		integer_sequence<phone_num_tag>
	> {};

	struct sex_male : pegtl::one<'m'> {};
	struct sex_female : pegtl::one<'f'> {};
	struct sex : pegtl::sor<sex_male, sex_female> {};

	struct birth_tag {};
	struct country_tag {};
	struct city_tag {};
	struct joined_tag {};
	struct phone_tag {};
	struct sex_tag {};
	struct status_tag {};

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

	struct interest_tag {};
	struct premium_start_tag {};
	struct premium_end_tag {};
	struct likee_id_tag {};
	struct like_ts_tag {};

	struct like : object<
		field<TAO_PEGTL_STRING("id"), nat<likee_id_tag>>,
		field<TAO_PEGTL_STRING("ts"), nat<like_ts_tag>>
	> {};

	/* Syntactic constructions such as { ... }, [ ... ], key: value, etc. Those are not to be templated. */
	/* Primitive types such as nat, unicode_string, etc. To be heavily templated. */
	/* Aggregates - ??? Ah, ok: we just template tags and that's it. */

	struct account : object<
		field<TAO_PEGTL_STRING("id"), nat<id_tag>>,
		field<TAO_PEGTL_STRING("email"), quoted<email>>,
		field<TAO_PEGTL_STRING("fname"), quoted<unicode_string<fname_tag>>>,
		field<TAO_PEGTL_STRING("sname"), quoted<unicode_string<sname_tag>>>,
		field<TAO_PEGTL_STRING("phone"), quoted<phone>>,
		field<TAO_PEGTL_STRING("sex"), quoted<sex>>,
		field<TAO_PEGTL_STRING("birth"), nat<birth_tag>>,
		field<TAO_PEGTL_STRING("country"), quoted<unicode_string<country_tag>>>,
		field<TAO_PEGTL_STRING("city"), quoted<unicode_string<city_tag>>>,
		field<TAO_PEGTL_STRING("joined"), nat<joined_tag>>,
		field<TAO_PEGTL_STRING("status"), quoted<status>>,
		field<TAO_PEGTL_STRING("interests"), array<quoted<unicode_string<interest_tag>>>>,
		field<TAO_PEGTL_STRING("premium"), object<
			field<TAO_PEGTL_STRING("start"), nat<premium_start_tag>>,
			field<TAO_PEGTL_STRING("finish"), nat<premium_end_tag>>
		>>,
		field<TAO_PEGTL_STRING("likes"), array<like>>
	> {};
	struct file : pegtl::seq<wss, object<field<TAO_PEGTL_STRING("accounts"), array<account>>>> {};

	template<class Rule>
	struct action : pegtl::nothing<Rule> {};

	template<>
	struct action<nat<id_tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.id = parseint::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<nat<likee_id_tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		like.other = parseint::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<nat<like_ts_tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		like.net_ts = parseint::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<like> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.likes.insert(like);
		like = Like{};
	}};

	template<>
	struct action<unicode_string<fname_tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		CoolHash h = *(acc.fname_idx = coolhash(buffer.c_str(), buffer.length()));
		if (fnames.find(h) == fnames.end()) fnames[h] = name_t(buffer.c_str(), buffer.length());
		buffer.clear();
	}};

	template<>
	struct action<unicode_string<sname_tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		CoolHash h = *(acc.sname_idx = coolhash(buffer.c_str(), buffer.length()));
		if (snames.find(h) == snames.end()) snames[h] = name_t(buffer.c_str(), buffer.length());
		buffer.clear();
	}};

	template<>
	struct action<unicode_string<country_tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		CoolHash h = *(acc.country_idx = coolhash(buffer.c_str(), buffer.length()));
		if (countries.find(h) == countries.end()) countries[h] = country_t(buffer.c_str(), buffer.length());
		buffer.clear();
	}};

	template<>
	struct action<unicode_string<city_tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		CoolHash h = *(acc.city_idx = coolhash(buffer.c_str(), buffer.length()));
		if (cities.find(h) == cities.end()) cities[h] = city_t(buffer.c_str(), buffer.length());
		buffer.clear();
	}};

	template<>
	struct action<unicode_string<interest_tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		CoolHash h = coolhash(buffer.c_str(), buffer.length());
		acc.interest_idcs.insert(h);
		if (interests.find(h) == interests.end()) interests[h] = interest_t(buffer.c_str(), buffer.length());
		buffer.clear();
	}};

	template<>
	struct action<nat<phone_prefix_tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.phone = phone_t{}; // init the optional
		acc.phone->prefix = parseint::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<nat<phone_code_tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.phone->code = parseint::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<integer_sequence<phone_num_tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.phone->num = parseint::readint<10>(in.begin(), in.size());
		acc.phone->num_len = in.size();
	}};

	template<>
	struct action<nat<birth_tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.birth = parseint::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<nat<joined_tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.joined = parseint::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<nat<premium_start_tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.premium_start = parseint::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<nat<premium_end_tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.premium_end = parseint::readint<10>(in.begin(), in.size());
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
		int width = utf8_encode(buf, parseint::readint<16>(in.begin()+2, 4));
		buffer.append(buf, width);
	}};

	template<>
	struct action<account> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		Id id = acc.id;

		const Account& acc_ref = accounts_by_id[id] = std::move(acc);
		acc = Account{};

		ids_by_sex[acc_ref.sex].insert(id);
		ids_by_domain[domain].insert(id);
		if (acc_ref.email) ids_sorted_by_email.insert(id);
		ids_by_status[acc_ref.status].insert(id);
		if (acc_ref.fname_idx) ids_by_fname[fnames[*acc_ref.fname_idx]].insert(id);
		if (acc_ref.sname_idx) ids_by_sname[snames[*acc_ref.sname_idx]].insert(id);
		if (acc_ref.phone) ids_by_code[acc_ref.phone->code].insert(id);
		ids_by_phone_presence[acc_ref.phone.has_value()].insert(id);
		if (acc_ref.country_idx) ids_by_country[countries[*acc_ref.country_idx]].insert(id);
		if (acc_ref.city_idx) ids_by_city[cities[*acc_ref.city_idx]].insert(id);
		for (const CoolHash hash : acc_ref.interest_idcs) ids_by_interest[interests[hash]].insert(id);
		if (*acc_ref.premium_start < current_ts && *acc_ref.premium_end > current_ts) ids_by_premium_now.insert(id);
		ids_by_premium_presence[acc_ref.premium_start.has_value()].insert(id);

		domain.clear();
	}};
}

namespace request_grammar {
	namespace pegtl = tao::pegtl;

	template<class> struct nat : pegtl::seq<pegtl::range<'1', '9'>, pegtl::star<pegtl::digit>> {};
	template<class Key, class Value>
	struct param : pegtl::seq<Key, pegtl::one<'='>, Value> {};
	template<class... Params>
	struct params : pegtl::list<pegtl::sor<Params...>, pegtl::one<'&'>> {};
	struct crlf : TAO_PEGTL_STRING("\r\n") {};
	struct http_version : pegtl::seq<TAO_PEGTL_STRING(" HTTP/1.1"), crlf> {};
	template<class Key, class Value>
	struct header : pegtl::seq<Key, TAO_PEGTL_STRING(": "), Value, crlf> {};
	struct conn_close : TAO_PEGTL_ISTRING("close") {};
	struct conn_keepalive : TAO_PEGTL_ISTRING("keep-alive") {};
	struct headers : pegtl::until<pegtl::seq<crlf, crlf>, pegtl::sor<
		header<TAO_PEGTL_ISTRING("Connection"), pegtl::sor<conn_keepalive, conn_close>>,
		pegtl::seq<pegtl::until<crlf>, crlf>
	>> {};

	template<class> struct email_domain : pegtl::plus<pegtl::sor<pegtl::alnum, pegtl::one<'.'>>> {};
	template<class tag> struct email : pegtl::seq<pegtl::plus<pegtl::sor<pegtl::alnum, pegtl::one<'.', '-', '_'>>>, pegtl::one<'@'>, email_domain<tag>> {};

	template<class> struct sex_male : pegtl::one<'m'> {};
	template<class> struct sex_female : pegtl::one<'f'> {};
	template<class tag> struct sex : pegtl::sor<sex_male<tag>, sex_female<tag>> {};

	template<class> struct urlescaped_char : pegtl::seq<pegtl::one<'%'>, pegtl::rep<2, pegtl::xdigit>> {};
	template<class> struct urlescaped_whitespace : pegtl::one<'+'> {};
	template<class> struct urlallowed_char : pegtl::sor<pegtl::alnum, pegtl::one<'-', '.', '_', '~'>> {};
	template<class tag> struct urlescaped_string : pegtl::plus<pegtl::sor<urlallowed_char<tag>, urlescaped_char<tag>, urlescaped_whitespace<tag>>> {};

	template<class> struct status_single : TAO_PEGTL_STRING("%D1%81%D0%B2%D0%BE%D0%B1%D0%BE%D0%B4%D0%BD%D1%8B") {};
	template<class> struct status_relationship : TAO_PEGTL_STRING("%D0%B7%D0%B0%D0%BD%D1%8F%D1%82%D1%8B") {};
	template<class> struct status_undecided : TAO_PEGTL_STRING("%D0%B2%D1%81%D1%91%20%D1%81%D0%BB%D0%BE%D0%B6%D0%BD%D0%BE") {};
	template<class tag> struct status : pegtl::sor<status_undecided<tag>, status_relationship<tag>, status_single<tag>> {};

	template<class> struct null_0 : pegtl::one<'0'> {};
	template<class> struct null_1 : pegtl::one<'1'> {};
	template<class tag> struct null : pegtl::sor<null_0<tag>, null_1<tag>> {};

	template<template<class> class Item, class Sep, class tag> struct list : pegtl::list<Item<tag>, Sep> {};

	template<class> struct year : pegtl::rep<4, pegtl::digit> {};

	struct sex_eq_tag {};
	struct email_domain_tag {};
	struct email_lt_tag {};
	struct email_gt_tag {};
	struct status_eq_tag {};
	struct status_neq_tag {};
	struct sname_eq_tag {};
	struct sname_starts_tag {};
	struct sname_null_tag {};
	struct phone_code_tag {};
	struct phone_null_tag {};
	struct country_eq_tag {};
	struct country_null_tag {};
	struct city_eq_tag {};
	struct city_any_tag {};
	struct city_null_tag {};
	struct birth_lt_tag {};
	struct birth_gt_tag {};
	struct birth_year_tag {};
	struct interests_contains_tag {};
	struct interests_any_tag {};
	struct likes_contains_tag {};
	struct premium_now_tag {};
	struct premium_null_tag {};
	struct query_id_tag {};
	struct limit_tag {};

	struct newacc : pegtl::any {};
	struct updacc : pegtl::any {};
	struct newlikes : pegtl::any {};
	struct id_tag {};
	struct request : pegtl::sor<
		pegtl::seq<TAO_PEGTL_STRING("GET /accounts/"), pegtl::sor<
			pegtl::seq<TAO_PEGTL_STRING("filter/?"), params<
				param<TAO_PEGTL_STRING("sex_eq"), sex<sex_eq_tag>>,
				param<TAO_PEGTL_STRING("email_domain"), email_domain<email_domain_tag>>,
				param<TAO_PEGTL_STRING("email_lt"), email<email_lt_tag>>,
				param<TAO_PEGTL_STRING("email_gt"), email<email_gt_tag>>,
				param<TAO_PEGTL_STRING("status_eq"), status<status_eq_tag>>,
				param<TAO_PEGTL_STRING("status_neq"), status<status_neq_tag>>,
				param<TAO_PEGTL_STRING("sname_eq"), urlescaped_string<sname_eq_tag>>,
				param<TAO_PEGTL_STRING("sname_starts"), urlescaped_string<sname_starts_tag>>,
				param<TAO_PEGTL_STRING("sname_null"), null<sname_null_tag>>,
				param<TAO_PEGTL_STRING("phone_code"), nat<phone_code_tag>>,
				param<TAO_PEGTL_STRING("phone_null"), null<phone_null_tag>>,
				param<TAO_PEGTL_STRING("country_eq"), urlescaped_string<country_eq_tag>>,
				param<TAO_PEGTL_STRING("country_null"), null<country_null_tag>>,
				param<TAO_PEGTL_STRING("city_eq"), urlescaped_string<city_eq_tag>>,
				param<TAO_PEGTL_STRING("city_any"), list<urlescaped_string, pegtl::one<','>, city_any_tag>>,
				param<TAO_PEGTL_STRING("city_null"), null<city_null_tag>>,
				param<TAO_PEGTL_STRING("birth_lt"), nat<birth_lt_tag>>,
				param<TAO_PEGTL_STRING("birth_gt"), nat<birth_gt_tag>>,
				param<TAO_PEGTL_STRING("birth_year"), year<birth_year_tag>>,
				param<TAO_PEGTL_STRING("interests_contains"), list<urlescaped_string, pegtl::one<','>, interests_contains_tag>>,
				param<TAO_PEGTL_STRING("interests_any"), list<urlescaped_string, pegtl::one<','>, interests_any_tag>>,
				param<TAO_PEGTL_STRING("likes_contains"), list<nat, pegtl::one<','>, likes_contains_tag>>,
				param<TAO_PEGTL_STRING("premium_now"), nat<premium_now_tag>>,
				param<TAO_PEGTL_STRING("premium_null"), nat<premium_null_tag>>,
				param<TAO_PEGTL_STRING("query_id"), nat<query_id_tag>>,
				param<TAO_PEGTL_STRING("limit"), nat<limit_tag>>
			>>,
			pegtl::seq<TAO_PEGTL_STRING("group/?"), params<
				// ...
			>>,
			pegtl::seq<nat<id_tag>, pegtl::sor<
				pegtl::seq<TAO_PEGTL_STRING("/recommend/?"), params<
					// ...
				>>,
				pegtl::seq<TAO_PEGTL_STRING("/suggest/?"), params<
					// ...
				>>
			>>
		>>,
		pegtl::seq<TAO_PEGTL_STRING("POST /accounts/"), pegtl::sor<
			pegtl::seq<TAO_PEGTL_STRING("new/"), http_version, headers, newacc>,
			pegtl::seq<nat<id_tag>, pegtl::one<'/'>, http_version, headers, updacc>,
			pegtl::seq<TAO_PEGTL_STRING("likes/"), http_version, headers, newlikes>
		>>
	> {};

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
	std::cerr << fmt::format("accounts_by_id.dynamic: {}\n", accounts_by_id.dynamic.size());

	std::cerr << fmt::format("ids total: {}\n", ids_sorted_by_email.size());

	cnt = 0; for (const auto& x : ids_by_interest) cnt += x.size();
	std::cerr << fmt::format("ids_by_interest: {}\n", cnt);

	cnt = 0; for (const auto& x : accounts_by_id.static_) if (x) cnt += x->likes.size();
	std::cerr << fmt::format("likes: {}\n", cnt);
}

void build_likers() {
	accounts_by_id.foreach([&](Account& acc) {
		for (Like like : acc.likes) {
			accounts_by_id[like.other].likers.insert(acc.id);
		}
	});
}

#include <iostream>
int main(int argc, char** argv) {
	namespace pegtl = tao::pegtl;
	if (*argv[1] == 'j') {
		Account acc{};
		email_t domain{};
		std::string buffer;
		Like like{};
		std::vector<Like> likes;
		for (int i = 2; i < argc; ++i) {
			pegtl::file_input in(argv[i]);
			pegtl::parse<pegtl::must<account_grammar::file>, account_grammar::action>(in, acc, domain, buffer, like);
		}

		build_likers();
		report();
	} else if (*argv[1] == 'r') {
		pegtl::file_input in(argv[2]);
		using namespace request_grammar;
		pegtl::parse<pegtl::must<request_grammar::request>>(in);
	}
}
