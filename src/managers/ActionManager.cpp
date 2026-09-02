#include "managers/ActionManager.hpp"
	#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#endif

#include <algorithm>
#include <stdexcept>

#include "FlowUi/App.hpp"
#include "internal/ManagerStorage/ActionManagerState.hpp"
#include "internal/ManagerStorage/ManagerStateAccess.hpp"
#include "internal/StorageSystem/IStorageSystem.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
void ActionManager::appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept {
	if (!storage_ || stateHandle_ == 0u) return;
	try {
		const auto& current = state();
		devSystems::DevContainerMemoryAccumulator memory{};
		memory.addNodeContainer(current.bindings);
		memory.add(current.deferredRemovals);
		memory.add(devUiRecipes_);
		devSystems::appendManagerSample(sink, devSystems::memory_sources::kActions.id, memory);
	} catch (...) {}
}
#endif

namespace action = detail::action;
namespace manager_storage = detail::manager_storage;
namespace storage = detail::storage;

#if FLOW_UI_DEV_MODE
std::uint64_t ActionManager::devRevision() const noexcept {
	return devRevision_;
}

std::size_t ActionManager::devActionCount() const noexcept {
	try { return (storage_ && stateHandle_ ? state().bindings.size() : 0) + devUiRecipes_.size(); }
	catch (...) { return 0; }
}

bool ActionManager::visitDevActions(void* userData, DevActionVisitor visitor) const {
	if (!storage_ || !stateHandle_ || !visitor) return false;
	for (const auto& [id, handle] : state().bindings) {
		const storage::ConstPersistentRecordView record =
			std::as_const(*storage_).persistentRecord(
				handle, storage::ResourceKind::AppActionBinding);
		if (!record || record.headerBytes < sizeof(action::AppActionBindingHeader)) continue;
		const auto& header = *static_cast<const action::AppActionBindingHeader*>(record.header);
		ActionDebugInfo debug{};
		debug.kind = ActionCallKind::App;
		debug.appId = id;
		debug.debugName = storage_->string(header.debugName);
		debug.availability.enabled = header.availability.enabled;
		debug.bound = !header.tombstoned;
		debug.invocationCount = header.invocationCount;
		debug.discardedResultCount = header.discardedResultCount;
		debug.lastStatus = header.lastStatus;
		debug.lastSource = header.lastSource;
		debug.lastInvocationThrew = header.lastInvocationThrew;
		if (!visitor(userData, DevActionView{
			.debug = debug,
			.callableTypeHash = header.callableTypeHash,
			.resultTypeHash = header.resultTypeHash,
			.reconstructable = !header.tombstoned,
		})) return false;
	}
	for (const DevUiRecipeRecord& recipe : devUiRecipes_) {
		ActionDebugInfo debug{};
		debug.kind = ActionCallKind::Ui;
		debug.uiRecipeId = recipe.id;
		debug.debugName = recipe.debugName;
		debug.availability.enabled = true;
		debug.bound = recipe.reconstructable;
		debug.definitionSource = recipe.source;
		if (!visitor(userData, DevActionView{
			.debug = debug,
			.callableTypeHash = recipe.id,
			.reconstructable = recipe.reconstructable,
		})) return false;
	}
	return true;
}

