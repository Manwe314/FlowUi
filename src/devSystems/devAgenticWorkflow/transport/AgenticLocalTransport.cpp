#include "devSystems/devAgenticWorkflow/AgenticLocalTransport.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <sddl.h>
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace FlowUi::devSystems::agentic {
namespace {
constexpr std::string_view local_protocol = "FlowUi.Agentic.Local/1";
constexpr std::size_t local_worker_count = 8;
constexpr std::string_view handler_failure_wire =
	"{\"schema\":\"FlowUi.Agentic.CommandResult\",\"schema_version\":\"1.0\","
	"\"operation_status\":\"failed\",\"exit_code\":4,\"result\":null,"
	"\"error\":{\"code\":\"handler_failed\"}}\n";

[[nodiscard]] auto transport_error(ErrorCode code, std::uint32_t native_code = 0) noexcept {
	return unexpectedError(makeError(code, ErrorSite::AgenticTransport, 0, 0, native_code));
}

[[nodiscard]] Json protocol_failure(std::string_view reason) {
	return Json::make_object(
		{{"schema", "FlowUi.Agentic.CommandResult"},
		 {"schema_version", "1.0"},
		 {"operation_status", "failed"},
		 {"exit_code", Json(3.0)},
		 {"error", Json::make_object({{"code", "unavailable"}, {"message", Json(reason)}})}});
}

[[nodiscard]] bool valid_endpoint(const AgenticLocalEndpoint& endpoint) noexcept {
	return !endpoint.endpoint.empty() && !endpoint.run_id.empty() &&
		   endpoint.run_id.size() <= 128 && endpoint.nonce.size() >= 16 &&
		   endpoint.nonce.size() <= 256 && endpoint.maximum_message_bytes >= 256 &&
		   endpoint.maximum_message_bytes <= 16u * 1024u * 1024u && endpoint.timeout_ms > 0 &&
		   endpoint.timeout_ms <= 60000;
}

[[nodiscard]] Json handle_message(std::string_view message, const AgenticLocalEndpoint& endpoint,
								  const AgenticLocalHandler& handler) {
	auto decoded = parse_json(message, endpoint.maximum_message_bytes);
	if (!decoded)
		return protocol_failure("invalid_json");
	const auto* protocol = decoded->find("protocol");
	const auto* run_id = decoded->find("run_id");
	const auto* nonce = decoded->find("nonce");
	const auto* request = decoded->find("request");
	if (!protocol || protocol->string_or() != local_protocol || !run_id ||
		run_id->string_or() != endpoint.run_id || !nonce || nonce->string_or() != endpoint.nonce ||
		!request) {
		return protocol_failure("stale_or_unauthorized_run");
	}
	return handler(*request);
}

#ifndef _WIN32
void close_socket(int socket_descriptor) noexcept {
	if (socket_descriptor >= 0)
		::close(socket_descriptor);
}

[[nodiscard]] bool await_socket(int socket_descriptor, short events,
								std::chrono::steady_clock::time_point deadline) noexcept {
	while (std::chrono::steady_clock::now() < deadline) {
		const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
								   deadline - std::chrono::steady_clock::now())
								   .count();
		pollfd descriptor{socket_descriptor, events, 0};
		const auto outcome =
			::poll(&descriptor, 1, static_cast<int>(std::max<std::int64_t>(1, remaining)));
		if (outcome > 0)
			return (descriptor.revents & events) != 0;
		if (outcome < 0 && errno == EINTR)
			continue;
		return false;
	}
	return false;
}

[[nodiscard]] bool write_message(int socket_descriptor, std::string_view bytes,
								 std::uint64_t timeout_ms) noexcept {
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
	std::size_t written = 0;
	while (written < bytes.size()) {
		if (!await_socket(socket_descriptor, POLLOUT, deadline))
			return false;
#ifdef MSG_NOSIGNAL
		const auto count =
			::send(socket_descriptor, bytes.data() + written, bytes.size() - written, MSG_NOSIGNAL);
#else
		const auto count =
			::send(socket_descriptor, bytes.data() + written, bytes.size() - written, 0);
#endif
		if (count <= 0) {
			if (count < 0 && errno == EINTR)
				continue;
			return false;
		}
		written += static_cast<std::size_t>(count);
	}
	return true;
}

[[nodiscard]] Result<std::string> read_message(int socket_descriptor,
											   const AgenticLocalEndpoint& endpoint) {
	const auto deadline =
		std::chrono::steady_clock::now() + std::chrono::milliseconds(endpoint.timeout_ms);
	std::string bytes;
	bytes.reserve(std::min<std::size_t>(endpoint.maximum_message_bytes, 8192));
	std::array<char, 4096> buffer{};
	while (bytes.size() < endpoint.maximum_message_bytes) {
		if (!await_socket(socket_descriptor, POLLIN, deadline))
			return transport_error(ErrorCode::AgenticTimeout);
		const auto count =
			::recv(socket_descriptor, buffer.data(),
				   std::min(buffer.size(), endpoint.maximum_message_bytes - bytes.size()), 0);
		if (count <= 0) {
			if (count < 0 && errno == EINTR)
				continue;
			return transport_error(ErrorCode::AgenticUnavailable);
		}
		bytes.append(buffer.data(), static_cast<std::size_t>(count));
		const auto newline = bytes.find('\n');
		if (newline != std::string::npos) {
			if (newline + 1 != bytes.size())
				return transport_error(ErrorCode::AgenticInvalidRequest);
			bytes.resize(newline);
			return bytes;
		}
	}
	return transport_error(ErrorCode::AgenticBudgetExhausted);
}

[[nodiscard]] bool owned_peer(int socket_descriptor) noexcept {
#if defined(__linux__)
	ucred credentials{};
	socklen_t length = sizeof(credentials);
	return ::getsockopt(socket_descriptor, SOL_SOCKET, SO_PEERCRED, &credentials, &length) == 0 &&
		   credentials.uid == ::geteuid();
#elif defined(__APPLE__) || defined(__FreeBSD__)
	uid_t user_id{};
	gid_t group_id{};
	return ::getpeereid(socket_descriptor, &user_id, &group_id) == 0 && user_id == ::geteuid();
#else
	struct stat descriptor_status{};
	return ::fstat(socket_descriptor, &descriptor_status) == 0;
#endif
}
#else
[[nodiscard]] Result<DWORD> pipe_transfer(HANDLE pipe, void* buffer, DWORD bytes, bool writing,
										  std::uint64_t timeout_ms) noexcept {
	OVERLAPPED operation{};
	operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (operation.hEvent == nullptr)
		return transport_error(ErrorCode::AgenticUnavailable, GetLastError());
	DWORD transferred = 0;
	const auto completed = writing ? WriteFile(pipe, buffer, bytes, &transferred, &operation)
								   : ReadFile(pipe, buffer, bytes, &transferred, &operation);
	if (!completed) {
		const auto native_error = GetLastError();
		if (native_error != ERROR_IO_PENDING) {
			CloseHandle(operation.hEvent);
			return transport_error(ErrorCode::AgenticUnavailable, native_error);
		}
		if (WaitForSingleObject(operation.hEvent, static_cast<DWORD>(timeout_ms)) !=
			WAIT_OBJECT_0) {
			CancelIoEx(pipe, &operation);
			GetOverlappedResult(pipe, &operation, &transferred, TRUE);
			CloseHandle(operation.hEvent);
			return transport_error(ErrorCode::AgenticTimeout);
		}
		if (!GetOverlappedResult(pipe, &operation, &transferred, FALSE)) {
			const auto operation_error = GetLastError();
			CloseHandle(operation.hEvent);
			return transport_error(ErrorCode::AgenticUnavailable, operation_error);
		}
	}
	CloseHandle(operation.hEvent);
	return transferred;
}

[[nodiscard]] bool pipe_connect(HANDLE pipe, const std::atomic<bool>& active) noexcept {
	OVERLAPPED operation{};
	operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (operation.hEvent == nullptr)
		return false;
	if (ConnectNamedPipe(pipe, &operation)) {
		CloseHandle(operation.hEvent);
		return true;
	}
	const auto native_error = GetLastError();
	if (native_error == ERROR_PIPE_CONNECTED) {
		CloseHandle(operation.hEvent);
		return true;
	}
	if (native_error != ERROR_IO_PENDING) {
		CloseHandle(operation.hEvent);
		return false;
	}
	while (active.load() && WaitForSingleObject(operation.hEvent, 100) == WAIT_TIMEOUT) {}
	DWORD transferred = 0;
	if (!active.load())
		CancelIoEx(pipe, &operation);
	const auto completed = GetOverlappedResult(pipe, &operation, &transferred, TRUE);
	CloseHandle(operation.hEvent);
	return completed && active.load();
}

[[nodiscard]] bool pipe_write(HANDLE pipe, std::string_view bytes,
							  std::uint64_t timeout_ms) noexcept {
	std::size_t written = 0;
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
	while (written < bytes.size()) {
		const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
								   deadline - std::chrono::steady_clock::now())
								   .count();
		if (remaining <= 0)
			return false;
		auto amount = pipe_transfer(pipe, const_cast<char*>(bytes.data() + written),
									static_cast<DWORD>(bytes.size() - written), true,
									static_cast<std::uint64_t>(remaining));
		if (!amount || *amount == 0)
			return false;
		written += *amount;
	}
	return true;
}

