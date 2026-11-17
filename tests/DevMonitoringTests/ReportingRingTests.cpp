#include "devSystems/devMonitoringAndReporting/reporting/DevReportingRing.hpp"

#include <memory>
#include <stdexcept>

namespace {
struct Payload {
	int value = 0;

	[[nodiscard]] Payload* operator&() {
		throw std::runtime_error("Payload address operator must not run during iteration");
	}
	[[nodiscard]] const Payload* operator&() const {
		throw std::runtime_error("Const payload address operator must not run during iteration");
	}
};
} // namespace

int main() {
	FlowUi::devSystems::detail::ReportingRing<Payload> ring(2);
	ring.push_back(Payload{7});
	ring.push_back(Payload{11});
	ring.push_back(Payload{13});
	auto iterator = ring.begin();
	if (iterator.operator->() != std::addressof(*iterator) || iterator->value != 11)
		return 1;
	++iterator;
	if (iterator->value != 13)
		return 2;
	const auto& observed = ring;
	const auto constantIterator = observed.begin();
	if (constantIterator.operator->() != std::addressof(*constantIterator) ||
		constantIterator->value != 11)
		return 3;
	return 0;
}
