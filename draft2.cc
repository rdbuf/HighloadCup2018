#include <string>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <array>

#define FMT_STRING_ALIAS 1
#include <fmt/format.h>
#include <fmt/ostream.h>
#undef FMT_STRING_ALIAS

/* Definition of domain specific types */

#include "set.hh"
#include <tsl/array_map.h>
#include <tsl/hopscotch_map.h>
#include <unordered_set>

#include "common-types.hh"

std::unordered_map<CoolHash, name_t> fnames;
std::shared_mutex fnames_mutex;
std::unordered_map<CoolHash, name_t> snames;
std::shared_mutex snames_mutex;
std::unordered_map<CoolHash, country_t> countries;
std::shared_mutex countries_mutex;
std::unordered_map<CoolHash, city_t> cities;
std::shared_mutex cities_mutex;
std::unordered_map<CoolHash, interest_t> interests;
std::shared_mutex interests_mutex;

#include "account.hh"
#include "like.hh"
#include "string.hh"

/* Indices */

#include <tsl/array_map.h>
#include <tsl/htrie_map.h>
#include <unordered_map>
#include <utility>
#include <type_traits>
struct AccountsStore {
	constexpr static size_t N = 1600000;

	std::array<std::optional<Account>, N> static_; // ids are considered mostly contiguous
	std::array<std::shared_mutex, N> mutexes;
	std::unordered_map<Id, Account> dynamic;
	std::shared_mutex mutex_dynamic;
	std::unordered_map<Id, std::shared_mutex> mutexes_dynamic;

	template<class F>
	void foreach(F f) {
		for (std::optional<Account>& acc : static_) if (acc) f(*acc);
		std::unique_lock lk(mutex_dynamic);
		for (const std::pair<const Id, Account>& acc : dynamic) f(const_cast<Account&>(acc.second));
	}

	std::shared_mutex& corresponding_mutex(Id idx) {
		if (idx < static_.size()) {
			return mutexes[idx];
		} else {
			std::shared_lock lk(mutex_dynamic);
			return mutexes_dynamic[idx];
		}
	}

	// was tailored for move-only types
	// template<class T, template<class...> class C, class... Ts, size_t... I>
	// auto cons_helper(T&& head, C<Ts...>&& tail, std::index_sequence<I...>) {
	// 	return C<T, Ts...>(std::move(head), std::move(std::get<I>(tail))...);
	// }

	// template<class T, template<class...> class C, class... Ts>
	// auto cons(T&& head, C<Ts...>&& tail) -> C<T, Ts...> {
	// 	return cons_helper(std::move(head), std::move(tail), std::index_sequence_for<Ts...>());
	// }
	template<class T, size_t N, size_t... I>
	auto cons_helper(std::array<T, 1>&& head, std::array<T, N>&& tail, std::index_sequence<I...>) -> std::array<T, N+1> {
		return std::array<T, N+1>{std::move(head[0]), std::move(std::get<I>(tail))...};
	}

	template<class T, size_t N>
	auto cons(std::array<T, 1>&& head, std::array<T, N>&& tail) -> std::array<T, N+1> {
		return cons_helper(std::move(head), std::move(tail), std::make_index_sequence<N>());
	}



	// template<size_t N, class T> struct TupleN_helper { using type = decltype(cons(std::declval<T>(), std::declval<TupleN_helper<N-1, T>>())); };
	// template<class T> struct TupleN_helper<1, T> { using type = std::tuple<T>; };
	// template<size_t N, class T> using TupleN = typename TupleN_helper<N, T>::type;
	// template<size_t N, class T>
	// using TupleN = std::array<T, N>;

	template<template<class> class LockT, class T>
	using const_qualified = std::conditional_t<std::is_same_v<LockT<std::shared_mutex>, std::shared_lock<std::shared_mutex>>, const T,
		std::conditional_t<std::is_same_v<LockT<std::shared_mutex>, std::unique_lock<std::shared_mutex>>, T, void>>;

	// template<class... OptionalSharedLock>
	// void process_optional_locks_helper(OptionalSharedLock lk, OptionalSharedLock... lks) {
	// 	if (lk) return cons(lk, process_optional_locks_helper(lks...));
	// 	else return process_optional_locks_helper(lks...);
	// }
	// template

	template<template<class> class LockT, bool ShouldCreate = false, class... IdT>
	std::array<std::tuple<std::optional<const_qualified<LockT, std::reference_wrapper<Account>>>, LockT<std::shared_mutex>, std::optional<std::shared_lock<std::shared_mutex>>>, sizeof...(IdT) + 1>
	get_and_lock_inner(Id id, IdT... ids) {
		auto result = cons(get_and_lock_inner<LockT, ShouldCreate>(id), get_and_lock_inner<LockT, ShouldCreate>(ids...));
		return result;
	}

	template<template<class> class LockT, bool ShouldCreate = false>
	std::array<std::tuple<std::optional<const_qualified<LockT, std::reference_wrapper<Account>>>, LockT<std::shared_mutex>, std::optional<std::shared_lock<std::shared_mutex>>>, 1>
	get_and_lock_inner(Id id) { // wow, that's even type-safe
		using acc_ref_t = std::optional<const_qualified<LockT, std::reference_wrapper<Account>>>;
		using ret_triplet = std::tuple<acc_ref_t, LockT<std::shared_mutex>, std::optional<std::shared_lock<std::shared_mutex>>>;
		using ret_val_t = std::array<ret_triplet, 1>;
		if (id < N) {
			std::shared_lock lk(mutexes[id]);
			if constexpr (!ShouldCreate) {
				return ret_val_t{ret_triplet{(static_[id].has_value() ? acc_ref_t(*static_[id]) : acc_ref_t(std::nullopt)), LockT(mutexes[id], std::defer_lock), std::nullopt}};
			} else {
				return ret_val_t{ret_triplet{(!static_[id].has_value() ? acc_ref_t(*(static_[id] = Account{})) : acc_ref_t(std::nullopt)), LockT(mutexes[id], std::defer_lock), std::nullopt}};
			}
		} else {
			std::shared_lock lk(mutex_dynamic);
			auto it = dynamic.find(id);
			if constexpr (ShouldCreate) {
				return ret_val_t{ret_triplet{(it != dynamic.end() ? acc_ref_t(it->second) : acc_ref_t(std::nullopt)), LockT(mutexes_dynamic[id], std::defer_lock), std::move(lk)}};
			} else {
				return ret_val_t{ret_triplet{(it == dynamic.end() ? acc_ref_t(dynamic[id]) : acc_ref_t(std::nullopt)), LockT(mutexes_dynamic[id], std::defer_lock), std::move(lk)}};
			}
		}
	}

	template<size_t... I>
	void get_and_lock_lock_helper(auto& result, std::index_sequence<I...>) {
		static_assert(sizeof...(I) > 0, "must specify at least one id");
		if constexpr (sizeof...(I) > 1) {
			std::lock(std::get<1>(std::get<I>(result))...);
		} else {
			std::get<1>(result[0]).lock();
		}
	}