[[nodiscard]] Result<std::string> pipe_read(HANDLE pipe, const AgenticLocalEndpoint& endpoint,
											const std::atomic<bool>* active = nullptr) {
	std::string bytes;
	bytes.reserve(std::min<std::size_t>(endpoint.maximum_message_bytes, 8192));
	std::array<char, 4096> buffer{};
	const auto deadline =
		std::chrono::steady_clock::now() + std::chrono::milliseconds(endpoint.timeout_ms);
	while (bytes.size() < endpoint.maximum_message_bytes &&
		   std::chrono::steady_clock::now() < deadline && (!active || active->load())) {
		DWORD available = 0;
		if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr))
			return transport_error(ErrorCode::AgenticUnavailable, GetLastError());
		if (available == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
			continue;
		}
		const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
								   deadline - std::chrono::steady_clock::now())
								   .count();
		if (remaining <= 0)
			return transport_error(ErrorCode::AgenticTimeout);
		auto amount = pipe_transfer(
			pipe, buffer.data(),
			static_cast<DWORD>(std::min<std::size_t>(
				{buffer.size(), available, endpoint.maximum_message_bytes - bytes.size()})),
			false, static_cast<std::uint64_t>(remaining));
		if (!amount)
			return unexpectedError(amount.error());
		if (*amount == 0)
			return transport_error(ErrorCode::AgenticUnavailable);
		bytes.append(buffer.data(), *amount);
		const auto newline = bytes.find('\n');
		if (newline != std::string::npos) {
			if (newline + 1 != bytes.size())
				return transport_error(ErrorCode::AgenticInvalidRequest);
			bytes.resize(newline);
			return bytes;
		}
	}
	return transport_error(bytes.size() >= endpoint.maximum_message_bytes
							   ? ErrorCode::AgenticBudgetExhausted
							   : ErrorCode::AgenticTimeout);
}
#endif
} // namespace

