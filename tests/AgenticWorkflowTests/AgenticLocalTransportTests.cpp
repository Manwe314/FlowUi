#include "devSystems/devAgenticWorkflow/AgenticLocalTransport.hpp"
#include "devSystems/devAgenticWorkflow/AgenticTypes.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

using namespace FlowUi;
using namespace FlowUi::devSystems::agentic;
namespace {
void require(bool condition, std::string_view message) {
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}
} // namespace

int main() {
	const auto identity = make_identifier("transport");
	require(bool(identity), "transport identity unavailable");
	AgenticLocalEndpoint endpoint;
#ifdef _WIN32
	endpoint.endpoint = "\\\\.\\pipe\\flowui-" + *identity;
#else
	endpoint.endpoint = (std::filesystem::temp_directory_path() / (*identity + ".sock")).string();
#endif
	endpoint.run_id = *identity;
	endpoint.nonce = "owner-private-concurrent-transport-test";
	endpoint.timeout_ms = 5000;
	std::mutex gate_mutex;
	std::condition_variable gate;
	std::size_t waiting_clients = 0;
	bool released = false;
	std::atomic<std::uint64_t> sideEffects{0};
	AgenticLocalServer server;
	const auto started = server.start(endpoint, [&](const Json& request) {
		const auto* operation = request.find("operation");
		if (operation && operation->string_or() == "self-stop") {
			const auto stopped = server.stop();
			return Json::make_object({{"self_stop_rejected",
				!stopped && stopped.error().code == ErrorCode::AgenticBusy}});
		}
		if (operation && operation->string_or() == "throw-after-dispatch") {
			++sideEffects;
			throw std::runtime_error("application handler failed after its side effect");
		}
		if (operation && operation->string_or() == "wait") {
			std::unique_lock lock(gate_mutex);
			++waiting_clients;
			gate.notify_all();
			gate.wait_for(lock, std::chrono::seconds(4), [&] { return released; });
			return Json::make_object({{"released", released}});
		}
		if (operation && operation->string_or() == "cancel") {
			std::lock_guard lock(gate_mutex);
			released = true;
			gate.notify_all();
			return Json::make_object({{"canceled", true}});
		}
		return Json::make_object({{"echo", request}});
	});
	require(bool(started), "local server did not start");
	std::array<std::thread, 4> clients;
	std::array<bool, 4> outcomes{};
	for (std::size_t client_index = 0; client_index < clients.size(); ++client_index)
		clients[client_index] = std::thread([&, client_index] {
			const auto response =
				send_local_command(endpoint, Json::make_object({{"operation", "wait"}}));
			outcomes[client_index] =
				response && response->find("released") && response->find("released")->bool_or();
		});
	bool concurrent = false;
	{
		std::unique_lock lock(gate_mutex);
		concurrent = gate.wait_for(lock, std::chrono::seconds(2),
								   [&] { return waiting_clients == clients.size(); });
	}
	const auto cancel_started = std::chrono::steady_clock::now();
	const auto canceled =
		send_local_command(endpoint, Json::make_object({{"operation", "cancel"}}));
	const auto cancel_elapsed = std::chrono::steady_clock::now() - cancel_started;
	{
		std::lock_guard lock(gate_mutex);
		released = true;
		gate.notify_all();
	}
	for (auto& client : clients)
		client.join();
	require(concurrent, "bounded waits serialized local command admission");
	require(canceled && canceled->find("canceled") && canceled->find("canceled")->bool_or(),
			"cancellation could not enter while waits were pending");
	require(cancel_elapsed < std::chrono::seconds(1), "cancellation waited behind long requests");
	for (const auto outcome : outcomes)
		require(outcome, "concurrent wait did not observe cancellation");
	const auto echo = send_local_command(endpoint, Json::make_object({{"value", "authenticated"}}));
	require(echo && echo->find("echo"), "concurrent server lost ordinary request handling");
	const auto selfStop = send_local_command(endpoint,
		Json::make_object({{"operation", "self-stop"}}));
	require(selfStop && selfStop->find("self_stop_rejected") &&
		selfStop->find("self_stop_rejected")->bool_or() && server.running(),
		"worker stop did not preserve owner cleanup after rejecting self-join");
	const auto failedHandler = send_local_command(endpoint,
		Json::make_object({{"operation", "throw-after-dispatch"}}));
	require(failedHandler && failedHandler->find("error") &&
		failedHandler->find("error")->find("code")->string_or() == "handler_failed" &&
		sideEffects.load() == 1,
		"failed handler lost its explicit failure or was automatically redispatched");
	auto unauthorized = endpoint;
	unauthorized.nonce = "wrong-owner-private-nonce";
	const auto rejected = send_local_command(unauthorized, Json::make_object());
	require(rejected && rejected->find("error") &&
				rejected->find("error")->find("message")->string_or() ==
					"stale_or_unauthorized_run",
			"concurrency bypassed endpoint authentication");
	require(bool(server.stop()), "owner could not stop and join transport workers");
	require(!server.running(), "transport workers remained active after stop");
	require(!send_local_command(endpoint, Json::make_object()),
			"stopped endpoint accepted commands");
	require(bool(server.start(endpoint, [](const Json& request) { return request; })),
			"stopped transport could not restart");
	require(bool(server.stop()), "owner could not stop restarted transport");
	return 0;
}