	template<template<class> class LockT, bool ShouldCreate = false, class... IdT>
	std::array<std::tuple<std::optional<const_qualified<LockT, std::reference_wrapper<Account>>>, LockT<std::shared_mutex>, std::optional<std::shared_lock<std::shared_mutex>>>, sizeof...(IdT)>
	get_and_lock(IdT... ids) {
		auto result = get_and_lock_inner<LockT, ShouldCreate>(ids...);
		get_and_lock_lock_helper(result, std::make_index_sequence<sizeof...(ids)>());
		return result;
	}


	bool exists(Id idx) {
		if (idx < N) {
			std::shared_lock lk(mutexes[idx]);
			return static_[idx].has_value();
		} else {
			std::shared_lock lk(mutex_dynamic);
			return dynamic.find(idx) != dynamic.end();
		}
	}

	// 1. Removal never happens, only updates
	// 2. Map rehash needs an exclusive lock
	// Account& operator[](Id idx) noexcept {
	// 	if (idx < static_.size()) {
	// 		std::shared_lock lk(mutexes[idx]);
	// 		if (!static_[idx]) {
	// 			lk.unlock();
	// 			std::unique_lock lk_uniq(mutexes[idx]);
	// 			if (!static_[idx]) {
	// 				static_[idx] = Account{};
	// 			}
	// 			return *static_[idx];
	// 		} else {
	// 			return *static_[idx];
	// 		}
	// 	} else {
	// 		std::shared_lock lk(mutex_dynamic);
	// 		if (dynamic.find(idx) == dynamic.end()) {
	// 			lk.unlock();
	// 			std::unique_lock lk_uniq(mutex_dynamic);
	// 			if (dynamic.find(idx) == dynamic.end()) {
	// 				dynamic[idx];
	// 			}
	// 			return dynamic[idx];
	// 		} else {
	// 			return dynamic[idx];
	// 		}
	// 	}
	// }
} accounts_by_id;
// all this global shit is to be protected by shared_mutexes
std::array<set<Id>, 2> ids_by_sex;
std::shared_mutex ids_by_sex_mutex;
tsl::array_map<char, set<Id>> ids_by_domain;
std::shared_mutex ids_by_domain_mutex;
struct cmp_id_by_email { bool operator()(Id a, Id b) {
	// std::shared_lock lk1(accounts_by_id.corresponding_mutex(a), std::defer_lock), lk2(accounts_by_id.corresponding_mutex(b), std::defer_lock);
	// std::lock(lk1, lk2);
	// return *accounts_by_id[a].email < *accounts_by_id[b].email;
	auto [acc1, acc2] = accounts_by_id.get_and_lock<std::shared_lock>(a, b);
	return std::get<0>(acc1)->get().email < std::get<0>(acc2)->get().email;
}};
set<Id, cmp_id_by_email> ids_sorted_by_email;
std::shared_mutex ids_sorted_by_email_mutex;
std::array<set<Id>, 3> ids_by_status;
std::shared_mutex ids_by_status_mutex;
tsl::array_map<char, set<Id>> ids_by_fname;
std::shared_mutex ids_by_fname_mutex;
tsl::htrie_map<char, set<Id>> ids_by_sname;
std::shared_mutex ids_by_sname_mutex;
enum { absent, present };
std::array<set<Id>, 2> ids_by_sname_presence;
std::shared_mutex ids_by_sname_presence_mutex;
tsl::hopscotch_map<uint16_t, set<Id>> ids_by_code;
std::shared_mutex ids_by_code_mutex;
std::array<set<Id>, 2> ids_by_phone_presence;
std::shared_mutex ids_by_phone_presence_mutex;
tsl::array_map<char, set<Id>> ids_by_country;
std::shared_mutex ids_by_country_mutex;
std::array<set<Id>, 2> ids_by_country_presence;
std::shared_mutex ids_by_country_presence_mutex;
tsl::array_map<char, set<Id>> ids_by_city;
std::shared_mutex ids_by_city_mutex;
std::array<set<Id>, 2> ids_by_city_presence;
std::shared_mutex ids_by_city_presence_mutex;
struct cmp_id_by_birth { bool operator()(Id a, Id b) { // all optionals are assumed to be present
	// std::shared_lock lk1(accounts_by_id.corresponding_mutex(a), std::defer_lock), lk2(accounts_by_id.corresponding_mutex(b), std::defer_lock);
	// std::lock(lk1, lk2);
	// return accounts_by_id[a].birth < accounts_by_id[b].birth;
	auto [acc1, acc2] = accounts_by_id.get_and_lock<std::shared_lock>(a, b);
	return std::get<0>(acc1)->get().birth < std::get<0>(acc2)->get().birth;
}};
set<Id, cmp_id_by_birth> ids_sorted_by_birth;
std::shared_mutex ids_sorted_by_birth_mutex;
tsl::array_map<char, set<Id>> ids_by_interest;
std::shared_mutex ids_by_interest_mutex;
std::array<set<Id>, 2> ids_by_premium_now;
std::shared_mutex ids_by_premium_now_mutex;
std::array<set<Id>, 2> ids_by_premium_presence;
std::shared_mutex ids_by_premium_presence_mutex;

std::unordered_set<uint64_t> existing_phones;
std::shared_mutex existing_phones_mutex;
std::unordered_set<std::string_view> existing_emails;
std::shared_mutex existing_emails_mutex;

int64_t current_ts;

#include "parseint.hh"

/* Parsers */

#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/icu/utf8.hpp>
#include <utf8_encode.c>

namespace account_grammar {
	namespace pegtl = tao::pegtl;

	struct wss : pegtl::star<pegtl::one<' ', '\n', '\t'>> {};
	template<char Opening, class Rule, char Closing>
	struct list : pegtl::seq<
		pegtl::one<Opening>, wss,
		pegtl::opt<pegtl::list<Rule, pegtl::one<','>, pegtl::space>>,
		wss, pegtl::one<Closing>
	> {};

	template<class> struct nat : pegtl::seq<pegtl::range<'1', '9'>, pegtl::star<pegtl::digit>> {};

	template<class tag> struct email_domain : pegtl::plus<pegtl::sor<pegtl::alnum, pegtl::one<'.'>>> {};
	template<class tag> struct email : pegtl::seq<pegtl::plus<pegtl::sor<pegtl::alnum, pegtl::one<'.', '-', '_'>>>, pegtl::one<'@'>, email_domain<tag>> {};

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

 	template<class tag> struct id_tag {};
	template<class tag> struct email_tag {};
	template<class tag> struct fname_tag {};
	template<class tag> struct sname_tag {};

	template<class> struct integer_sequence : pegtl::plus<pegtl::digit> {};

	template<class tag> struct phone_prefix_tag {};
	template<class tag> struct phone_code_tag {};
	template<class tag> struct phone_num_tag {};
	template<class tag>
	struct phone : pegtl::seq<
		nat<phone_prefix_tag<tag>>,
		pegtl::one<'('>, nat<phone_code_tag<tag>>, pegtl::one<')'>,
		integer_sequence<phone_num_tag<tag>>
	> {};

	template<class tag> struct sex_male : pegtl::one<'m'> {};
	template<class tag> struct sex_female : pegtl::one<'f'> {};
	template<class tag> struct sex : pegtl::sor<sex_male<tag>, sex_female<tag>> {};

