#include "managers/ActionManager.hpp"

#include <algorithm>
#include <stdexcept>
#if FLOW_UI_DEV_MODE
#include <iostream>
#endif

#include "FlowUi/App.hpp"
#include "internal/ManagerStorage/ActionManagerState.hpp"
#include "internal/ManagerStorage/ManagerStateAccess.hpp"
#include "internal/StorageSystem/IStorageSystem.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi {

namespace action = detail::action;
namespace manager_storage = detail::manager_storage;
namespace storage = detail::storage;

ActionManager::~ActionManager() {
	destroy();
}

void ActionManager::init(App& app, storage::IStorageSystem& storageSystem) {
	destroy();
	const storage::StringId name = storageSystem.intern("flowui.actions.root");
	const storage::ResourceKey key{
		storage::ResourceDomain::Action,
		name,
		InvalidWindowId,
	};
	const storage::ManagerRecordHandle handle =
		manager_storage::createState<manager_storage::ActionManagerState>(
			storageSystem,
			key,
			storage::ResourceKind::ActionManager,
			name);
	if (!manager_storage::state<manager_storage::ActionManagerState>(
			&storageSystem, handle, storage::ResourceKind::ActionManager)) {
		(void)storageSystem.removeManagerRecord(key, storage::ResourceKind::ActionManager);
		throw std::runtime_error("FlowUi ActionManager storage root publication failed.");
	}
	app_ = &app;
	storage_ = &storageSystem;
	stateHandle_ = handle.packed();
	stateName_ = name;
}

void ActionManager::destroy() noexcept {
	if (storage_ && stateHandle_ != 0) {
		if (auto* current = manager_storage::state<manager_storage::ActionManagerState>(
				storage_,
				storage::ManagerRecordHandle::fromPacked(stateHandle_),
				storage::ResourceKind::ActionManager)) {
			for (const auto& [_, handle] : current->bindings) {
				(void)storage_->removePersistentRecord(handle, storage::ResourceKind::AppActionBinding);
			}
			for (const auto handle : current->deferredRemovals) {
				(void)storage_->removePersistentRecord(handle, storage::ResourceKind::AppActionBinding);
			}
			current->bindings.clear();
			current->deferredRemovals.clear();
		}
		try {
			(void)storage_->removeManagerRecord(
				storage::ResourceKey{
					storage::ResourceDomain::Action,
					stateName_,
					InvalidWindowId,
				},
				storage::ResourceKind::ActionManager);
		} catch (...) {
		}
	}
	stateHandle_ = 0;
	stateName_ = 0;
	storage_ = nullptr;
	app_ = nullptr;
}

void ActionManager::rebindOwner(App& app) noexcept {
	if (storage_) app_ = &app;
}

void ActionManager::attachTo(UiManager& ui) noexcept {
	ui.setActionManager(this);
}

manager_storage::ActionManagerState& ActionManager::state() {
	auto* result = manager_storage::state<manager_storage::ActionManagerState>(
		storage_,
		storage::ManagerRecordHandle::fromPacked(stateHandle_),
		storage::ResourceKind::ActionManager);
	if (!result) throw std::logic_error("FlowUi ActionManager is not initialized.");
	return *result;
}

const manager_storage::ActionManagerState& ActionManager::state() const {
	const auto* result = manager_storage::state<manager_storage::ActionManagerState>(
		storage_,
		storage::ManagerRecordHandle::fromPacked(stateHandle_),
		storage::ResourceKind::ActionManager);
	if (!result) throw std::logic_error("FlowUi ActionManager is not initialized.");
	return *result;
}

void ActionManager::validateBindingRequest(AppActionID id, bool replacement) const {
	if (!id) throw std::invalid_argument("FlowUi app action id must be nonzero.");
	const bool exists = state().bindings.contains(id);
	if (!replacement && exists) {
		throw std::logic_error("FlowUi app action is already bound; use rebind().");
	}
	if (replacement && !exists) {
		throw std::logic_error("FlowUi app action rebind requires an existing binding.");
	}
}

void ActionManager::publishBinding(
	AppActionID id,
	storage::PersistentRecordHandle handle,
	bool replacement) {
	auto& current = state();
	if (!replacement) {
		const auto [_, inserted] = current.bindings.emplace(id, handle);
		if (!inserted) {
			throw std::logic_error("FlowUi app action binding changed during publication.");
		}
	} else {
		const auto found = current.bindings.find(id);
		if (found == current.bindings.end()) {
			throw std::logic_error("FlowUi app action disappeared during rebind publication.");
		}
		const storage::PersistentRecordHandle previous = found->second;
		found->second = handle;
		retireBinding(previous);
	}
	storage_->noteManagerMutation(InvalidWindowId);
}

