#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace FlowUi::test {

class CheckFailure final : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

class Runner {
public:
	template <typename Function>
	void run(std::string_view name, Function&& function) {
		try {
			std::invoke(std::forward<Function>(function));
			++passed_;
			std::cout << "[PASS] " << name << '\n';
		} catch (const std::exception& error) {
			++failed_;
			std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
		} catch (...) {
			++failed_;
			std::cerr << "[FAIL] " << name << ": unknown exception\n";
		}
	}

	[[nodiscard]] int finish() const {
		std::cout << passed_ << " passed, " << failed_ << " failed\n";
		return failed_ == 0 ? 0 : 1;
	}

private:
	int passed_ = 0;
	int failed_ = 0;
};

inline void check(bool condition, const char* expression, const char* file, int line) {
	if (condition) return;
	throw CheckFailure(
		std::string(file) + ':' + std::to_string(line) + ": check failed: " + expression);
}

template <typename Function>
void checkThrows(Function&& function, const char* expression, const char* file, int line) {
	try {
		std::invoke(std::forward<Function>(function));
	} catch (...) {
		return;
	}
	throw CheckFailure(
		std::string(file) + ':' + std::to_string(line) + ": expected exception: " + expression);
}

} // namespace FlowUi::test

#define FLOWUI_CHECK(...) \
	::FlowUi::test::check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__, __FILE__, __LINE__)

#define FLOWUI_CHECK_THROWS(...) \
	::FlowUi::test::checkThrows([&] { static_cast<void>((__VA_ARGS__)); }, #__VA_ARGS__, __FILE__, __LINE__)