struct AgenticLocalServer::Impl {
	struct Worker {
		std::thread thread;
#ifdef _WIN32
		std::atomic<HANDLE> connection{INVALID_HANDLE_VALUE};
#else
		std::atomic<int> connection{-1};
		std::mutex connection_mutex;
#endif
	};
	AgenticLocalEndpoint endpoint{};
	AgenticLocalHandler handler{};
	std::array<Worker, local_worker_count> workers;
	std::atomic<bool> active{false};
#ifndef _WIN32
	int listener = -1;
#endif

	void run(Worker& worker) noexcept {
#ifdef _WIN32
		while (active.load()) {
			const auto pipe = worker.connection.load();
			if (pipe == INVALID_HANDLE_VALUE)
				break;
			if (!pipe_connect(pipe, active)) {
				if (active.load())
					std::this_thread::sleep_for(std::chrono::milliseconds(2));
				continue;
			}
			try {
				auto message = pipe_read(pipe, endpoint, &active);
				if (message && active.load()) {
					auto response = write_json(handle_message(*message, endpoint, handler));
					if (response.size() + 1 > endpoint.maximum_message_bytes)
						response = write_json(protocol_failure("response_budget_exhausted"));
					response.push_back('\n');
					const auto sent =
						active.load() && pipe_write(pipe, response, endpoint.timeout_ms);
					if (!sent)
						DisconnectNamedPipe(pipe);
				}
			} catch (...) {
				const auto sent =
					active.load() && pipe_write(pipe, handler_failure_wire, endpoint.timeout_ms);
				if (!sent)
					DisconnectNamedPipe(pipe);
			}
			DisconnectNamedPipe(pipe);
		}
#else
		while (active.load()) {
			pollfd descriptor{listener, POLLIN, 0};
			if (::poll(&descriptor, 1, 100) <= 0)
				continue;
			const auto connection = ::accept(listener, nullptr, nullptr);
			if (connection < 0)
				continue;
			{
				std::lock_guard lock(worker.connection_mutex);
				if (!active.load()) {
					close_socket(connection);
					break;
				}
				worker.connection.store(connection);
			}
			if (!owned_peer(connection)) {
				std::lock_guard lock(worker.connection_mutex);
				close_socket(worker.connection.exchange(-1));
				continue;
			}
			const auto descriptor_flags = ::fcntl(connection, F_GETFD);
			if (descriptor_flags >= 0)
				::fcntl(connection, F_SETFD, descriptor_flags | FD_CLOEXEC);
			const auto socket_flags = ::fcntl(connection, F_GETFL);
			if (socket_flags >= 0)
				::fcntl(connection, F_SETFL, socket_flags | O_NONBLOCK);
			try {
				auto message = read_message(connection, endpoint);
				if (message && active.load()) {
					auto response = write_json(handle_message(*message, endpoint, handler));
					if (response.size() + 1 > endpoint.maximum_message_bytes)
						response = write_json(protocol_failure("response_budget_exhausted"));
					response.push_back('\n');
					const auto sent =
						active.load() && write_message(connection, response, endpoint.timeout_ms);
					if (!sent)
						::shutdown(connection, SHUT_RDWR);
				}
			} catch (...) {
				const auto sent = active.load() && write_message(connection, handler_failure_wire,
																 endpoint.timeout_ms);
				if (!sent)
					::shutdown(connection, SHUT_RDWR);
			}
			{
				std::lock_guard lock(worker.connection_mutex);
				close_socket(worker.connection.exchange(-1));
			}
		}
#endif
	}
};