void ActionManager::retireBinding(storage::PersistentRecordHandle handle) noexcept {
	if (!storage_ || !handle) return;
	const storage::PersistentRecordView view = storage_->persistentRecord(
		handle, storage::ResourceKind::AppActionBinding);
	if (!view || view.headerBytes < sizeof(action::AppActionBindingHeader)) return;
	auto& header = *static_cast<action::AppActionBindingHeader*>(view.header);
	header.tombstoned = true;
	if (header.activeInvocations == 0) {
		(void)storage_->removePersistentRecord(handle, storage::ResourceKind::AppActionBinding);
		return;
	}
	try {
		state().deferredRemovals.push_back(handle);
	} catch (...) {
		// The active lease still protects the record. Its release path attempts
		// direct removal even when bookkeeping allocation fails.
	}
}

ActionManager::InvocationLease::InvocationLease(InvocationLease&& other) noexcept
	: owner_(std::exchange(other.owner_, nullptr)),
	  handle_(std::exchange(other.handle_, {})),
	  header_(std::exchange(other.header_, nullptr)),
	  payload_(std::exchange(other.payload_, nullptr)) {}

ActionManager::InvocationLease& ActionManager::InvocationLease::operator=(
	InvocationLease&& other) noexcept {
	if (this == &other) return *this;
	release();
	owner_ = std::exchange(other.owner_, nullptr);
	handle_ = std::exchange(other.handle_, {});
	header_ = std::exchange(other.header_, nullptr);
	payload_ = std::exchange(other.payload_, nullptr);
	return *this;
}

ActionManager::InvocationLease::~InvocationLease() {
	release();
}

void ActionManager::InvocationLease::release() noexcept {
	if (owner_ && header_) owner_->releaseInvocation(handle_, *header_);
	owner_ = nullptr;
	handle_ = {};
	header_ = nullptr;
	payload_ = nullptr;
}

void ActionManager::InvocationLease::noteSuccess(bool discardedResult) noexcept {
#if FLOW_UI_DEV_MODE
	if (!header_) return;
	header_->lastStatus = ActionInvocationStatus::Invoked;
	header_->lastInvocationThrew = false;
	if (discardedResult && header_->resultTypeHash != 0) ++header_->discardedResultCount;
#else
	(void)discardedResult;
#endif
}

void ActionManager::InvocationLease::noteException() noexcept {
#if FLOW_UI_DEV_MODE
	if (header_) header_->lastInvocationThrew = true;
#endif
}

void ActionManager::releaseInvocation(
	storage::PersistentRecordHandle handle,
	action::AppActionBindingHeader& header) noexcept {
	if (header.activeInvocations > 0) --header.activeInvocations;
	if (!header.tombstoned || header.activeInvocations != 0 || !storage_) return;
	(void)storage_->removePersistentRecord(handle, storage::ResourceKind::AppActionBinding);
	if (!stateHandle_) return;
	try {
		auto& deferred = state().deferredRemovals;
		std::erase(deferred, handle);
	} catch (...) {
	}
}