std::optional<ActionCall> ActionManager::makeDevActionCall(
	ActionCallKind kind,
	std::uint64_t stableId,
	std::uint64_t expectedCallableTypeHash,
	std::uint64_t expectedResultTypeHash) const noexcept {
	if (kind == ActionCallKind::None && stableId == 0u) return ActionCall{};
	if (kind == ActionCallKind::Ui && stableId != 0u) {
		const auto found = std::ranges::find_if(devUiRecipes_,
			[stableId](const DevUiRecipeRecord& recipe) {
				return recipe.id == stableId && recipe.reconstructable;
			});
		return found == devUiRecipes_.end()
			? std::nullopt : std::optional<ActionCall>{ActionCall{found->prototype}};
	}
	if (kind != ActionCallKind::App || stableId == 0u || !storage_ || !stateHandle_) {
		return std::nullopt;
	}
	try {
		const auto found = std::ranges::find_if(state().bindings,
			[stableId](const auto& binding) { return binding.first.value == stableId; });
		if (found == state().bindings.end()) return std::nullopt;
		const storage::ConstPersistentRecordView record = std::as_const(*storage_).persistentRecord(
			found->second, storage::ResourceKind::AppActionBinding);
		if (!record || record.headerBytes < sizeof(action::AppActionBindingHeader)) return std::nullopt;
		const auto& header = *static_cast<const action::AppActionBindingHeader*>(record.header);
		if (header.tombstoned ||
			(expectedCallableTypeHash != 0u && header.callableTypeHash != expectedCallableTypeHash) ||
			(expectedResultTypeHash != 0u && header.resultTypeHash != expectedResultTypeHash)) {
			return std::nullopt;
		}
		return ActionCall{appActions_.select(found->first)};
	} catch (...) {
		return std::nullopt;
	}
}

#endif

#if FLOW_UI_DEV_MODE || FLOW_UI_HAS_BAKED_CHANGES
void ActionManager::noteUiRecipe(
	std::uint64_t recipeId,
	const UiActionCall* reconstructable
#if FLOW_UI_DEV_MODE
	, std::string_view debugName,
	ActionSourceLocation source
#endif
	) noexcept {
	if (recipeId == 0) return;
	for (DevUiRecipeRecord& recipe : devUiRecipes_) {
		if (recipe.id != recipeId) continue;
		if (reconstructable && !recipe.reconstructable) {
			recipe.prototype = *reconstructable;
			recipe.reconstructable = true;
		}
		return;
	}
	try {
		devUiRecipes_.push_back(DevUiRecipeRecord{
			.id = recipeId,
			.prototype = reconstructable ? *reconstructable : UiActionCall{},
			.reconstructable = reconstructable != nullptr,
#if FLOW_UI_DEV_MODE
			.debugName = debugName,
			.source = source,
#endif
		});
#if FLOW_UI_DEV_MODE
		++devRevision_;
#endif
	} catch (...) {}
}
#endif

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
		throw FlowUiException(makeError(ErrorCode::ResourcePublicationFailed, ErrorSite::ActionManagerInitialize));
	}
	app_ = &app;
	storage_ = &storageSystem;
	stateHandle_ = handle.packed();
	stateName_ = name;
}

void ActionManager::destroy() noexcept {
#if FLOW_UI_DEV_MODE || FLOW_UI_HAS_BAKED_CHANGES
	devUiRecipes_.clear();
#endif
#if FLOW_UI_DEV_MODE
	++devRevision_;
#endif
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
	if (!result) detail::terminateForFatalError(makeError(ErrorCode::InternalInvariantBroken, ErrorSite::ActionInvoke));
	return *result;
}

const manager_storage::ActionManagerState& ActionManager::state() const {
	const auto* result = manager_storage::state<manager_storage::ActionManagerState>(
		storage_,
		storage::ManagerRecordHandle::fromPacked(stateHandle_),
		storage::ResourceKind::ActionManager);
	if (!result) detail::terminateForFatalError(makeError(ErrorCode::InternalInvariantBroken, ErrorSite::ActionInvoke));
	return *result;
}

Status ActionManager::validateBindingRequest(AppActionID id, bool replacement) const {
	if (!id) return unexpectedError(makeError(ErrorCode::InvalidActionId, ErrorSite::ActionBind));
	const bool exists = state().bindings.contains(id);
	if (!replacement && exists) {
		return unexpectedError(makeError(ErrorCode::ActionAlreadyBound, ErrorSite::ActionBind, id.value));
	}
	if (replacement && !exists) {
		return unexpectedError(makeError(ErrorCode::ActionNotBound, ErrorSite::ActionRebind, id.value));
	}
	return {};
}

