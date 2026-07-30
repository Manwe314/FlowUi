#pragma once

#include <stdexcept>
#include <string>

#include "Vulkan/Vk_Context.hpp"

namespace FlowUi::test {

class VulkanUnavailable final : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

class HeadlessVulkanFixture {
public:
	HeadlessVulkanFixture();
	~HeadlessVulkanFixture();

	HeadlessVulkanFixture(const HeadlessVulkanFixture&) = delete;
	HeadlessVulkanFixture& operator=(const HeadlessVulkanFixture&) = delete;

	[[nodiscard]] VulkanContext& context() noexcept { return context_; }
	[[nodiscard]] const std::string& deviceName() const noexcept { return deviceName_; }
	[[nodiscard]] bool hasNonCoherentHostVisibleMemory() const noexcept {
		return hasNonCoherentHostVisibleMemory_;
	}

private:
	void create();
	void reset() noexcept;

	VulkanContext context_{};
	std::string deviceName_;
	bool hasNonCoherentHostVisibleMemory_ = false;
};

} // namespace FlowUi::test