ActionManager::InvocationLease ActionManager::beginInvocation(
	AppActionCall call,
	uint64_t requestedResultType,
	bool resultRequired,
	ActionInvocationSource source,
	ActionInvokeError& error) {
	if (!call) {
		error = ActionInvokeError::Empty;
		return {};
	}
	if (!storage_) {
		error = ActionInvokeError::Unbound;
		return {};
	}
	auto& current = state();
	const auto found = current.bindings.find(call.id);
	if (found == current.bindings.end()) {
		error = ActionInvokeError::Unbound;
#if FLOW_UI_DEV_MODE
		reportInvocationError(call, error);
#endif
		return {};
	}
	const storage::PersistentRecordHandle handle = found->second;
	const storage::PersistentRecordView view = storage_->persistentRecord(
		handle, storage::ResourceKind::AppActionBinding);
	if (!view || view.headerBytes < sizeof(action::AppActionBindingHeader)) {
		error = ActionInvokeError::Unbound;
#if FLOW_UI_DEV_MODE
		reportInvocationError(call, error);
#endif
		return {};
	}
	auto& header = *static_cast<action::AppActionBindingHeader*>(view.header);
	if (header.tombstoned) {
		error = ActionInvokeError::Unbound;
#if FLOW_UI_DEV_MODE
		reportInvocationError(call, error);
#endif
		return {};
	}
	if (!header.availability.enabled) {
		error = ActionInvokeError::Disabled;
#if FLOW_UI_DEV_MODE
		header.lastStatus = ActionInvocationStatus::Disabled;
		reportInvocationError(call, error);
#endif
		return {};
	}
	if (resultRequired) {
		if (header.resultTypeHash != requestedResultType ||
			(requestedResultType != 0 && header.invokeResult == nullptr)) {
			error = ActionInvokeError::ResultTypeMismatch;
#if FLOW_UI_DEV_MODE
			reportInvocationError(call, error);
#endif
			return {};
		}
	}
	++header.activeInvocations;
#if FLOW_UI_DEV_MODE
	++header.invocationCount;
	header.lastSource = source;
	header.lastInvocationThrew = false;
#else
	(void)source;
#endif
	return InvocationLease{*this, handle, header, view.payload};
}

#if FLOW_UI_DEV_MODE
void ActionManager::reportInvocationError(
	AppActionCall call,
	ActionInvokeError error) noexcept {
	if (!storage_ || !call || error == ActionInvokeError::Empty) return;
	try {
		const std::string_view name = call.id.debugName.empty()
			? std::string_view{"flowui.app_action.unnamed"}
			: call.id.debugName;
		const storage::StringId nameId = storage_->intern(name);
		const storage::ResourceKey key{
			storage::ResourceDomain::Action,
			nameId,
			InvalidWindowId,
		};
		const uint32_t diagnosticCode = 0xA000u + static_cast<uint32_t>(error);
		if (!storage_->markDiagnosticOnce(key, diagnosticCode)) return;
		const char* reason = "unknown";
		switch (error) {
		case ActionInvokeError::Unbound: reason = "unbound"; break;
		case ActionInvokeError::Disabled: reason = "disabled"; break;
		case ActionInvokeError::ResultTypeMismatch: reason = "result type mismatch"; break;
		case ActionInvokeError::Empty: return;
		}
		std::cerr << "FlowUi action diagnostic: '" << name
			<< "' invocation was " << reason << ".\n";
	} catch (...) {
	}
}
#endif

ActionInvocationStatus ActionManager::invokeApp(
	AppActionCall call,
	ActionInvocationSource source) {
	ActionInvokeError error = ActionInvokeError::Empty;
	auto lease = beginInvocation(call, 0, false, source, error);
	if (!lease) {
		switch (error) {
		case ActionInvokeError::Empty: return ActionInvocationStatus::Empty;
		case ActionInvokeError::Disabled: return ActionInvocationStatus::Disabled;
		case ActionInvokeError::Unbound:
		case ActionInvokeError::ResultTypeMismatch:
		default: return ActionInvocationStatus::Unbound;
		}
	}
	try {
		lease.header().invokeDiscard(lease.payload());
		lease.noteSuccess(true);
		return ActionInvocationStatus::Invoked;
	} catch (...) {
		lease.noteException();
		throw;
	}
}

ActionInvocationStatus ActionManager::invoke(
	ActionCall call,
	ActionInvocationSource source) {
	switch (call.kind_) {
	case ActionCallKind::None:
		return ActionInvocationStatus::Empty;
	case ActionCallKind::App:
		return invokeApp(call.payload_.app, source);
	case ActionCallKind::Ui:
		if (!call.payload_.ui.invokeThunk_) return ActionInvocationStatus::Empty;
		call.payload_.ui.invokeThunk_(call.payload_.ui.payload_.data());
		return ActionInvocationStatus::Invoked;
	default:
		return ActionInvocationStatus::Empty;
	}
}

ActionAvailability ActionManager::availability(ActionCall call) const noexcept {
	switch (call.kind_) {
	case ActionCallKind::Ui:
		return ActionAvailability{.enabled = static_cast<bool>(call.payload_.ui)};
	case ActionCallKind::App: {
		const AppActionAvailability appAvailability = appActions_.availability(call.payload_.app);
		return ActionAvailability{.enabled = appAvailability.enabled};
	}
	case ActionCallKind::None:
	default:
		return {};
	}
}