AgenticLocalServer::AgenticLocalServer() : impl_(std::make_unique<Impl>()) {}
AgenticLocalServer::~AgenticLocalServer() {
	stop();
}

Status AgenticLocalServer::start(const AgenticLocalEndpoint& endpoint,
								 AgenticLocalHandler handler) noexcept {
	try {
		if (!valid_endpoint(endpoint) || !handler)
			return transport_error(ErrorCode::AgenticInvalidRequest);
		if (impl_->active.load())
			return transport_error(ErrorCode::AgenticBusy);
		impl_->endpoint = endpoint;
		impl_->handler = std::move(handler);
#ifdef _WIN32
		if (!endpoint.endpoint.starts_with("\\\\.\\pipe\\"))
			return transport_error(ErrorCode::AgenticInvalidScope);
		PSECURITY_DESCRIPTOR security_descriptor = nullptr;
		if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(
				"D:P(A;;GA;;;OW)", SDDL_REVISION_1, &security_descriptor, nullptr))
			return transport_error(ErrorCode::AgenticUnavailable, GetLastError());
		SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), security_descriptor, FALSE};
		DWORD creation_error = ERROR_SUCCESS;
		for (std::size_t worker_index = 0; worker_index < impl_->workers.size(); ++worker_index) {
			const auto pipe = CreateNamedPipeA(
				endpoint.endpoint.c_str(),
				PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
					(worker_index == 0 ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0),
				PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
				static_cast<DWORD>(local_worker_count),
				static_cast<DWORD>(endpoint.maximum_message_bytes),
				static_cast<DWORD>(endpoint.maximum_message_bytes),
				static_cast<DWORD>(endpoint.timeout_ms), &security);
			if (pipe == INVALID_HANDLE_VALUE) {
				creation_error = GetLastError();
				break;
			}
			impl_->workers[worker_index].connection.store(pipe);
		}
		LocalFree(security_descriptor);
		if (creation_error != ERROR_SUCCESS) {
			stop();
			return transport_error(ErrorCode::AgenticUnavailable, creation_error);
		}
