#include "devSystems/devTooling/override/DevOverrideTypes.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/Public/TextureRef.hpp"
#include "FlowUi/Private/TypeOperations.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace FlowUi::devSystems::tooling {

DevOwnedValue::DevOwnedValue(const DevOwnedValue& other) {
	if (!other)
		return;
	const tooling::DevTypeSchema schema{
		.id = other.type(),
		.size = other.size_,
		.alignment = other.alignment_,
	};
	try {
		if (initializeCopy(schema, *other.operations_, other.data()) !=
			tooling::DevValueOperationStatus::Success)
			throw std::runtime_error("Cannot copy the reflected payload.");
		if (other.retained_text_)
			for (const auto& reference : *other.retained_text_) {
				auto& field = *reinterpret_cast<std::string_view*>(
					static_cast<std::byte*>(data()) + reference.offset);
				if (!retain_text_reference(field, std::string(field)))
					throw std::bad_alloc{};
			}
	} catch (...) {
		reset();
		throw;
	}
}

DevOwnedValue& DevOwnedValue::operator=(const DevOwnedValue& other) {
	if (this == &other)
		return *this;
	DevOwnedValue replacement(other);
	*this = std::move(replacement);
	return *this;
}

DevOwnedValue::DevOwnedValue(DevOwnedValue&& other) noexcept {
	moveFrom(std::move(other));
}

DevOwnedValue& DevOwnedValue::operator=(DevOwnedValue&& other) noexcept {
	if (this == &other)
		return *this;
	reset();
	moveFrom(std::move(other));
	return *this;
}

const void* DevOwnedValue::data() const noexcept {
	return heap_ ? allocation_.get() : inline_.data();
}

void* DevOwnedValue::data() noexcept {
	return heap_ ? allocation_.get() : inline_.data();
}

std::size_t DevOwnedValue::heapBytes() const noexcept {
	std::size_t bytes = heap_ ? size_ : 0;
	if (retained_text_) {
		bytes += sizeof(*retained_text_) + retained_text_->capacity() * sizeof(RetainedTextReference);
		for (const auto& reference : *retained_text_) {
			if (!reference.text)
				continue;
			bytes += sizeof(std::string);
			const auto object_address = reinterpret_cast<std::uintptr_t>(reference.text.get());
			const auto data_address = reinterpret_cast<std::uintptr_t>(reference.text->data());
			if (data_address < object_address || data_address >= object_address + sizeof(std::string))
				bytes += reference.text->capacity() + 1;
		}
	}
	return bytes;
}

void DevOwnedValue::reset() noexcept {
	if (operations_)
		operations_->destroy(data());
	allocation_.reset();
	retained_text_.reset();
	operations_ = nullptr;
	size_ = 0;
	alignment_ = 0;
	heap_ = false;
}

bool DevOwnedValue::retain_text_reference(std::string_view& field, std::string text) noexcept {
	if (!*this || size_ < sizeof(std::string_view))
		return false;
	const auto base = reinterpret_cast<std::uintptr_t>(data());
	const auto address = reinterpret_cast<std::uintptr_t>(std::addressof(field));
	if (address < base || address - base > size_ - sizeof(std::string_view))
		return false;
	try {
		auto retained = std::make_unique<const std::string>(std::move(text));
		if (!retained_text_) {
			retained_text_ = std::make_unique<std::vector<RetainedTextReference>>();
			retained_text_->reserve(1);
		}
		const auto offset = static_cast<std::size_t>(address - base);
		for (auto& reference : *retained_text_)
			if (reference.offset == offset) {
				reference.text = std::move(retained);
				field = *reference.text;
				return true;
			}
		retained_text_->emplace_back(RetainedTextReference{std::move(retained), offset});
		field = *retained_text_->back().text;
		return true;
	} catch (...) {
		return false;
	}
}

