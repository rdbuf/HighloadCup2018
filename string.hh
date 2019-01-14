#pragma once

#include <string_view>
#include <cstring>

#include <fmt/format.h>
#include <iostream>

template<size_t Len, size_t BytesPerSymbol>
struct fixwstr { // unsafe fixed-width string without boundary checks
	char str[Len*BytesPerSymbol+1];
	char* ptr = str;

	fixwstr() = default;
	fixwstr(const char* a, size_t sz) noexcept {
		std::memcpy(str, a, sz);
		ptr = str + sz;
	}
	fixwstr(const fixwstr& other) noexcept {
		std::memcpy(str, other.str, other.size());
		ptr = str + other.size();
	}
	fixwstr(fixwstr&& other) noexcept {
		std::memcpy(str, other.str, other.size());
		ptr = str + other.size();
		other.clear();
	}
	fixwstr& operator=(const fixwstr& other) noexcept {
		std::memcpy(str, other.str, other.size());
		ptr = str + other.size();
		return *this;
	}
	fixwstr& operator=(fixwstr&& other) noexcept {
		std::memcpy(str, other.str, other.size());
		ptr = str + other.size();
		other.clear();
		return *this;
	}
	~fixwstr() { clear(); }

	void construct_from(const char* a, size_t sz) noexcept {
		std::memcpy(str, a, sz);
		ptr = str + sz;
	}
	void clear() { ptr = str; }

	operator std::string_view() { return std::string_view(str, ptr - str); }
	operator const std::string_view() const { return std::string_view(str, ptr - str); }

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

// #define FMT_STRING_ALIAS 1
// #include <fmt/format.h>
// #undef FMT_STRING_ALIAS

// internal::format_type<typename buffer_context<Char>::type, T>::value = false;

// template<size_t N, size_t M>
// struct fmt::formatter<fixwstr<N, M>> : formatter<std::string_view> {
// 	template <class FormatContext>
// 	auto format(fixwstr<N, M> str, FormatContext &ctx) {
// 		return formatter<std::string_view>::format(str, ctx);
// 	}
// };