#else
		if (endpoint.endpoint.size() >= sizeof(sockaddr_un{}.sun_path))
			return transport_error(ErrorCode::AgenticInvalidScope);
		const auto listener = ::socket(AF_UNIX, SOCK_STREAM, 0);
		if (listener < 0)
			return transport_error(ErrorCode::AgenticUnavailable, errno);
		::fcntl(listener, F_SETFD, FD_CLOEXEC);
		const auto listener_flags = ::fcntl(listener, F_GETFL);
		if (listener_flags < 0 || ::fcntl(listener, F_SETFL, listener_flags | O_NONBLOCK) != 0) {
			const auto native_code = errno;
			close_socket(listener);
			return transport_error(ErrorCode::AgenticUnavailable, native_code);
		}
		sockaddr_un address{};
		address.sun_family = AF_UNIX;
		std::memcpy(address.sun_path, endpoint.endpoint.c_str(), endpoint.endpoint.size() + 1);
		if (::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
			const auto native_code = errno;
			close_socket(listener);
			return transport_error(ErrorCode::AgenticUnavailable, native_code);
		}
		if (::chmod(endpoint.endpoint.c_str(), S_IRUSR | S_IWUSR) != 0 ||
			::listen(listener, 8) != 0) {
			const auto native_code = errno;
			close_socket(listener);
			::unlink(endpoint.endpoint.c_str());
			return transport_error(ErrorCode::AgenticUnavailable, native_code);
		}
		impl_->listener = listener;
#endif
		impl_->active.store(true);
		try {
			for (auto& worker : impl_->workers)
				worker.thread = std::thread(
					[implementation = impl_.get(), slot = &worker] { implementation->run(*slot); });
		} catch (...) {
			stop();
			return transport_error(ErrorCode::AgenticUnavailable);
		}
		return {};
	} catch (...) {
		return transport_error(ErrorCode::AgenticBudgetExhausted);
	}
}

void AgenticLocalServer::stop() noexcept {
	if (!impl_)
		return;
	impl_->active.store(false);
#ifdef _WIN32
	for (auto& worker : impl_->workers) {
		const auto pipe = worker.connection.load();
		if (pipe != INVALID_HANDLE_VALUE)
			CancelIoEx(pipe, nullptr);
	}
#else
	if (impl_->listener >= 0)
		::shutdown(impl_->listener, SHUT_RDWR);
	for (auto& worker : impl_->workers) {
		std::lock_guard lock(worker.connection_mutex);
		const auto connection = worker.connection.load();
		if (connection >= 0)
			::shutdown(connection, SHUT_RDWR);
	}
#endif
	for (auto& worker : impl_->workers)
		if (worker.thread.joinable())
			worker.thread.join();
#ifdef _WIN32
	for (auto& worker : impl_->workers) {
		const auto stopped_pipe = worker.connection.exchange(INVALID_HANDLE_VALUE);
		if (stopped_pipe != INVALID_HANDLE_VALUE) {
			DisconnectNamedPipe(stopped_pipe);
			CloseHandle(stopped_pipe);
		}
	}
#else
	if (impl_->listener >= 0) {
		close_socket(impl_->listener);
		impl_->listener = -1;
		::unlink(impl_->endpoint.endpoint.c_str());
	}
#endif
	impl_->handler = {};
}