	template<class tag> struct birth_tag {};
	template<class tag> struct country_tag {};
	template<class tag> struct city_tag {};
	template<class tag> struct joined_tag {};
	template<class tag> struct phone_tag {};
	template<class tag> struct sex_tag {};
	template<class tag> struct status_tag {};

	template<class tag>
	struct status_single : pegtl::sor<
		pegtl::seq<pegtl::one<0xd1>, pegtl::one<0x81>, pegtl::one<0xd0>, pegtl::one<0xb2>, pegtl::one<0xd0>, pegtl::one<0xbe>, pegtl::one<0xd0>, pegtl::one<0xb1>, pegtl::one<0xd0>, pegtl::one<0xbe>, pegtl::one<0xd0>, pegtl::one<0xb4>, pegtl::one<0xd0>, pegtl::one<0xbd>, pegtl::one<0xd1>, pegtl::one<0x8b>>, // "свободны"
		TAO_PEGTL_STRING(R"(\u0441\u0432\u043e\u0431\u043e\u0434\u043d\u044b)")
	> {};
	template<class tag>
	struct status_relationship : pegtl::sor<
		pegtl::seq<pegtl::one<0xd0>, pegtl::one<0xb7>, pegtl::one<0xd0>, pegtl::one<0xb0>, pegtl::one<0xd0>, pegtl::one<0xbd>, pegtl::one<0xd1>, pegtl::one<0x8f>, pegtl::one<0xd1>, pegtl::one<0x82>, pegtl::one<0xd1>, pegtl::one<0x8b>>, // "заняты"
		TAO_PEGTL_STRING(R"(\u0437\u0430\u043d\u044f\u0442\u044b)")
	> {};
	template<class tag>
	struct status_undecided : pegtl::sor<
		pegtl::seq<pegtl::one<0xd0>, pegtl::one<0xb2>, pegtl::one<0xd1>, pegtl::one<0x81>, pegtl::one<0xd1>, pegtl::one<0x91>, pegtl::one<0x20>, pegtl::one<0xd1>, pegtl::one<0x81>, pegtl::one<0xd0>, pegtl::one<0xbb>, pegtl::one<0xd0>, pegtl::one<0xbe>, pegtl::one<0xd0>, pegtl::one<0xb6>, pegtl::one<0xd0>, pegtl::one<0xbd>, pegtl::one<0xd0>, pegtl::one<0xbe>>, // "всё сложно"
		TAO_PEGTL_STRING(R"(\u0432\u0441\u0451 \u0441\u043b\u043e\u0436\u043d\u043e)")
	> {};
	template<class tag>
	struct status : pegtl::sor<status_single<tag>, status_relationship<tag>, status_undecided<tag>> {};

	template<class tag> struct interest_tag {};
	template<class tag> struct premium_start_tag {};
	template<class tag> struct premium_end_tag {};
	template<class tag> struct likee_id_tag {};
	template<class tag> struct like_ts_tag {};

	template<class tag>
	struct like : object<
		field<TAO_PEGTL_STRING("id"), nat<likee_id_tag<tag>>>,
		field<TAO_PEGTL_STRING("ts"), nat<like_ts_tag<tag>>>
	> {};

	template<class tag>
	struct account : object<
		field<TAO_PEGTL_STRING("id"), nat<id_tag<tag>>>,
		field<TAO_PEGTL_STRING("email"), quoted<email<tag>>>,
		field<TAO_PEGTL_STRING("fname"), quoted<unicode_string<fname_tag<tag>>>>,
		field<TAO_PEGTL_STRING("sname"), quoted<unicode_string<sname_tag<tag>>>>,
		field<TAO_PEGTL_STRING("phone"), quoted<phone<tag>>>,
		field<TAO_PEGTL_STRING("sex"), quoted<sex<tag>>>,
		field<TAO_PEGTL_STRING("birth"), nat<birth_tag<tag>>>,
		field<TAO_PEGTL_STRING("country"), quoted<unicode_string<country_tag<tag>>>>,
		field<TAO_PEGTL_STRING("city"), quoted<unicode_string<city_tag<tag>>>>,
		field<TAO_PEGTL_STRING("joined"), nat<joined_tag<tag>>>,
		field<TAO_PEGTL_STRING("status"), quoted<status<tag>>>,
		field<TAO_PEGTL_STRING("interests"), array<quoted<unicode_string<interest_tag<tag>>>>>,
		field<TAO_PEGTL_STRING("premium"), object<
			field<TAO_PEGTL_STRING("start"), nat<premium_start_tag<tag>>>,
			field<TAO_PEGTL_STRING("finish"), nat<premium_end_tag<tag>>>
		>>,
		field<TAO_PEGTL_STRING("likes"), array<like<tag>>>
	> {};
	struct intialboot_tag {};
	struct file : pegtl::seq<wss, object<field<TAO_PEGTL_STRING("accounts"), array<account<intialboot_tag>>>>> {};

	template<class Rule>
	struct action : pegtl::nothing<Rule> {};