#if FLOW_UI_DEV_MODE
std::optional<ActionDebugInfo> ActionManager::debugInfo(ActionCall call) const noexcept {
	if (!call) return std::nullopt;
	ActionDebugInfo info{};
	info.kind = call.kind_;
	info.availability = availability(call);
	if (call.kind_ == ActionCallKind::Ui) {
		info.uiRecipeId = call.payload_.ui.recipeId_;
		info.debugName = call.payload_.ui.recipeName_;
		info.definitionSource = call.payload_.ui.definitionSource_;
		info.bound = static_cast<bool>(call.payload_.ui);
		return info;
	}
	if (call.kind_ != ActionCallKind::App) return info;
	info.appId = call.payload_.app.id;
	info.debugName = call.payload_.app.id.debugName;
	if (!storage_) return info;
	try {
		const auto found = state().bindings.find(call.payload_.app.id);
		if (found == state().bindings.end()) return info;
		const storage::ConstPersistentRecordView view =
			std::as_const(*storage_).persistentRecord(
				found->second, storage::ResourceKind::AppActionBinding);
		if (!view || view.headerBytes < sizeof(action::AppActionBindingHeader)) return info;
		const auto& header = *static_cast<const action::AppActionBindingHeader*>(view.header);
		info.bound = !header.tombstoned;
		info.debugName = storage_->string(header.debugName);
		info.invocationCount = header.invocationCount;
		info.discardedResultCount = header.discardedResultCount;
		info.lastStatus = header.lastStatus;
		info.lastSource = header.lastSource;
		info.lastInvocationThrew = header.lastInvocationThrew;
	} catch (...) {
	}
	return info;
}
#endif

AppActionCall AppActions::select(AppActionID id) const noexcept {
	return AppActionCall{id};
}

bool AppActions::isBound(AppActionID id) const noexcept {
	if (!owner_ || !owner_->storage_ || !id) return false;
	try {
		return owner_->state().bindings.contains(id);
	} catch (...) {
		return false;
	}
}

bool AppActions::isBound(AppActionCall call) const noexcept {
	return call && isBound(call.id);
}

bool AppActions::unbind(AppActionID id) {
	if (!owner_ || !owner_->storage_ || !id) return false;
	auto& current = owner_->state();
	const auto found = current.bindings.find(id);
	if (found == current.bindings.end()) return false;
	const storage::PersistentRecordHandle handle = found->second;
	current.bindings.erase(found);
	owner_->retireBinding(handle);
	owner_->storage_->noteManagerMutation(InvalidWindowId);
	return true;
}

ActionInvocationStatus AppActions::invoke(AppActionCall call) {
	return owner_ ? owner_->invokeApp(call, {}) : ActionInvocationStatus::Unbound;
}

AppActionAvailability AppActions::availability(AppActionCall call) const noexcept {
	if (!owner_ || !owner_->storage_ || !call) return {.enabled = false};
	try {
		const auto found = owner_->state().bindings.find(call.id);
		if (found == owner_->state().bindings.end()) return {.enabled = false};
		const storage::ConstPersistentRecordView view =
			std::as_const(*owner_->storage_).persistentRecord(
				found->second, storage::ResourceKind::AppActionBinding);
		if (!view || view.headerBytes < sizeof(action::AppActionBindingHeader)) {
			return {.enabled = false};
		}
		const auto& header = *static_cast<const action::AppActionBindingHeader*>(view.header);
		return header.tombstoned ? AppActionAvailability{.enabled = false} : header.availability;
	} catch (...) {
		return {.enabled = false};
	}
}

bool AppActions::setAvailability(
	AppActionID id,
	AppActionAvailability availabilityValue) {
	if (!owner_ || !owner_->storage_ || !id) return false;
	auto& current = owner_->state();
	const auto found = current.bindings.find(id);
	if (found == current.bindings.end()) return false;
	const storage::PersistentRecordView view = owner_->storage_->persistentRecord(
		found->second, storage::ResourceKind::AppActionBinding);
	if (!view || view.headerBytes < sizeof(action::AppActionBindingHeader)) return false;
	auto& header = *static_cast<action::AppActionBindingHeader*>(view.header);
	if (header.tombstoned) return false;
	if (header.availability.enabled == availabilityValue.enabled) return true;
	header.availability = availabilityValue;
	owner_->storage_->noteManagerMutation(InvalidWindowId);
	return true;
}

} // namespace FlowUi