bool DevOwnedValue::retain_schema_text_references(const tooling::DevSchemaGeneration& schema,
												  tooling::DevTypeIndex type) noexcept {
	const auto retain = [&](const auto& self, tooling::DevTypeIndex current, void* destination,
							std::uint32_t depth) -> bool {
		const auto* descriptor = schema.type(current);
		if (!descriptor || !destination || depth > 16)
			return false;
		if (descriptor->id == ::FlowUi::detail::typeHash<TextureRef>()) {
			auto& texture = *static_cast<TextureRef*>(destination);
			return texture.sourceKey.empty() ||
				   retain_text_reference(texture.sourceKey, std::string(texture.sourceKey));
		}
		if (descriptor->kind != tooling::DevTypeKind::Object)
			return true;
		for (const auto& field : schema.fieldsOf(current)) {
			if (field.operations >= schema.fieldOperations.size())
				return false;
			const auto* access = schema.fieldOperations[field.operations];
			if (access && access->mutableAddress &&
				!self(self, field.valueType, access->mutableAddress(destination), depth + 1))
				return false;
		}
		return true;
	};
	try {
		return retain(retain, type, data(), 0);
	} catch (...) {
		return false;
	}
}

tooling::DevValueOperationStatus DevOwnedValue::copyFrom(const tooling::DevSchemaGeneration& schema,
														 tooling::DevTypeIndex type,
														 const void* source,
														 DevOwnedValue& destination) noexcept {
	const tooling::DevTypeSchema* typeSchema = schema.type(type);
	if (!typeSchema || type.value >= schema.typeOperations.size()) {
		return tooling::DevValueOperationStatus::Unsupported;
	}
	const tooling::DevTypeOps* operations = schema.typeOperations[type.value];
	if (!operations || operations->type != typeSchema->id) {
		return tooling::DevValueOperationStatus::Unsupported;
	}
	try {
		DevOwnedValue replacement;
		const tooling::DevValueOperationStatus status =
			replacement.initializeCopy(*typeSchema, *operations, source);
		if (status == tooling::DevValueOperationStatus::Success) {
			if (!replacement.retain_schema_text_references(schema, type))
				return tooling::DevValueOperationStatus::Failed;
			destination = std::move(replacement);
		}
		return status;
	} catch (...) {
		return tooling::DevValueOperationStatus::Failed;
	}
}

tooling::DevValueOperationStatus
DevOwnedValue::initializeCopy(const tooling::DevTypeSchema& schema,
	const tooling::DevTypeOps& operations, const void* source) {
	if (source == nullptr)
		return tooling::DevValueOperationStatus::NullSource;
	if ((!operations.copyConstruct && !operations.copyConstructOrThrow) ||
		!operations.destroy || schema.size == 0 || schema.alignment == 0) {
		return tooling::DevValueOperationStatus::Unsupported;
	}

	size_ = schema.size;
	alignment_ = schema.alignment;
	heap_ = size_ > InlineBytes || alignment_ > alignof(std::max_align_t) ||
		!operations.moveConstructNoexcept;
	if (heap_) {
		allocation_.get_deleter().alignment = alignment_;
		allocation_.reset(static_cast<std::byte*>(::operator new(size_, std::align_val_t{alignment_})));
	}
	const tooling::DevValueOperationStatus status = operations.copyConstructOrThrow
		? (operations.copyConstructOrThrow(source, data()), tooling::DevValueOperationStatus::Success)
		: operations.copyConstruct(source, data());
	if (status != tooling::DevValueOperationStatus::Success) {
		reset();
		return status;
	}
	operations_ = &operations;
	return tooling::DevValueOperationStatus::Success;
}

void DevOwnedValue::moveFrom(DevOwnedValue&& other) noexcept {
	if (!other)
		return;
	if (other.heap_) {
		allocation_ = std::move(other.allocation_);
		operations_ = other.operations_;
		size_ = other.size_;
		alignment_ = other.alignment_;
		heap_ = true;
		other.operations_ = nullptr;
		other.size_ = 0;
		other.alignment_ = 0;
		other.heap_ = false;
		retained_text_ = std::move(other.retained_text_);
		return;
	}

	other.operations_->moveConstructNoexcept(other.data(), inline_.data());
	size_ = other.size_;
	alignment_ = other.alignment_;
	operations_ = other.operations_;
	heap_ = false;
	retained_text_ = std::move(other.retained_text_);
	other.reset();
}

} // namespace FlowUi::devSystems::tooling

#endif