	template<class tag>
	struct action<nat<id_tag<tag>>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.id = parseint::readint<10>(in.begin(), in.size());
	}};

	template<class tag>
	struct action<nat<likee_id_tag<tag>>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		like.likee = parseint::readint<10>(in.begin(), in.size());
	}};

	template<class tag>
	struct action<nat<like_ts_tag<tag>>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		like.ts = parseint::readint<10>(in.begin(), in.size());
	}};

	template<class tag>
	struct action<like<tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.likes.insert(like);
		like = Like{};
	}};

	template<class tag>
	struct action<unicode_string<fname_tag<tag>>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		CoolHash h = *(acc.fname_idx = coolhash(buffer.c_str(), buffer.length()));
		std::shared_lock lk(fnames_mutex);
		if (fnames.find(h) == fnames.end()) {
			lk.unlock();
			std::unique_lock lk_uniq(fnames_mutex);
			fnames[h] = name_t(buffer.c_str(), buffer.length());
		} else {
			assert(fnames.find(h)->second == std::string_view(buffer.c_str(), buffer.length()));
		}
		buffer.clear();
	}};

	template<class tag>
	struct action<unicode_string<sname_tag<tag>>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		CoolHash h = *(acc.sname_idx = coolhash(buffer.c_str(), buffer.length()));
		std::shared_lock lk(snames_mutex);
		if (snames.find(h) == snames.end()) {
			lk.unlock();
			std::unique_lock lk_uniq(fnames_mutex);
			snames[h] = name_t(buffer.c_str(), buffer.length());
		} else {
			assert(snames.find(h)->second == std::string_view(buffer.c_str(), buffer.length()));
		}
		buffer.clear();
	}};

	template<class tag>
	struct action<unicode_string<country_tag<tag>>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		CoolHash h = *(acc.country_idx = coolhash(buffer.c_str(), buffer.length()));
		std::shared_lock lk(countries_mutex);
		if (countries.find(h) == countries.end()) {
			lk.unlock();
			std::unique_lock lk_uniq(countries_mutex);
			countries[h] = country_t(buffer.c_str(), buffer.length());
		} else {
			assert(countries.find(h)->second == std::string_view(buffer.c_str(), buffer.length()));
		}
		buffer.clear();
	}};

	template<class tag>
	struct action<unicode_string<city_tag<tag>>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		CoolHash h = *(acc.city_idx = coolhash(buffer.c_str(), buffer.length()));
		std::shared_lock lk(countries_mutex);
		if (cities.find(h) == cities.end()) {
			lk.unlock();
			std::unique_lock lk_uniq(cities_mutex);
			cities[h] = city_t(buffer.c_str(), buffer.length());
		} else {
			assert(cities.find(h)->second == std::string_view(buffer.c_str(), buffer.length()));
		}
		buffer.clear();
	}};

	template<class tag>
	struct action<unicode_string<interest_tag<tag>>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		CoolHash h = coolhash(buffer.c_str(), buffer.length());
		acc.interest_idcs.insert(h);
		std::shared_lock lk(countries_mutex);
		if (interests.find(h) == interests.end()) {
			lk.unlock();
			std::unique_lock lk_uniq(interests_mutex);
			interests[h] = interest_t(buffer.c_str(), buffer.length());
		} else {
			assert(interests.find(h)->second == std::string_view(buffer.c_str(), buffer.length()));
		}
		buffer.clear();
	}};

	template<class tag>
	struct action<nat<phone_prefix_tag<tag>>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.phone = phone_t{}; // init the optional
		acc.phone->prefix = parseint::readint<10>(in.begin(), in.size());
	}};

	template<class tag>
	struct action<nat<phone_code_tag<tag>>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.phone->code = parseint::readint<10>(in.begin(), in.size());
	}};

	template<class tag>
	struct action<integer_sequence<phone_num_tag<tag>>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.phone->num = parseint::readint<10>(in.begin(), in.size());
		acc.phone->num_len = in.size();
	}};

	template<class tag>
	struct action<nat<phone_tag<tag>>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
	}};

	template<class tag>
	struct action<nat<birth_tag<tag>>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.birth = parseint::readint<10>(in.begin(), in.size());
	}};

	template<class tag>
	struct action<nat<joined_tag<tag>>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.joined = parseint::readint<10>(in.begin(), in.size());
	}};

	template<class tag>
	struct action<nat<premium_start_tag<tag>>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.premium_start = parseint::readint<10>(in.begin(), in.size());
	}};

	template<class tag>
	struct action<nat<premium_end_tag<tag>>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.premium_end = parseint::readint<10>(in.begin(), in.size());
	}};

	template<class tag>
	struct action<status_single<tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.status = single;
	}};

	template<class tag>
	struct action<status_relationship<tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.status = relationship;
	}};

	template<class tag>
	struct action<status_undecided<tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.status = undecided;
	}};

	template<class tag>
	struct action<email_domain<tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		domain = email_t(in.begin(), in.size());
	}};

	template<class tag>
	struct action<email<tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.email = email_t(in.begin(), in.size());
	}};

	template<class tag>
	struct action<sex_male<tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		acc.sex = male;
	}};

	template<class tag>
	struct action<sex_female<tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
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
	struct action<account<intialboot_tag>> { template<class Input> static void apply(const Input& in, Account& acc, email_t& domain, std::string& buffer, Like& like) {
		Id id = acc.id;

		auto [acc_] = accounts_by_id.get_and_lock<std::unique_lock, true>(id);
		auto& acc_ref = std::get<0>(acc_)->get();
		acc_ref = std::move(acc);
		acc = Account{};

		ids_by_sex[acc_ref.sex].insert(id);
		ids_by_domain[domain].insert(id);
 		ids_sorted_by_email.insert(id);
		existing_emails.insert(acc_ref.email);
		ids_by_status[acc_ref.status].insert(id);
		if (acc_ref.fname_idx) ids_by_fname[fnames[*acc_ref.fname_idx]].insert(id);
		if (acc_ref.sname_idx) {
			ids_by_sname[snames[*acc_ref.sname_idx]].insert(id);
		}
		ids_by_sname_presence[acc_ref.sname_idx.has_value()].insert(id);
		if (acc_ref.phone) {
			ids_by_code[acc_ref.phone->code].insert(id);
			existing_phones.insert(*acc_ref.phone);
		}
		ids_by_phone_presence[acc_ref.phone.has_value()].insert(id);
		if (acc_ref.country_idx) ids_by_country[countries[*acc_ref.country_idx]].insert(id);
		ids_by_country_presence[acc_ref.country_idx.has_value()].insert(id);
		if (acc_ref.city_idx) ids_by_city[cities[*acc_ref.city_idx]].insert(id);
		ids_by_city_presence[acc_ref.city_idx.has_value()].insert(id);
		for (const CoolHash hash : acc_ref.interest_idcs) ids_by_interest[interests[hash]].insert(id);
		if (acc_ref.premium_start.has_value() && acc_ref.premium_end.has_value()) {
			ids_by_premium_now[*acc_ref.premium_start < current_ts && *acc_ref.premium_end > current_ts].insert(id);
		}
		ids_by_premium_presence[acc_ref.premium_start.has_value()].insert(id);
		ids_sorted_by_birth.insert(id);

		domain.clear();
	}};
}

