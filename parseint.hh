#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
namespace parseint {
	template<unsigned char base>
	constexpr uint64_t from_digit(char d) {
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

	template<uint64_t a>
	constexpr uint64_t pow_(uint64_t b) {
		if (b == 0) return 1;
		return a * parseint::pow_<a>(b - 1);
	}

	template<uint64_t a>
	constexpr std::array<uint64_t, 21> precomputed_powers = {
		pow_<a>(0), pow_<a>(1), pow_<a>(2), pow_<a>(3), pow_<a>(4), pow_<a>(5), pow_<a>(6), pow_<a>(7), pow_<a>(8), pow_<a>(9), pow_<a>(10),
		pow_<a>(11), pow_<a>(12), pow_<a>(13), pow_<a>(14), pow_<a>(15), pow_<a>(16), pow_<a>(17), pow_<a>(18), pow_<a>(19), pow_<a>(20)
	};

	template<uint64_t a>
	constexpr uint64_t pow(uint64_t b) {
		if (b < precomputed_powers<a>.size()) return precomputed_powers<a>[b];
		return a * parseint::pow<a>(b - 1);
	}

	template<unsigned char base>
	constexpr uint64_t readint(const char* sym, size_t num_digits) {
		uint64_t result = 0;
		for (size_t i = 0; i < num_digits; ++i) result += from_digit<base>(sym[num_digits-1 - i]) * parseint::pow<base>(i);
		return result;
	}

	template<unsigned char base, size_t num_digits>
	constexpr std::array<char, num_digits> showint(uint64_t value) {
		std::array<char, num_digits> result;
		for (int i = 0; i < num_digits; ++i) result[i] = to_digit<base>((value / parseint::pow<base>(num_digits-1 - i)) % base);
		return result;
	}
}