void ActionManager::publishBinding(
	AppActionID id,
	storage::PersistentRecordHandle handle,
	bool replacement) {
	auto& current = state();
	if (!replacement) {
		const auto [_, inserted] = current.bindings.emplace(id, handle);
		if (!inserted) {
			detail::terminateForFatalError(makeError(
				ErrorCode::ActionPublicationConflict, ErrorSite::ActionPublish, id.value));
		}
	} else {
		const auto found = current.bindings.find(id);
		if (found == current.bindings.end()) {
			detail::terminateForFatalError(makeError(
				ErrorCode::ActionPublicationConflict, ErrorSite::ActionPublish, id.value));
		}
		const storage::PersistentRecordHandle previous = found->second;
		found->second = handle;
		retireBinding(previous);
	}
	storage_->noteManagerMutation(InvalidWindowId);
#if FLOW_UI_DEV_MODE
	++devRevision_;
#endif
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
	if (owner_) ++owner_->devRevision_;
#else
	(void)discardedResult;
#endif
}

void ActionManager::InvocationLease::noteException() noexcept {
#if FLOW_UI_DEV_MODE
	if (header_) {
		header_->lastInvocationThrew = true;
		if (owner_) ++owner_->devRevision_;
	}
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
	++devRevision_;
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
		ErrorCode code = ErrorCode::ActionNotBound;
		switch (error) {
		case ActionInvokeError::Unbound: code = ErrorCode::ActionNotBound; break;
		case ActionInvokeError::Disabled: code = ErrorCode::ActionDisabled; break;
		case ActionInvokeError::ResultTypeMismatch:
			code = ErrorCode::ActionResultTypeMismatch;
			break;
		case ActionInvokeError::Empty: return;
		}
		static constexpr auto source =
			devSystems::makeDevErrorSource("flowui.action.invocation_diagnostic");
		static constexpr auto breadcrumb =
			devSystems::makeDevErrorBreadcrumb("flowui.action.callback.failed");
		devSystems::recordGlobalDevBreadcrumb(
			breadcrumb, call.id.value, static_cast<uint64_t>(error));
		devSystems::recordGlobalDevDiagnostic(
			makeError(code, ErrorSite::ActionInvoke, call.id.value), source, name);
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
	case ActionCallKind::UiRecipe: {
#if FLOW_UI_DEV_MODE || FLOW_UI_HAS_BAKED_CHANGES
		const auto found = std::ranges::find_if(devUiRecipes_,
			[&call](const DevUiRecipeRecord& recipe) {
				return recipe.id == call.payload_.stableId && recipe.reconstructable;
			});
		if (found == devUiRecipes_.end()) return ActionInvocationStatus::Unbound;
		found->prototype.invokeThunk_(found->prototype.payload_.data());
		return ActionInvocationStatus::Invoked;
#else
		return ActionInvocationStatus::Unbound;
#endif
	}
	default:
		return ActionInvocationStatus::Empty;
	}
}

ActionAvailability ActionManager::availability(ActionCall call) const noexcept {
	switch (call.kind_) {
	case ActionCallKind::Ui:
		return ActionAvailability{.enabled = static_cast<bool>(call.payload_.ui)};
	case ActionCallKind::UiRecipe:
#if FLOW_UI_DEV_MODE || FLOW_UI_HAS_BAKED_CHANGES
		return ActionAvailability{.enabled = std::ranges::any_of(devUiRecipes_,
			[&call](const DevUiRecipeRecord& recipe) {
				return recipe.id == call.payload_.stableId && recipe.reconstructable;
			})};
#else
		return {};
#endif
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
	if (call.kind_ == ActionCallKind::UiRecipe) {
		info.kind = ActionCallKind::Ui;
		info.uiRecipeId = call.payload_.stableId;
		const auto found = std::ranges::find_if(devUiRecipes_,
			[&call](const DevUiRecipeRecord& recipe) {
				return recipe.id == call.payload_.stableId;
			});
		if (found != devUiRecipes_.end()) {
			info.debugName = found->debugName;
			info.definitionSource = found->source;
			info.bound = found->reconstructable;
		}
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
#if FLOW_UI_DEV_MODE
	++owner_->devRevision_;
#endif
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
#if FLOW_UI_DEV_MODE
	++owner_->devRevision_;
#endif
	return true;
}

} // namespace FlowUi