bool AgenticLocalServer::running() const noexcept {
	return impl_ && impl_->active.load();
}

Result<Json> send_local_command(const AgenticLocalEndpoint& endpoint,
								const Json& request) noexcept {
	try {
		if (!valid_endpoint(endpoint))
			return transport_error(ErrorCode::AgenticInvalidRequest);
		auto bytes = write_json(Json::make_object({{"protocol", Json(local_protocol)},
												   {"run_id", endpoint.run_id},
												   {"nonce", endpoint.nonce},
												   {"request", request}}));
		bytes.push_back('\n');
		if (bytes.size() > endpoint.maximum_message_bytes)
			return transport_error(ErrorCode::AgenticBudgetExhausted);
#ifdef _WIN32
		if (!WaitNamedPipeA(endpoint.endpoint.c_str(), static_cast<DWORD>(endpoint.timeout_ms)))
			return transport_error(ErrorCode::AgenticUnavailable, GetLastError());
		const auto pipe = CreateFileA(endpoint.endpoint.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
									  nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
		if (pipe == INVALID_HANDLE_VALUE)
			return transport_error(ErrorCode::AgenticUnavailable, GetLastError());
		if (!pipe_write(pipe, bytes, endpoint.timeout_ms)) {
			CloseHandle(pipe);
			return transport_error(ErrorCode::AgenticUnavailable);
		}
		auto response = pipe_read(pipe, endpoint);
		CloseHandle(pipe);
#else
		if (endpoint.endpoint.size() >= sizeof(sockaddr_un{}.sun_path))
			return transport_error(ErrorCode::AgenticInvalidScope);
		struct stat socket_status{};
		if (::lstat(endpoint.endpoint.c_str(), &socket_status) != 0 ||
			!S_ISSOCK(socket_status.st_mode) || socket_status.st_uid != ::geteuid() ||
			(socket_status.st_mode & (S_IRWXG | S_IRWXO)) != 0)
			return transport_error(ErrorCode::AgenticUnavailable);
		const auto connection = ::socket(AF_UNIX, SOCK_STREAM, 0);
		if (connection < 0)
			return transport_error(ErrorCode::AgenticUnavailable, errno);
		::fcntl(connection, F_SETFD, FD_CLOEXEC);
		const auto socket_flags = ::fcntl(connection, F_GETFL);
		::fcntl(connection, F_SETFL, socket_flags | O_NONBLOCK);
		sockaddr_un address{};
		address.sun_family = AF_UNIX;
		std::memcpy(address.sun_path, endpoint.endpoint.c_str(), endpoint.endpoint.size() + 1);
		const auto connected =
			::connect(connection, reinterpret_cast<sockaddr*>(&address), sizeof(address));
		if (connected != 0 && errno != EINPROGRESS) {
			const auto native_code = errno;
			close_socket(connection);
			return transport_error(ErrorCode::AgenticUnavailable, native_code);
		}
		if (connected != 0 && !await_socket(connection, POLLOUT,
											std::chrono::steady_clock::now() +
												std::chrono::milliseconds(endpoint.timeout_ms))) {
			close_socket(connection);
			return transport_error(ErrorCode::AgenticTimeout);
		}
		int connection_error = 0;
		socklen_t error_length = sizeof(connection_error);
		if (::getsockopt(connection, SOL_SOCKET, SO_ERROR, &connection_error, &error_length) != 0 ||
			connection_error != 0 || !owned_peer(connection) ||
			!write_message(connection, bytes, endpoint.timeout_ms)) {
			close_socket(connection);
			return transport_error(ErrorCode::AgenticUnavailable, connection_error);
		}
		auto response = read_message(connection, endpoint);
		close_socket(connection);
#endif
		if (!response)
			return unexpectedError(response.error());
		return parse_json(*response, endpoint.maximum_message_bytes);
	} catch (...) {
		return transport_error(ErrorCode::AgenticBudgetExhausted);
	}
}

} // namespace FlowUi::devSystems::agentic
