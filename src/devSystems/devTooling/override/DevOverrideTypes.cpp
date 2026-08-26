#include "devSystems/devTooling/override/DevOverrideTypes.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <utility>

namespace FlowUi::devSystems::tooling {

DevOwnedValue::DevOwnedValue(const DevOwnedValue& other) noexcept {
	if (!other) return;
	const devMode::DevTypeSchema schema{
		.id = other.type(),
		.size = other.size_,
		.alignment = other.alignment_,
	};
	(void)initializeCopy(schema, *other.operations_, other.data());
}

DevOwnedValue& DevOwnedValue::operator=(const DevOwnedValue& other) noexcept {
	if (this == &other) return *this;
	DevOwnedValue replacement(other);
	*this = std::move(replacement);
	return *this;
}

DevOwnedValue::DevOwnedValue(DevOwnedValue&& other) noexcept {
	moveFrom(std::move(other));
}

DevOwnedValue& DevOwnedValue::operator=(DevOwnedValue&& other) noexcept {
	if (this == &other) return *this;
	reset();
	moveFrom(std::move(other));
	return *this;
}

const void* DevOwnedValue::data() const noexcept {
	return heap_ ? allocation_ : inline_.data();
}

void* DevOwnedValue::data() noexcept {
	return heap_ ? allocation_ : inline_.data();
}

void DevOwnedValue::reset() noexcept {
	if (operations_) operations_->destroy(data());
	if (heap_ && allocation_) {
		::operator delete(allocation_, std::align_val_t{alignment_});
	}
	allocation_ = nullptr;
	operations_ = nullptr;
	size_ = 0;
	alignment_ = 0;
	heap_ = false;
}

devMode::DevValueOperationStatus DevOwnedValue::copyFrom(
	const devMode::DevSchemaGeneration& schema,
	devMode::DevTypeIndex type,
	const void* source,
	DevOwnedValue& destination) noexcept {
	const devMode::DevTypeSchema* typeSchema = schema.type(type);
	if (!typeSchema || type.value >= schema.typeOperations.size()) {
		return devMode::DevValueOperationStatus::Unsupported;
	}
	const devMode::DevTypeOps* operations = schema.typeOperations[type.value];
	if (!operations || operations->type != typeSchema->id) {
		return devMode::DevValueOperationStatus::Unsupported;
	}
	DevOwnedValue replacement;
	const devMode::DevValueOperationStatus status =
		replacement.initializeCopy(*typeSchema, *operations, source);
	if (status == devMode::DevValueOperationStatus::Success) {
		destination = std::move(replacement);
	}
	return status;
}

devMode::DevValueOperationStatus DevOwnedValue::initializeCopy(
	const devMode::DevTypeSchema& schema,
	const devMode::DevTypeOps& operations,
	const void* source) noexcept {
	if (source == nullptr) return devMode::DevValueOperationStatus::NullSource;
	if (!operations.copyConstruct || !operations.destroy ||
		schema.size == 0 || schema.alignment == 0) {
		return devMode::DevValueOperationStatus::Unsupported;
	}

	size_ = schema.size;
	alignment_ = schema.alignment;
	heap_ = size_ > InlineBytes || alignment_ > alignof(std::max_align_t);
	if (heap_) {
		try {
			allocation_ = ::operator new(size_, std::align_val_t{alignment_});
		} catch (...) {
			allocation_ = nullptr;
			operations_ = nullptr;
			return devMode::DevValueOperationStatus::Failed;
		}
	}
	const devMode::DevValueOperationStatus status = operations.copyConstruct(source, data());
	if (status != devMode::DevValueOperationStatus::Success) {
		if (heap_ && allocation_) {
			::operator delete(allocation_, std::align_val_t{alignment_});
		}
		allocation_ = nullptr;
		size_ = 0;
		alignment_ = 0;
		heap_ = false;
		return status;
	}
	operations_ = &operations;
	return devMode::DevValueOperationStatus::Success;
}

void DevOwnedValue::moveFrom(DevOwnedValue&& other) noexcept {
	if (!other) return;
	if (other.heap_) {
		allocation_ = other.allocation_;
		operations_ = other.operations_;
		size_ = other.size_;
		alignment_ = other.alignment_;
		heap_ = true;
		other.allocation_ = nullptr;
		other.operations_ = nullptr;
		other.size_ = 0;
		other.alignment_ = 0;
		other.heap_ = false;
		return;
	}

	const devMode::DevTypeSchema schema{
		.id = other.type(),
		.size = other.size_,
		.alignment = other.alignment_,
	};
	if (initializeMove(schema, *other.operations_, other.data()) ==
		devMode::DevValueOperationStatus::Success) {
		other.reset();
	}
}

devMode::DevValueOperationStatus DevOwnedValue::initializeMove(
	const devMode::DevTypeSchema& schema,
	const devMode::DevTypeOps& operations,
	void* source) noexcept {
	if (!operations.moveConstruct) return initializeCopy(schema, operations, source);
	size_ = schema.size;
	alignment_ = schema.alignment;
	heap_ = false;
	const devMode::DevValueOperationStatus status =
		operations.moveConstruct(source, inline_.data());
	if (status != devMode::DevValueOperationStatus::Success) {
		size_ = 0;
		alignment_ = 0;
		return status;
	}
	operations_ = &operations;
	return devMode::DevValueOperationStatus::Success;
}

} // namespace FlowUi::devSystems::tooling

#endif