#define ASIO_STANDALONE
#include <asio.hpp>
#undef ASIO_STANDALONE

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
	struct content_length_tag {};
	struct headers : pegtl::until<pegtl::sor<crlf, pegtl::eof>, pegtl::sor<
		header<TAO_PEGTL_ISTRING("Connection"), pegtl::sor<conn_keepalive, conn_close>>,
		header<TAO_PEGTL_ISTRING("Content-Length"), nat<content_length_tag>>,
		pegtl::seq<pegtl::until<crlf>>
	>> {};

	template<class> struct email_domain : pegtl::plus<pegtl::sor<pegtl::alnum, pegtl::one<'.'>>> {};
	template<class tag> struct email : pegtl::seq<pegtl::plus<pegtl::sor<pegtl::alnum, pegtl::one<'.', '-', '_'>>>, pegtl::one<'@'>, email_domain<tag>> {};

	template<class tag> struct ascii_string : pegtl::seq<pegtl::print> {};

	template<class> struct sex_male : pegtl::one<'m'> {};
	template<class> struct sex_female : pegtl::one<'f'> {};
	template<class tag> struct sex : pegtl::sor<sex_male<tag>, sex_female<tag>> {};

	template<class> struct urlescaped_char : pegtl::seq<pegtl::one<'%'>, pegtl::rep<2, pegtl::xdigit>> {};
	template<class> struct urlescaped_whitespace : pegtl::one<'+'> {};
	template<class> struct urlallowed_char : pegtl::sor<pegtl::alnum, pegtl::one<'-', '.', '_', '~'>> {};
	template<class tag> struct urlescaped_string : pegtl::plus<pegtl::sor<urlallowed_char<tag>, urlescaped_char<tag>, urlescaped_whitespace<tag>>> {};

	template<class> struct status_single : TAO_PEGTL_STRING("%D1%81%D0%B2%D0%BE%D0%B1%D0%BE%D0%B4%D0%BD%D1%8B") {};
	template<class> struct status_relationship : TAO_PEGTL_STRING("%D0%B7%D0%B0%D0%BD%D1%8F%D1%82%D1%8B") {};
	template<class> struct status_undecided : TAO_PEGTL_STRING("%D0%B2%D1%81%D1%91+%D1%81%D0%BB%D0%BE%D0%B6%D0%BD%D0%BE") {};
	template<class tag> struct status : pegtl::sor<status_undecided<tag>, status_relationship<tag>, status_single<tag>> {};

	template<class> struct boolean_false : pegtl::one<'0'> {};
	template<class> struct boolean_true : pegtl::one<'1'> {};
	template<class tag> struct boolean : pegtl::sor<boolean_false<tag>, boolean_true<tag>> {};

	template<template<class> class Item, class Sep, class tag> struct list : pegtl::list<Item<tag>, Sep> {};

	struct query_id_tag{};
	template<class tag> struct int_ : pegtl::plus<pegtl::digit> {};
	struct query_id : param<TAO_PEGTL_STRING("query_id"), int_<query_id_tag>> {};

	template<class> struct year : pegtl::rep<4, pegtl::digit> {};

	namespace http_prefix {
		constexpr const char ok[] =
			"HTTP/1.1 200 \r\n"
			"Content-Type: application/json\r\n"
			"Connection: Keep-Alive\r\n"
			"Content-Length: ";
		constexpr const char creation_successful[] =
			"HTTP/1.1 201 \r\n"
			"Content-Type: application/json\r\n"
			"Connection: keep-alive\r\n"
			"Content-Length: 2\r\n"
			"\r\n"
			"{}";
		constexpr const char update_successful[] =
			"HTTP/1.1 202 \r\n"
			"Content-Type: application/json\r\n"
			"Connection: keep-alive\r\n"
			"Content-Length: 2\r\n"
			"\r\n"
			"{}";
		constexpr const char bad_request[] =
			"HTTP/1.1 400 \r\n"
			"Content-Type: application/json\r\n"
			"Connection: keep-alive\r\n"
			"Content-Length: 0\r\n";
		constexpr const char not_found[] =
			"HTTP/1.1 404 \r\n"
			"Content-Type: application/json\r\n"
			"Connection: keep-alive\r\n"
			"Content-Length: 0\r\n";
	}

	// 1. We get a request, parse it, and go to the appropriate handler
	// 2. Handler does two things: first, it constructs and sends an answer (asio::async_write), and after that, updates data if necessary (asio::post on another io_context)
	template<class Rule>
	struct action : pegtl::nothing<Rule> {};

	// must be thread_local
	#define ACTION_ARGS \
		set<Id>& selected_intersection, \
		set<Id>& local_union, \
		std::string& decoded, \
		std::bitset<Account::printable_n>& fields, \
		size_t& limit, \
		asio::ip::tcp::socket& socket, \
		asio::streambuf& buf, \
		bool& keepalive, \
		Id id
		// std::vector<set<Id>*>& intersect, \
		// std::vector<set<Id>*>& unify, \
		// asio::io_context& scheduler

	template<>
	struct action<nat<content_length_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		size_t content_length = parseint::readint<10>(in.begin(), in.size());
		asio::read(socket, buf, asio::transfer_exactly(content_length));
	}};

	struct limit_tag {};

	namespace filter {
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

		struct grammar : pegtl::seq<TAO_PEGTL_STRING("filter/?"), params<
			param<TAO_PEGTL_STRING("sex_eq"), sex<sex_eq_tag>>,
			param<TAO_PEGTL_STRING("email_domain"), email_domain<email_domain_tag>>,
			param<TAO_PEGTL_STRING("email_lt"), ascii_string<email_lt_tag>>,
			param<TAO_PEGTL_STRING("email_gt"), ascii_string<email_gt_tag>>,
			param<TAO_PEGTL_STRING("status_eq"), status<status_eq_tag>>,
			param<TAO_PEGTL_STRING("status_neq"), status<status_neq_tag>>,
			param<TAO_PEGTL_STRING("sname_eq"), urlescaped_string<sname_eq_tag>>,
			param<TAO_PEGTL_STRING("sname_starts"), urlescaped_string<sname_starts_tag>>,
			param<TAO_PEGTL_STRING("sname_null"), boolean<sname_null_tag>>,
			param<TAO_PEGTL_STRING("phone_code"), nat<phone_code_tag>>,
			param<TAO_PEGTL_STRING("phone_null"), boolean<phone_null_tag>>,
			param<TAO_PEGTL_STRING("country_eq"), urlescaped_string<country_eq_tag>>,
			param<TAO_PEGTL_STRING("country_null"), boolean<country_null_tag>>,
			param<TAO_PEGTL_STRING("city_eq"), urlescaped_string<city_eq_tag>>,
			param<TAO_PEGTL_STRING("city_any"), list<urlescaped_string, TAO_PEGTL_ISTRING("%2C"), city_any_tag>>,
			param<TAO_PEGTL_STRING("city_null"), boolean<city_null_tag>>,
			param<TAO_PEGTL_STRING("birth_lt"), nat<birth_lt_tag>>,
			param<TAO_PEGTL_STRING("birth_gt"), nat<birth_gt_tag>>,
			param<TAO_PEGTL_STRING("birth_year"), year<birth_year_tag>>,
			param<TAO_PEGTL_STRING("interests_contains"), list<urlescaped_string, TAO_PEGTL_ISTRING("%2C"), interests_contains_tag>>,
			param<TAO_PEGTL_STRING("interests_any"), list<urlescaped_string, TAO_PEGTL_ISTRING("%2C"), interests_any_tag>>,
			param<TAO_PEGTL_STRING("likes_contains"), list<nat, TAO_PEGTL_ISTRING("%2C"), likes_contains_tag>>,
			param<TAO_PEGTL_STRING("premium_now"), boolean<premium_now_tag>>,
			param<TAO_PEGTL_STRING("premium_null"), boolean<premium_null_tag>>,
			param<TAO_PEGTL_STRING("limit"), nat<limit_tag>>,
			pegtl::must<query_id>
		>> {};
	}

	template<>
	struct action<sex_male<filter::sex_eq_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_by_sex[male]);
	}};

	template<>
	struct action<sex_female<filter::sex_eq_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_by_sex[female]);
	}};

	template<>
	struct action<sex<filter::sex_eq_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		fields[Account::print_sex] = true;
	}};

	template<>
	struct action<email_domain<filter::email_domain_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		std::shared_lock lk(ids_by_domain_mutex);
		if (auto it = ids_by_domain.find_ks(in.begin(), in.size()); it != ids_by_domain.end()) {
			selected_intersection.intersect(*it);
		} else selected_intersection.clear();
	}};

	template<>
	struct action<ascii_string<filter::email_lt_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_sorted_by_email.partition<true>(std::string_view(in.begin(), in.size()), [&](Id id, std::string_view sv) {
			// std::shared_lock lk(accounts_by_id.corresponding_mutex(id));
			// return accounts_by_id[id].email < sv;
			auto [acc] = accounts_by_id.get_and_lock<std::shared_lock>(id);
			return std::get<0>(acc)->get().email < sv;
		}));
	}};

	template<>
	struct action<ascii_string<filter::email_gt_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_sorted_by_email.partition<false>(std::string_view(in.begin(), in.size()), [&](Id id, std::string_view sv) {
			// std::shared_lock lk(accounts_by_id.corresponding_mutex(id));
			// return accounts_by_id[id].email < sv;
			auto [acc] = accounts_by_id.get_and_lock<std::shared_lock>(id);
			return std::get<0>(acc)->get().email < sv;
		}));
	}};

	template<>
	struct action<status_undecided<filter::status_eq_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_by_status[undecided]);
	}};

	template<>
	struct action<status_single<filter::status_eq_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_by_status[single]);
	}};

	template<>
	struct action<status_relationship<filter::status_eq_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_by_status[relationship]);
	}};

	template<>
	struct action<status<filter::status_eq_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		fields[Account::print_status] = true;
	}};

	template<>
	struct action<status_undecided<filter::status_neq_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		local_union.unite(ids_by_status[relationship]);
		local_union.unite(ids_by_status[single]);
	}};

	template<>
	struct action<status_single<filter::status_neq_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		local_union.unite(ids_by_status[relationship]);
		local_union.unite(ids_by_status[undecided]);
	}};

	template<>
	struct action<status_relationship<filter::status_neq_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		local_union.unite(ids_by_status[single]);
		local_union.unite(ids_by_status[undecided]);
	}};

	template<>
	struct action<status<filter::status_neq_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(local_union);
		local_union.clear();
		fields[Account::print_status] = true;
	}};

	template<class tag>
	struct action<urlescaped_char<tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		decoded.push_back(parseint::readint<16>(in.begin()+1, 2));
	}};

	template<class tag>
	struct action<urlescaped_whitespace<tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		decoded.push_back(' ');
	}};

	template<class tag>
	struct action<urlallowed_char<tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		decoded.push_back(*in.begin());
	}};

	template<>
	struct action<urlescaped_string<filter::sname_eq_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		std::shared_lock lk(ids_by_sname_mutex);
		if (auto it = ids_by_sname.find(decoded); it != ids_by_sname.end()) {
			selected_intersection.intersect(*it);
		} else selected_intersection.clear();
		decoded.clear();
		fields[Account::print_sname] = true;
	}};

	template<>
	struct action<urlescaped_string<filter::sname_starts_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		std::shared_lock lk(ids_by_sname_mutex);
		const auto prefix_range = ids_by_sname.equal_prefix_range(decoded);
		for (auto it = prefix_range.first; it != prefix_range.second; ++it) {
			local_union.unite(*it);
		}
		selected_intersection.intersect(local_union);
		local_union.clear();
		decoded.clear();
		fields[Account::print_sname] = true;
	}};

	template<>
	struct action<boolean_true<filter::sname_null_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_by_sname_presence[absent]);
	}};

	template<>
	struct action<boolean_false<filter::sname_null_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_by_sname_presence[present]);
		fields[Account::print_sname] = true;
	}};

	template<>
	struct action<nat<filter::phone_code_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		std::shared_lock lk(ids_by_code_mutex);
		if (auto it = ids_by_code.find(parseint::readint<10>(in.begin(), in.size())); it != ids_by_code.end()) {
			selected_intersection.intersect(it->second);
		} else selected_intersection.clear();
		fields[Account::print_phone] = true;
	}};

	template<>
	struct action<boolean_false<filter::phone_null_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_by_phone_presence[present]);
		fields[Account::print_phone] = true;
	}};

	template<>
	struct action<boolean_true<filter::phone_null_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_by_phone_presence[absent]);
	}};

	template<>
	struct action<urlescaped_string<filter::country_eq_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		std::shared_lock lk(ids_by_country_mutex);
		if (auto it = ids_by_country.find(decoded); it != ids_by_country.end()) {
			selected_intersection.intersect(*it);
		} else selected_intersection.clear();
		decoded.clear();
		fields[Account::print_country] = true;
	}};

	template<>
	struct action<boolean_false<filter::country_null_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_by_country_presence[present]);
		fields[Account::print_country] = true;
	}};

	template<>
	struct action<boolean_true<filter::country_null_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_by_country_presence[absent]);
	}};

	template<>
	struct action<urlescaped_string<filter::city_eq_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		std::shared_lock lk(ids_by_city_mutex);
		if (auto it = ids_by_city.find(decoded); it != ids_by_city.end()) {
			selected_intersection.intersect(*it);
		}
		else selected_intersection.clear();
		decoded.clear();
		fields[Account::print_city] = true;
	}};

	template<>
	struct action<urlescaped_string<filter::city_any_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		std::shared_lock lk(ids_by_city_mutex);
		if (auto it = ids_by_city.find(decoded); it != ids_by_city.end()) local_union.unite(*it);
		decoded.clear();
	}};

	template<>
	struct action<list<urlescaped_string, pegtl::one<','>, filter::city_any_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		fields[Account::print_city] = true;
		selected_intersection.intersect(local_union);
		local_union.clear();
	}};

	template<>
	struct action<boolean_false<filter::city_null_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_by_city_presence[present]);
		fields[Account::print_city] = true;
	}};

	template<>
	struct action<boolean_true<filter::city_null_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_by_city_presence[absent]);
	}};

	template<>
	struct action<nat<filter::birth_lt_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_sorted_by_birth.partition<true>(parseint::readint<10>(in.begin(), in.size()), [&](Id id, uint32_t ts) {
			// std::scoped_lock lk(accounts_by_id.corresponding_mutex(id));
			// return accounts_by_id[id].birth < ts;
			auto [acc] = accounts_by_id.get_and_lock<std::shared_lock>(id);
			return std::get<0>(acc)->get().birth < ts;
		}));
		fields[Account::print_birth] = true;
	}};

	template<>
	struct action<nat<filter::birth_gt_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_sorted_by_birth.partition<false>(parseint::readint<10>(in.begin(), in.size()), [&](Id id, uint32_t ts) {
			// std::scoped_lock lk(accounts_by_id.corresponding_mutex(id));
			// return accounts_by_id[id].birth < ts;
			auto [acc] = accounts_by_id.get_and_lock<std::shared_lock>(id);
			return std::get<0>(acc)->get().birth < ts;
		}));
		fields[Account::print_birth] = true;
	}};

	template<>
	struct action<year<filter::birth_year_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		uint32_t year = parseint::readint<10>(in.begin(), in.size());
		uint32_t epoch_lower = ((year - 1970) * 365 + (year - 1970 + 1) / 4) * 86400;
		uint32_t epoch_upper = ((year + 1 - 1970) * 365 + (year + 1 - 1970 + 1) / 4) * 86400;

		selected_intersection.intersect(ids_sorted_by_birth.mid_partition(epoch_lower, epoch_upper, [](Id id, uint32_t ts) {
			// std::scoped_lock lk(accounts_by_id.corresponding_mutex(id));
			// return accounts_by_id[id].birth < ts;
			auto [acc] = accounts_by_id.get_and_lock<std::shared_lock>(id);
			return std::get<0>(acc)->get().birth < ts;
		}));
		fields[Account::print_birth] = true;
	}};

	template<>
	struct action<urlescaped_string<filter::interests_contains_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		std::shared_lock lk(ids_by_interest_mutex);
		if (auto it = ids_by_interest.find(decoded); it != ids_by_interest.end()) selected_intersection.intersect(*it);
		else selected_intersection.clear();
		decoded.clear();
	}};

	template<>
	struct action<urlescaped_string<filter::interests_any_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		std::shared_lock lk(ids_by_interest_mutex);
		if (auto it = ids_by_interest.find(decoded); it != ids_by_interest.end()) local_union.unite(*it);
		decoded.clear();
	}};

	template<>
	struct action<nat<filter::likes_contains_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		int likee_id = parseint::readint<10>(in.begin(), in.size());
		// std::shared_lock lk(accounts_by_id.corresponding_mutex(id));
		// selected_intersection.intersect(accounts_by_id[id].likers);
		auto [acc] = accounts_by_id.get_and_lock<std::shared_lock>(likee_id);
		if (std::get<0>(acc)) selected_intersection.intersect(std::get<0>(acc)->get().likers);
		else selected_intersection.clear();
	}};

	template<>
	struct action<list<urlescaped_string, TAO_PEGTL_ISTRING("%2C"), filter::interests_any_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(local_union);
		local_union.clear();
	}};

	template<>
	struct action<boolean_false<filter::premium_now_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_by_premium_now[absent]);
	}};

	template<>
	struct action<boolean_true<filter::premium_now_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		selected_intersection.intersect(ids_by_premium_now[present]);
		fields[Account::print_premium] = true;
	}};

	template<>
	struct action<nat<limit_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		limit = parseint::readint<10>(in.begin(), in.size());
	}};

	template<>
	struct action<conn_keepalive> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		keepalive = true;
	}};

	template<>
	struct action<filter::grammar> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		std::string body(R"({"accounts":[)");
		for (auto i = 0; i < limit && i < selected_intersection.size(); ++i) {
			auto [acc] = accounts_by_id.get_and_lock<std::shared_lock>(selected_intersection.elements[selected_intersection.size()-i-1]);
			std::get<0>(acc)->get().serialize_to(std::back_inserter(body), fields);
			body.push_back(',');
		}
		if (selected_intersection.size()) body.pop_back();
		body += "]}";
		std::vector<asio::const_buffer> buffers({
			asio::buffer(http_prefix::ok, sizeof(http_prefix::ok) - 1),
			asio::buffer(std::to_string(body.length())),
			asio::buffer("\r\n\r\n", 4),
			asio::buffer(body)
		});
		asio::write(socket, buffers);
	}};

	struct bad_uri_exception {};
	struct bad_request_exception {};

	template<class Rule>
	struct control : pegtl::normal<Rule> {
		template<class Input, class... States>
		static void raise(const Input& in, States&&...) { throw bad_request_exception{}; }
	};
	struct id_tag {};
	struct accounts_route : TAO_PEGTL_STRING("/accounts/") {};
	struct suggest_route : TAO_PEGTL_STRING("/suggest/?") {};

	template<>
	struct control<nat<id_tag>> : pegtl::normal<nat<id_tag>> {
		template<class Input, class... States>
		static void raise(const Input& in, States&&...) { throw bad_uri_exception{}; }
	};
	template<>
	struct control<accounts_route> : pegtl::normal<accounts_route> {
		template<class Input, class... States>
		static void raise(const Input& in, States&&...) { throw bad_uri_exception{}; }
	};
	template<>
	struct control<suggest_route> : pegtl::normal<suggest_route> {
		template<class Input, class... States>
		static void raise(const Input& in, States&&...) { throw bad_uri_exception{}; }
	};

	template<>
	struct action<nat<id_tag>> { template<class Input> static void apply(const Input& in, ACTION_ARGS) {
		id = parseint::readint<10>(in.begin(), in.size());
		if (!accounts_by_id.exists(id)) throw bad_uri_exception{};
	}};

	struct key_sex : TAO_PEGTL_STRING("sex") {};
	struct key_status : TAO_PEGTL_STRING("status") {};
	struct key_interests : TAO_PEGTL_STRING("interests") {};
	struct key_country : TAO_PEGTL_STRING("country") {};
	struct key_city : TAO_PEGTL_STRING("city") {};
	template<class> struct key : pegtl::sor<key_sex, key_status, key_interests, key_country, key_city> {};

	template<class> struct order_asc : pegtl::one<'1'> {};
	template<class> struct order_desc : pegtl::seq<pegtl::one<'-'>, pegtl::one<'1'>> {};
	template<class tag> struct order : pegtl::sor<order_asc<tag>, order_desc<tag>> {};

	namespace group {
		struct sex_tag {};
		struct status_tag {};
		struct country_tag {};
		struct city_tag {};
		struct birth_tag {};
		struct interests_tag {};
		struct joined_tag {};
		struct keys_tag {};
		struct query_id_tag {};
		struct order_tag {};

		struct grammar : pegtl::seq<TAO_PEGTL_STRING("group/?"), pegtl::must<params<
				param<TAO_PEGTL_STRING("sex"), sex<sex_tag>>,
				param<TAO_PEGTL_STRING("status"), status<status_tag>>,
				param<TAO_PEGTL_STRING("country"), urlescaped_string<country_tag>>,
				param<TAO_PEGTL_STRING("city"), urlescaped_string<city_tag>>,
				param<TAO_PEGTL_STRING("birth"), year<birth_tag>>,
				param<TAO_PEGTL_STRING("interests"), urlescaped_string<interests_tag>>,
				param<TAO_PEGTL_STRING("joined"), year<joined_tag>>,
				param<TAO_PEGTL_STRING("keys"), list<key, pegtl::one<','>, keys_tag>>,
				param<TAO_PEGTL_STRING("limit"), nat<limit_tag>>,
				param<TAO_PEGTL_STRING("order"), order<order_tag>>,
				pegtl::must<query_id>
		>>> {};
	}

	namespace recommend {
		struct country_tag {};
		struct city_tag {};
		struct query_id_tag {};

		struct grammar : pegtl::seq<TAO_PEGTL_STRING("/recommend/?"), pegtl::must<params<
			param<TAO_PEGTL_STRING("country"), urlescaped_string<country_tag>>,
			param<TAO_PEGTL_STRING("city"), urlescaped_string<city_tag>>,
			param<TAO_PEGTL_STRING("limit"), nat<limit_tag>>,
			pegtl::must<query_id>
		>>> {};
	}

	namespace suggest {
		struct country_tag {};
		struct city_tag {};
		struct query_id_tag {};

		struct grammar : pegtl::seq<pegtl::must<suggest_route>, pegtl::must<params<
			param<TAO_PEGTL_STRING("country"), urlescaped_string<country_tag>>,
			param<TAO_PEGTL_STRING("city"), urlescaped_string<city_tag>>,
			param<TAO_PEGTL_STRING("limit"), nat<limit_tag>>,
			pegtl::must<query_id>
		>>> {};
	}

	namespace newacc {
		struct newacc_tag {};
		struct grammar : pegtl::seq<TAO_PEGTL_STRING("new/?"), pegtl::must<pegtl::seq<query_id, http_version, headers, account_grammar::account<newacc_tag>>>> {};
	}

	namespace newlikes {
		struct likee_id_tag {};
		struct liker_id_tag {};
		struct like_ts_tag {};
		struct like : account_grammar::object<
			account_grammar::field<TAO_PEGTL_STRING("likee"), nat<likee_id_tag>>,
			account_grammar::field<TAO_PEGTL_STRING("liker"), nat<liker_id_tag>>,
			account_grammar::field<TAO_PEGTL_STRING("ts"), nat<like_ts_tag>>
		> {};
		struct newlikes : account_grammar::object<
			account_grammar::field<TAO_PEGTL_STRING("likes"), account_grammar::array<like>>
		> {};

		struct grammar : pegtl::seq<TAO_PEGTL_STRING("likes/?"), pegtl::must<pegtl::seq<query_id, http_version, headers, newlikes>>> {};
	}

	namespace updacc {
		struct updacc_tag {};
		struct grammar : pegtl::seq<TAO_PEGTL_STRING("/?"), pegtl::must<pegtl::seq<query_id, http_version, headers, account_grammar::account<updacc_tag>>>> {};
	}

	struct request : pegtl::sor<
		pegtl::seq<TAO_PEGTL_STRING("GET "), pegtl::must<accounts_route>, pegtl::sor<
			filter::grammar,
			group::grammar,
			pegtl::seq<pegtl::must<nat<id_tag>>, pegtl::sor<
				recommend::grammar,
				suggest::grammar
			>>
		>, http_version, headers>,
		pegtl::seq<TAO_PEGTL_STRING("POST "), pegtl::must<accounts_route>, pegtl::sor<
			newacc::grammar,
			newlikes::grammar,
			pegtl::seq<pegtl::must<nat<id_tag>>, updacc::grammar>
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
		"interests: {}\n"
		"existing_phones: {}\n"
		"existing_emails: {}\n",
		fnames.size(), snames.size(),
		countries.size(), cities.size(),
		interests.size(), existing_phones.size(),
		existing_emails.size()
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
			acc.likers.insert(acc.id);
		}
	});
}

void test_printing() {
	accounts_by_id.foreach([&](Account& acc) {
		acc.serialize_to(std::ostream_iterator<char>(std::cout), std::bitset<Account::printable_n>{}.set());
	});
}

#include <signal.h>
#include <memory>
struct server {
	size_t port;
	asio::io_context io_context;
	asio::io_context worker_context;
	asio::executor_work_guard<asio::io_context::executor_type> worker_context_guard;

	asio::ip::tcp::acceptor acceptor;
	asio::signal_set signals;
	std::vector<std::thread> pool;

	server(size_t port) :
		signals(io_context, SIGINT, SIGTERM, SIGQUIT),
		acceptor(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
		worker_context_guard(asio::make_work_guard(worker_context)),
		port(port) {}

	void start_accept(std::shared_ptr<asio::ip::tcp::socket> socket_ptr) {
		acceptor.async_accept(*socket_ptr, [&, socket_ptr](const std::error_code& accept_error) {
			if (accept_error) {
				std::cerr << fmt::format("error: {}\n", accept_error);
				start_accept(std::make_shared<asio::ip::tcp::socket>(io_context));
				return;
			}
			std::cerr << "accepted\n";

			auto buf_ptr = std::make_shared<asio::streambuf>();
			asio::async_read_until(*socket_ptr, *buf_ptr, "\r\n\r\n", [&, socket_ptr, buf_ptr](const std::error_code& read_error, size_t read_bytes) {
				if (read_error) {
					std::cerr << fmt::format("error: {}\n", read_error);
					start_accept(std::make_shared<asio::ip::tcp::socket>(io_context));
					return;
				}

				auto _something = buf_ptr->data();
				std::cerr << fmt::format("read: {:s}\n", std::string(asio::buffers_begin(_something), asio::buffers_begin(_something) + read_bytes));

				asio::post(worker_context, [&, socket_ptr, buf_ptr] {
					namespace pegtl = tao::pegtl;
					bool use_intersection = false, use_union = false;
					set<Id> selected_intersection; selected_intersection.universe = true;
					set<Id> local_union;
					std::bitset<Account::printable_n> fields;
					size_t limit = 0;
					bool keepalive = false;
					std::string decoded;
					Id id;

					std::istream is(&*buf_ptr);
					try {
						pegtl::parse<pegtl::must<request_grammar::request>, request_grammar::action, request_grammar::control>(pegtl::istream_input(is, read_bytes, ""), selected_intersection, local_union, decoded, fields, limit, *socket_ptr, *buf_ptr, keepalive, id);
					} catch (const request_grammar::bad_uri_exception&) {
						asio::write(*socket_ptr, asio::buffer(request_grammar::http_prefix::not_found));
					} catch (const request_grammar::bad_request_exception&) {
						asio::write(*socket_ptr, asio::buffer(request_grammar::http_prefix::bad_request));
					}
					if (keepalive) start_accept(std::move(socket_ptr));
				});
			});
			start_accept(std::make_shared<asio::ip::tcp::socket>(io_context));
		});
	}

	void stop() {
		// handle somehow!
		for (auto& thread : pool) if (thread.joinable()) thread.join();
	}

	~server() {
		stop();
	}

	void run() {
		signals.async_wait([&](const asio::error_code& error, int signal_number) { std::cerr << fmt::format("interrupt handler: {}\n", signal_number); exit(0); });
		std::cerr << fmt::format("[+] Listening on port {:d}\n", port);
		start_accept(std::make_shared<asio::ip::tcp::socket>(io_context));
		for (int i = 0; i < 3; ++i) {
			pool.emplace_back([&](){ worker_context.run(); });
		}
		io_context.run();
	}
};

#include <iostream>
#include <string>
#include <fstream>
int main(int argc, char** argv) {
	using namespace std::literals::string_literals;
	existing_phones.reserve(600000);
	existing_emails.reserve(AccountsStore::N);
	namespace pegtl = tao::pegtl;
	if (*argv[1] == 'j') {
		Account acc{};
		email_t domain{};
		std::string buffer;
		Like like{};
		std::ifstream(argv[3]) >> current_ts;
		std::cerr << fmt::format("[+] Current timestamp: {:d}\n", current_ts);
		for (int i = 4; i < argc; ++i) {
			pegtl::file_input in(argv[i]);
			std::cerr << fmt::format("\r[.] Parsing file {:d}/{:d}", i - 4 + 1, argc - 4);
			pegtl::parse<pegtl::must<account_grammar::file>, account_grammar::action>(in, acc, domain, buffer, like);
		}
		std::cerr << fmt::format("\r[+] Parsed successfully     \n");
		std::cerr << fmt::format("[.] Building likers");
		std::cerr << fmt::format("\r[+] Likers built       \n");
		build_likers();
		report();

		size_t port = parseint::readint<10>(argv[2], std::strlen(argv[2]));

		server s(port);
		s.run();
	}
}
