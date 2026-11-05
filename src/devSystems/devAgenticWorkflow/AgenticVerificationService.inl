// Private service implementation. Included after AgenticWorkflowService::Impl is complete.
Json AgenticWorkflowService::Impl::verification_plan(const AgenticCommandRequest& request) {
	const auto* supplied = request.arguments.find("request");
	if (!supplied)
		return failure(request, agentic_error(AgenticError::invalid_request));
	auto normalized = normalize_verification_request(*supplied);
	if (!normalized)
		return failure(request, normalized.error());
	const auto selected_instance = argument_string(*normalized, "instance");
	if (!selected_instance.empty() && selected_instance != "run_selected" &&
		selected_instance != config.run_id)
		return failure(request, agentic_error(AgenticError::invalid_scope));
	auto rejected = Json::make_array();
	auto resolved = Json::make_array();
	const auto& limits = *normalized->find("limits");
	if (argument_integer(limits, "maximum_snapshot_bytes") > config.limits.maximum_snapshot_bytes ||
		argument_integer(limits, "maximum_images") > config.limits.maximum_images)
		rejected.array.emplace_back("requested_limits_exceed_runtime_reservation");
	if (argument_integer(limits, "maximum_report_bytes") <
		2048 + normalized->find("cases")->array.size() * 768)
		rejected.array.emplace_back("report_budget_cannot_retain_case_summaries");
	for (const auto& case_request : normalized->find("cases")->array) {
		const auto& workload = *case_request.find("workload");
		auto resolution = Json::make_object(
			{{"case_id", *case_request.find("case_id")}, {"kind", *workload.find("kind")}});
		if (argument_string(workload, "kind") == "registered_scenario") {
			const auto selected =
				std::find_if(scenarios.begin(), scenarios.end(), [&](const auto& scenario) {
					return scenario.descriptor.scenario_id ==
						   argument_string(workload, "scenario_id");
				});
			if (selected == scenarios.end())
				rejected.array.emplace_back("scenario_unavailable:" +
											argument_string(workload, "scenario_id"));
			else {
				const auto& descriptor = selected->descriptor;
				if (descriptor.revision != argument_string(workload, "revision") ||
					descriptor.fixture_id != argument_string(workload, "fixture_id") ||
					(workload.find("digest") &&
					 descriptor.digest != argument_string(workload, "digest")))
					rejected.array.emplace_back("stale_scenario_or_fixture:" +
												descriptor.scenario_id);
				if (descriptor.mode != ExecutionMode::windowed)
					rejected.array.emplace_back("verification_requires_windowed_scenario");
				resolution.set("descriptor", to_json(descriptor));
			}
		} else {
			if (!interaction || !config.input_control_enabled)
				rejected.array.emplace_back("input_control_unavailable");
			const auto decoded = interaction_plan_from_json(*workload.find("plan"));
			if (decoded) {
				if ((!decoded->run_id.empty() && decoded->run_id != config.run_id) ||
					decoded->window_id > UINT32_MAX ||
					!dependencies.application->hasWindow(static_cast<WindowId>(decoded->window_id)))
					rejected.array.emplace_back("interaction_target_window_unavailable");
				resolution.set("fixture_id", decoded->fixture_id);
				resolution.set("window_id", decimal(decoded->window_id));
			}
		}
		for (const auto& checkpoint : case_request.find("checkpoints")->array) {
			const auto evidence = argument_string(checkpoint, "evidence");
			if (checkpoint.find("required")->bool_or() &&
				((evidence != "tree" && !config.visual_capture_enabled) ||
				 (evidence != "image" && !capabilities.trees)))
				rejected.array.emplace_back("required_checkpoint_capability_unavailable");
		}
		resolved.array.emplace_back(std::move(resolution));
	}
	return command_success(
		request,
		Json::make_object(
			{{"normalized_request", *normalized},
			 {"request_digest", verification_request_digest(*normalized)},
			 {"feasible", rejected.array.empty()},
			 {"rejected_reasons", std::move(rejected)},
			 {"resolved_workloads", std::move(resolved)},
			 {"limits", limits},
			 {"estimated_bytes",
			  Json::make_object({{"snapshot_reservation", *limits.find("maximum_snapshot_bytes")},
								 {"retained_ceiling", *limits.find("maximum_retained_bytes")}})},
			 {"case_dependencies", "sequential_clean_fixture"},
			 {"required_capabilities", Json::make_array({"scenarios_or_input_control"})}}));
}

Json AgenticWorkflowService::Impl::verification_response(const AgenticCommandRequest& request,
														 const Verification& verification) {
	if (verification.terminal && request.operation != "verification.status" &&
		request.operation != "verification.cancel") {
		auto detail = request.operation == "verification.report" ? std::string("all")
																 : verification.report_detail;
		if (const auto* supplied = request.arguments.find("request"))
			if (const auto* preference = supplied->find("report"))
				if (const auto* selected = preference->find("detail"))
					detail = selected->string_or("failures");
		auto page = verification_report_page(verification.report, request.arguments, detail);
		if (!page)
			return failure(request, page.error());
		auto response = command_success(request, std::move(*page));
		response.set("job_id", verification.job_id);
		response.set("run_id", config.run_id);
		return response;
	}
	auto progress = Json::make_object(
		{{"job_id", verification.job_id},
		 {"suite_id", verification.terminal ? *verification.report.find("suite_id")
											: *verification.request.find("suite_id")},
		 {"phase", verification.phase},
		 {"sequence", decimal(verification.sequence)},
		 {"terminal", verification.terminal},
		 {"execution_complete", verification.publishing || verification.terminal},
		 {"report_published", verification.report_published},
		 {"cases_completed", decimal(verification.case_index)},
		 {"total_cases",
		  decimal(verification.terminal ? verification.report.find("cases")->array.size()
										: verification.request.find("cases")->array.size())},
		 {"cancellation_requested", verification.canceled},
		 {"suggested_wait_timeout_ms", Json::number(20000)}});
	auto response = command_success(request, std::move(progress),
									verification.terminal ? "completed" : "accepted");
	response.set("job_id", verification.job_id);
	response.set("run_id", config.run_id);
	return response;
}

Json AgenticWorkflowService::Impl::verification_command(const AgenticCommandRequest& request) {
	if (request.operation == "verification.plan")
		return verification_plan(request);
	if (request.operation == "verification.run") {
		const auto admission_started_ns = monotonic_nanoseconds();
		const auto* supplied = request.arguments.find("request");
		if (!supplied)
			return failure(request, agentic_error(AgenticError::invalid_request));
		auto normalized = normalize_verification_request(*supplied);
		if (!normalized)
			return failure(request, normalized.error());
		const auto selected_instance = argument_string(*normalized, "instance");
		if (!selected_instance.empty() && selected_instance != "run_selected" &&
			selected_instance != config.run_id)
			return failure(request, agentic_error(AgenticError::invalid_scope));
		const auto key = argument_string(*normalized, "idempotency_key");
		const auto digest = verification_request_digest(*normalized);
		if (key.empty())
			return command_failure(request, 2, "idempotency_key_required",
								   "A deliberate execution requires a nonempty retry key.");
		const auto now = monotonic_nanoseconds();
		constexpr std::uint64_t retention_ns = 86'400'000'000'000ull;
		std::erase_if(verification_receipts, [&](const auto& receipt) {
			return now - receipt.admitted_ns > retention_ns;
		});
		for (const auto& receipt : verification_receipts)
			if (receipt.key == key) {
				if (receipt.digest != digest)
					return command_failure(request, 2, "idempotency_conflict",
										   "This key already identifies a different execution.");
				if (const auto* verification = find_verification(receipt.job_id))
					return verification_response(request, *verification);
				auto expired = command_failure(request, 6, "result_expired",
											   "The retry record survives the bounded runtime "
											   "report history; read the sealed report offline.");
				expired.set("job_id", receipt.job_id);
				expired.set("run_id", config.run_id);
				expired.set("next",
							Json::make_object(
								{{"operation", "verification.report"},
								 {"arguments", Json::make_object({{"instance", config.run_id},
																  {"job_id", receipt.job_id}})}}));
				return expired;
			}
		const auto planned = verification_plan(request);
		if (argument_integer(planned, "exit_code") != 0)
			return planned;
		const auto* plan = planned.find("result");
		if (!plan || !plan->find("feasible")->bool_or())
			return command_failure(
				request, 3, "verification_plan_rejected",
				"Replan with current scenario revisions, capabilities and budgets.");
		if (verification_dirty)
			return command_failure(
				request, 6, "dirty_fixture",
				"Required cleanup did not establish a usable fixture; restart the instance.");
		if (active_verification() || active() || !publishing_job.empty() || manual_busy ||
			(interaction && interaction->active()) ||
			std::any_of(jobs.begin(), jobs.end(),
						[](const auto& job) { return !terminal(job->status.phase); }))
			return failure(request, agentic_error(AgenticError::busy));
		if (verification_receipts.size() >= 4096)
			return command_failure(request, 3, "idempotency_capacity_exhausted",
								   "Retry records are protected for 24 hours; admission is bounded "
								   "to 4096 executions per horizon.");
		while (verifications.size() >= 32 && verifications.front()->terminal)
			verifications.erase(verifications.begin());
		auto identity = make_identifier("verification");
		if (!identity)
			return failure(request, identity.error());
		auto verification = std::make_unique<Verification>();
		verification->request = std::move(*normalized);
		verification->report_detail =
			argument_string(*verification->request.find("report"), "detail");
		verification->job_id = *identity;
		verification->digest = digest;
		verification->key = key;
		verification->admitted_ns = verification->phase_started_ns = now;
		verification->admission_ns = monotonic_nanoseconds() - admission_started_ns;
		verification->cases.array.reserve(verification->request.find("cases")->array.size());
		verification->interaction_job.reserve(128);
		verification_receipts.emplace_back(VerificationReceipt{key, digest, *identity, now});
		verifications.emplace_back(std::move(verification));
		return verification_response(request, *verifications.back());
	}
	const auto identity = argument_string(request.arguments, "job_id");
	auto* verification = find_verification(identity);
	if (!verification)
		return command_failure(
			request, 6, "result_expired",
			"This job is absent from runtime history; use its sealed report in the store.");
	if (request.operation == "verification.cancel" && !verification->terminal &&
		!verification->publishing) {
		verification->canceled = true;
		++verification->sequence;
		condition.notify_all();
	}
	return verification_response(request, *verification);
}

void AgenticWorkflowService::Impl::seal_verification(Verification& verification) {
	if (verification.publishing || verification.terminal)
		return;
	if (verification.current.find("case_id") &&
		verification.cases.array.size() == verification.case_index) {
		verification.current.set(
			"cleanup", Json::make_object({{"completed", false}, {"fixture_usable", false}}));
		if (verification.current.find("assertions")->array.empty())
			verification.current.find("assertions")
				->array.emplace_back(Json::make_object({{"assertion_id", "workload_completed"},
														{"required", true},
														{"available", false}}));
		const auto current_coverage =
			verification_assertion_coverage(Json::make_array({verification.current}));
		verification.current.set("verdict",
								 verification_report_verdict(current_coverage, false, false));
		verification.cases.array.emplace_back(std::move(verification.current));
		++verification.case_index;
	}
	const auto& requested_cases = verification.request.find("cases")->array;
	for (std::size_t case_index = verification.case_index; case_index < requested_cases.size();
		 ++case_index) {
		const auto& skipped = requested_cases[case_index];
		verification.cases.array.emplace_back(Json::make_object(
			{{"case_id", *skipped.find("case_id")},
			 {"verdict", "inconclusive"},
			 {"assertions",
			  Json::make_array({Json::make_object({{"assertion_id", "workload_completed"},
												   {"required", true},
												   {"skipped", true}})})},
			 {"checkpoints", Json::make_array()},
			 {"cleanup",
			  Json::make_object({{"completed", true}, {"fixture_usable", !verification.dirty}})}}));
	}
	auto coverage = verification_assertion_coverage(verification.cases);
	const auto verdict = verification_report_verdict(
		coverage, verification.execution_status == "completed", !verification.dirty);
	auto timings = Json::make_object(
		{{"total_elapsed_ns", decimal(monotonic_nanoseconds() - verification.admitted_ns)},
		 {"admission_ns", decimal(verification.admission_ns)},
		 {"queue_ns", decimal(verification.queue_ns)},
		 {"setup_ns", decimal(verification.setup_ns)},
		 {"execution_ns", decimal(verification.execution_ns)},
		 {"assertion_ns", decimal(verification.assertion_ns)},
		 {"publication_ns", decimal(verification.publication_ns)}});
	for (const auto timing : {"readback_ns", "report_ns"})
		timings.set(timing, Json{});
	auto limitations = Json::make_array(
		{"unavailable_pipeline_spans_are_null", "rss_gpu_and_encoder_workspace_unavailable",
		 "automatic_disk_retention_not_enabled", "historical_failure_pixels_unavailable"});
	if (!verification.termination_reason.empty())
		limitations.array.emplace_back(verification.termination_reason);
	if (verification.request.find("report")->find("include_contact_sheet")->bool_or())
		limitations.array.emplace_back(
			"optional_contact_sheet_unavailable_use_original_image_selectors");
	verification.report = Json::make_object(
		{{"schema", "FlowUi.Agentic.VerificationReport"},
		 {"schema_version", "1.0"},
		 {"job_id", verification.job_id},
		 {"run_id", config.run_id},
		 {"suite_id", *verification.request.find("suite_id")},
		 {"suite_revision", *verification.request.find("suite_revision")},
		 {"request_digest", verification.digest},
		 {"execution_status", verification.execution_status},
		 {"verdict", verdict},
		 {"coverage", std::move(coverage)},
		 {"cases", verification.cases},
		 {"timings", std::move(timings)},
		 {"resources",
		  Json::make_object(
			  {{"peak_owned_evidence_bytes", decimal(verification.peak_owned_bytes)},
			   {"readback_reserved_bytes", argument_string(*verification.request.find("evidence"),
														   "policy") == "assertions_only"
											   ? decimal(0)
											   : Json{}},
			   {"completed_rgb_bytes_peak", decimal(verification.readback_bytes)},
			   {"encoded_bytes", Json{}},
			   {"disk_bytes", decimal(verification.disk_bytes)},
			   {"measurement_availability", "owned_snapshot_and_inventory_only"}})},
		 {"evidence",
		  Json::make_object({{"selector", Json::make_object({{"instance", config.run_id},
															 {"job_id", verification.job_id}})},
							 {"image_count", decimal(verification.image_count)},
							 {"contact_sheet", Json{}}})},
		 {"cleanup", Json::make_object({{"completed", !verification.dirty},
										{"fixture_usable", !verification.dirty},
										{"errors", Json::make_array()}})},
		 {"limitations", std::move(limitations)}});
	if (verification.dirty)
		verification.report.find("cleanup")->find("errors")->array.emplace_back(
			"required_cleanup_incomplete");
	const auto maximum_report =
		argument_integer(*verification.request.find("limits"), "maximum_report_bytes");
	if (write_json(verification.report).size() > maximum_report) {
		// Aggregates remain canonical; detailed records remain in each case's sealed capture.
		for (auto& case_report : verification.report.find("cases")->array) {
			case_report.set("assertions", Json::make_array());
			case_report.set("details_omitted", true);
		}
		verification.report.find("limitations")
			->array.emplace_back("assertion_details_in_case_capture");
	}
	if (write_json(verification.report).size() > maximum_report) {
		for (auto& case_report : verification.report.find("cases")->array) {
			case_report.set("checkpoints", Json::make_array());
			std::erase_if(case_report.object, [](const auto& field) {
				return field.first == "workload" || field.first == "manifest";
			});
		}
		verification.report.find("limitations")
			->array.emplace_back("checkpoint_details_in_case_capture");
	}
	verification.publishing = true;
	verification.phase = "publishing";
	++verification.sequence;
	verification_publication.emplace(VerificationPublication{
		verification.report, run_path / "verification" / (verification.job_id + ".json"),
		verification.job_id});
	condition.notify_all();
}

void AgenticWorkflowService::Impl::advance_verification() {
	auto* verification = active_verification();
	if (!verification || verification->publishing)
		return;
	auto& suite = *verification;
	const auto now = monotonic_nanoseconds();
	const auto transition = [&](std::string_view phase) {
		suite.phase = phase;
		suite.phase_started_ns = now;
		++suite.sequence;
	};
	const auto& case_requests = suite.request.find("cases")->array;
	if (suite.case_index >= case_requests.size()) {
		seal_verification(suite);
		return;
	}
	const auto& case_request = case_requests[suite.case_index];
	const auto& workload = *case_request.find("workload");
	const auto deadline = argument_integer(suite.request, "execution_timeout_ns");
	const bool scenario_expired =
		suite.scenario_index != SIZE_MAX && suite.case_started_ns != 0 &&
		now - suite.case_started_ns >= scenarios[suite.scenario_index].descriptor.deadline_ns;
	if ((suite.canceled || scenario_expired || now - suite.admitted_ns >= deadline) &&
		suite.phase != "cleanup" && suite.phase != "draining") {
		suite.execution_status = suite.canceled ? "canceled" : "timeout";
		suite.case_incomplete = true;
		if (suite.child_job.empty()) {
			seal_verification(suite);
			return;
		}
		if (interaction && interaction->active()) {
			if (!suite.interaction_job.empty()) {
				const auto canceled = owner_service->cancel_job(suite.interaction_job);
				suite.dirty |= !canceled;
			}
		}
		transition("cleanup");
	}
	if (suite.child_job.empty()) {
		AgenticCaptureRequest capture;
		capture.limits = config.limits;
		capture.limits.maximum_snapshot_bytes =
			argument_integer(*suite.request.find("limits"), "maximum_snapshot_bytes");
		capture.limits.maximum_image_bytes =
			std::min(config.limits.maximum_image_bytes, capture.limits.maximum_snapshot_bytes / 4);
		capture.limits.maximum_images = std::max<std::uint64_t>(
			1, argument_integer(*suite.request.find("limits"), "maximum_images") -
				   std::min(suite.image_count,
							argument_integer(*suite.request.find("limits"), "maximum_images")));
		capture.limits.maximum_records =
			std::min<std::uint64_t>(4096, config.limits.maximum_records);
		const auto retained_limit =
			argument_integer(*suite.request.find("limits"), "maximum_retained_bytes");
		const auto report_reservation =
			argument_integer(*suite.request.find("limits"), "maximum_report_bytes");
		if (suite.disk_bytes >= retained_limit ||
			report_reservation >= retained_limit - suite.disk_bytes) {
			suite.execution_status = "failed";
			seal_verification(suite);
			return;
		}
		capture.limits.maximum_disk_bytes = retained_limit - suite.disk_bytes - report_reservation;
		capture.duration_ns = deadline;
		capture.maximum_expected_tick_cadence = 1;
		capture.performance = capture.runtime_diagnostics = capture.memory =
			SectionRequirement::omit;
		capture.visual = case_request.find("checkpoints")->array.empty()
							 ? SectionRequirement::omit
							 : SectionRequirement::optional;
		capture.capture_window = true;
		capture.content.include_tree_text =
			suite.request.find("evidence")->find("include_tree_text")->bool_or();
		capture.content.include_native_messages =
			suite.request.find("evidence")->find("include_native_messages")->bool_or();
		capture.content.include_stacks =
			suite.request.find("evidence")->find("include_stacks")->bool_or();
		capture.content.include_source_paths = false;
		capture.content.include_interaction_outcomes =
			suite.request.find("evidence")->find("include_interaction_outcomes")->bool_or();
		capture.fixture_id = argument_string(workload, "fixture_id");
		if (argument_string(workload, "kind") == "interaction_plan") {
			capture.window_ids.emplace_back(argument_integer(*workload.find("plan"), "window_id"));
		} else {
			for (std::size_t scenario_index = 0; scenario_index < scenarios.size();
				 ++scenario_index)
				if (scenarios[scenario_index].descriptor.scenario_id ==
					argument_string(workload, "scenario_id"))
					suite.scenario_index = scenario_index;
			capture.seed = argument_integer(workload, "seed");
		}
		verification_admission = true;
		auto admitted = owner_service->request_capture(capture);
		verification_admission = false;
		if (!admitted) {
			suite.execution_status = "failed";
			suite.termination_reason = errorName(admitted.error().code);
			seal_verification(suite);
			return;
		}
		suite.child_job = *admitted;
		find_job(*admitted)->verification_owned = true;
		find_job(*admitted)->snapshot.provenance.set("verification_job_id", suite.job_id);
		if (suite.scenario_index != SIZE_MAX) {
			auto& metadata = find_job(*admitted)->snapshot.request;
			const auto& descriptor = scenarios[suite.scenario_index].descriptor;
			metadata.scenario_id = descriptor.scenario_id;
			metadata.scenario_revision = descriptor.revision;
			metadata.scenario_digest = descriptor.digest;
		}
		suite.current = Json::make_object(
			{{"case_id", *case_request.find("case_id")},
			 {"fixture_id", capture.fixture_id},
			 {"capture_id", find_job(*admitted)->status.capture_id},
			 {"assertions", Json::make_array()},
			 {"checkpoints", Json::make_array()},
			 {"cleanup", Json::make_object({{"completed", false}, {"fixture_usable", false}})}});
		auto workload_identity = Json::make_object({{"kind", *workload.find("kind")}});
		for (const auto field : {"scenario_id", "revision", "fixture_id", "digest", "seed"})
			if (const auto* value = workload.find(field))
				workload_identity.set(field, *value);
		if (suite.scenario_index != SIZE_MAX)
			workload_identity.set("digest", scenarios[suite.scenario_index].descriptor.digest);
		workload_identity.set("seed", decimal(capture.seed));
		suite.current.set("workload", std::move(workload_identity));
		suite.case_started_ns = now;
		transition("preparing");
		return;
	}
	auto* child = find_job(suite.child_job);
	if (!child)
		return;
	if (const auto owned = snapshot_owned_bytes(child->snapshot); owned)
		suite.peak_owned_bytes = std::max(suite.peak_owned_bytes, *owned);
	if (terminal(child->status.phase) && suite.phase != "draining") {
		suite.case_incomplete = true;
		suite.dirty = true;
		suite.execution_status = "failed";
		transition("draining");
	}
	if (child->status.phase == JobPhase::finalizing && suite.phase != "draining" &&
		suite.phase != "cleanup") {
		suite.case_incomplete = true;
		suite.execution_status = "failed";
		transition("cleanup");
	}
	if (suite.phase == "preparing") {
		if (child->status.phase != JobPhase::recording)
			return;
		suite.queue_ns += now - suite.case_started_ns;
		transition(suite.scenario_index == SIZE_MAX ? "interaction" : "setup");
	}
	if (suite.phase == "interaction") {
		if (suite.interaction_job.empty()) {
			auto admitted = interaction->start_plan(*workload.find("plan"), {}, suite.child_job);
			if (!admitted) {
				suite.case_incomplete = true;
				transition("checkpoints");
			} else
				suite.interaction_job = *admitted;
			return;
		}
		const auto result = interaction->query_result(suite.interaction_job);
		if (!result || (result->phase != AgenticInteractionPhase::completed &&
						result->phase != AgenticInteractionPhase::failed &&
						result->phase != AgenticInteractionPhase::canceled))
			return;
		for (const auto& step : result->steps) {
			if (step.expected == AgenticInteractionExpectation::none)
				continue;
			suite.current.find("assertions")
				->array.emplace_back(Json::make_object(
					{{"assertion_id", step.step_id},
					 {"required", true},
					 {"available", step.verdict == AgenticInteractionVerdict::passed ||
									   step.verdict == AgenticInteractionVerdict::failed},
					 {"passed", step.verdict == AgenticInteractionVerdict::passed},
					 {"expected", Json::number(static_cast<std::uint32_t>(step.expected))},
					 {"observed", Json::number(static_cast<std::uint32_t>(step.observed))},
					 {"reason", step.reason}}));
		}
		suite.current.find("workload")->set("plan_digest", result->plan_digest);
		suite.case_failed = result->assertion_verdict == AgenticInteractionVerdict::failed;
		suite.case_incomplete = !result->execution_succeeded || !result->evidence_complete;
		suite.dirty = !result->cleanup_complete;
		transition("checkpoints");
	}
	if (suite.phase == "setup" || suite.phase == "step" || suite.phase == "verify") {
		const auto& scenario = scenarios[suite.scenario_index];
		const auto& callback = suite.phase == "setup"  ? scenario.setup
							   : suite.phase == "step" ? scenario.step
													   : scenario.verify;
		const auto callback_phase = suite.phase;
		const auto callback_started_ns = monotonic_nanoseconds();
		const auto result = scenario_callback(*child, callback,
											  callback_phase == "setup"	 ? "setup_readiness"
											  : callback_phase == "step" ? "measured_step"
																		 : "assertions");
		{
			for (; suite.harvested_actions < child->snapshot.actions.array.size();
				 ++suite.harvested_actions) {
				const auto& action = child->snapshot.actions.array[suite.harvested_actions];
				if (!action.find("assertion") || !action.find("assertion")->bool_or())
					continue;
				auto assertion = Json::make_object({{"assertion_id", *action.find("action")},
													{"required", true},
													{"available", true},
													{"passed", *action.find("passed")}});
				for (const auto key : {"app_tick", "state_revision", "observed_frame"})
					if (const auto* value = action.find(key))
						assertion.set(key, *value);
				if (child->snapshot.request.content.include_interaction_outcomes)
					assertion.set("outcome", *action.find("arguments"));
				suite.case_failed |= !action.find("passed")->bool_or();
				suite.current.find("assertions")->array.emplace_back(std::move(assertion));
			}
		}
		const auto callback_duration_ns = monotonic_nanoseconds() - callback_started_ns;
		if (callback_phase == "setup")
			suite.setup_ns += callback_duration_ns;
		else if (callback_phase == "step")
			suite.execution_ns += callback_duration_ns;
		else
			suite.assertion_ns += callback_duration_ns;
		if (result && *result == AgenticScenarioProgress::pending)
			return;
		if (!result || suite.case_failed) {
			suite.case_incomplete |= !result;
			transition("checkpoints");
		} else if (callback_phase == "setup")
			transition("step");
		else if (callback_phase == "step")
			transition("verify");
		else
			transition("checkpoints");
		return;
	}
	if (suite.phase == "checkpoints") {
		const auto& checkpoints = case_request.find("checkpoints")->array;
		while (suite.checkpoint_index < checkpoints.size()) {
			const auto& checkpoint = checkpoints[suite.checkpoint_index];
			const bool failure_trigger = argument_string(checkpoint, "trigger") == "case_failed";
			if (failure_trigger != (suite.case_failed || suite.case_incomplete)) {
				++suite.checkpoint_index;
				continue;
			}
			const auto evidence = argument_string(checkpoint, "evidence");
			if (!child->checkpoint_active && evidence != "tree" &&
				suite.image_count >=
					argument_integer(*suite.request.find("limits"), "maximum_images")) {
				suite.current.find("checkpoints")
					->array.emplace_back(
						Json::make_object({{"checkpoint_id", *checkpoint.find("checkpoint_id")},
										   {"required", *checkpoint.find("required")},
										   {"available", false},
										   {"passed", false},
										   {"reason", "image_budget_exhausted"}}));
				suite.case_incomplete |= checkpoint.find("required")->bool_or();
				++suite.checkpoint_index;
				continue;
			}
			if (!child->checkpoint_active) {
				child->checkpoint_active = true;
				child->checkpoint_admitted = false;
				child->checkpoint_tree_recorded = false;
				suite.checkpoint_image_begin = child->snapshot.images.size();
				suite.checkpoint_tree_begin = child->snapshot.artifacts.size();
				child->checkpoint_image = evidence != "tree";
				child->checkpoint_tree = evidence != "image";
				child->checkpoint_window = argument_integer(checkpoint, "window_id");
				if (child->checkpoint_window == 0 && !child->snapshot.request.window_ids.empty())
					child->checkpoint_window = child->snapshot.request.window_ids.front();
				if (std::find(child->snapshot.request.window_ids.begin(),
							  child->snapshot.request.window_ids.end(),
							  child->checkpoint_window) == child->snapshot.request.window_ids.end())
					child->snapshot.request.window_ids.emplace_back(child->checkpoint_window);
				suite.checkpoint_tick = safe_point.app_tick;
				suite.phase_started_ns = now;
				return;
			}
			const auto image =
				std::find_if(child->snapshot.images.begin() +
								 static_cast<std::ptrdiff_t>(suite.checkpoint_image_begin),
							 child->snapshot.images.end(), [&](const auto& value) {
								 return value.window_id == child->checkpoint_window &&
										value.app_tick >= suite.checkpoint_tick;
							 });
			const auto tree = std::find_if(
				child->snapshot.artifacts.begin() +
					static_cast<std::ptrdiff_t>(suite.checkpoint_tree_begin),
				child->snapshot.artifacts.end(), [&](const auto& value) {
					return value.schema == "FlowUi.Agentic.Tree" &&
						   (image != child->snapshot.images.end()
								? value.artifact_id ==
									  (image->tree_artifact_id.empty()
										   ? "tree_" + std::to_string(image->window_id) + "_" +
												 std::to_string(image->tree_generation)
										   : image->tree_artifact_id)
								: value.artifact_id.starts_with(
									  "tree_" + std::to_string(child->checkpoint_window) + "_"));
				});
			const bool complete =
				(!child->checkpoint_image || image != child->snapshot.images.end()) &&
				(!child->checkpoint_tree || tree != child->snapshot.artifacts.end());
			const bool expired = now - suite.phase_started_ns >= 2'000'000'000ull ||
								 safe_point.app_tick - suite.checkpoint_tick >= 120;
			if (!complete && !expired)
				return;
			auto record = Json::make_object(
				{{"checkpoint_id", *checkpoint.find("checkpoint_id")},
				 {"required", *checkpoint.find("required")},
				 {"available", complete},
				 {"passed", complete},
				 {"capture_id", child->status.capture_id},
				 {"trigger", *checkpoint.find("trigger")},
				 {"temporal_relation", failure_trigger ? "next_eligible_frame_after_failure"
													   : "after_required_observation"}});
			if (image != child->snapshot.images.end()) {
				record.set("image", image_metadata(*image));
				suite.readback_bytes =
					std::max<std::uint64_t>(suite.readback_bytes, image->rgb.size());
			}
			if (tree != child->snapshot.artifacts.end())
				record.set("tree_artifact", tree->artifact_id);
			if (!complete && checkpoint.find("required")->bool_or())
				suite.case_incomplete = true;
			suite.current.find("checkpoints")->array.emplace_back(std::move(record));
			child->checkpoint_active = false;
			++suite.checkpoint_index;
		}
		transition("cleanup");
	}
	if (suite.phase == "cleanup") {
		if (!suite.cleanup_started) {
			if (interaction && interaction->active() && !suite.interaction_job.empty()) {
				const auto canceled = owner_service->cancel_job(suite.interaction_job);
				suite.dirty |= !canceled;
				if (now - suite.phase_started_ns < 2'000'000'000ull)
					return;
				suite.dirty = true;
			}
			suite.cleanup_started = true;
		}
		if (suite.scenario_index != SIZE_MAX) {
			const auto result =
				scenario_callback(*child, scenarios[suite.scenario_index].cleanup, "cleanup");
			if (result && *result == AgenticScenarioProgress::pending &&
				now - suite.phase_started_ns < 2'000'000'000ull)
				return;
			suite.dirty |= !result || *result == AgenticScenarioProgress::pending;
		}
		if (interaction && interaction->active() &&
			!interaction->ready_to_materialize(suite.child_job)) {
			if (now - suite.phase_started_ns < 2'000'000'000ull)
				return;
			if (!suite.interaction_job.empty()) {
				const auto canceled = owner_service->cancel_job(suite.interaction_job);
				suite.dirty |= !canceled;
			}
			suite.dirty = true;
		}
		if (suite.scenario_index != SIZE_MAX) {
			for (const auto& declaration :
				 scenarios[suite.scenario_index].descriptor.assertions.array) {
				const auto assertion_id = declaration.kind == Json::Kind::string
											  ? std::string(declaration.string_or())
											  : argument_string(declaration, "id");
				if (!assertion_id.empty() &&
					std::none_of(
						suite.current.find("assertions")->array.begin(),
						suite.current.find("assertions")->array.end(), [&](const auto& assertion) {
							return argument_string(assertion, "assertion_id") == assertion_id;
						}))
					suite.current.find("assertions")
						->array.emplace_back(Json::make_object({{"assertion_id", assertion_id},
																{"required", true},
																{"available", false}}));
			}
		}
		if (suite.current.find("assertions")->array.empty())
			suite.current.find("assertions")
				->array.emplace_back(
					Json::make_object({{"assertion_id", "required_semantic_assertions"},
									   {"required", true},
									   {"available", false}}));
		suite.current.set("cleanup", Json::make_object({{"completed", !suite.dirty},
														{"fixture_usable", !suite.dirty}}));
		suite.current.set("duration_ns", decimal(now - suite.case_started_ns));
		child->checkpoint_active = false;
		child->stop_requested = true;
		transition("draining");
	}
	if (suite.phase == "draining") {
		if (!terminal(child->status.phase))
			return;
		if (child->status.phase != JobPhase::sealed)
			suite.case_incomplete = true;
		if (!child->status.reason.empty())
			suite.current.set("evidence_status_reason", child->status.reason);
		if (!suite.interaction_job.empty()) {
			const auto interaction_result = interaction->query_result(suite.interaction_job);
			suite.dirty |= !interaction_result || !interaction_result->cleanup_complete;
			suite.current.set("cleanup", Json::make_object({{"completed", !suite.dirty},
															{"fixture_usable", !suite.dirty}}));
		}
		const auto case_coverage =
			verification_assertion_coverage(Json::make_array({suite.current}));
		suite.current.set("verdict", verification_report_verdict(
										 case_coverage, !suite.case_incomplete, !suite.dirty));
		if (!child->status.manifest_path.empty())
			suite.current.set("manifest", child->status.manifest_path.generic_string());
		suite.current.set("coverage", case_coverage);
		const auto report_limit =
			argument_integer(*suite.request.find("limits"), "maximum_report_bytes");
		if (write_json(suite.cases).size() + write_json(suite.current).size() + 2048 >
			report_limit) {
			suite.current.set("assertions", Json::make_array());
			suite.current.set("checkpoints", Json::make_array());
			suite.current.set("details_omitted", true);
			suite.current.set("detail_selector",
							  Json::make_object({{"capture_id", child->status.capture_id}}));
			std::erase_if(suite.current.object, [](const auto& field) {
				return field.first == "manifest" || field.first == "workload";
			});
		}
		suite.cases.array.emplace_back(std::move(suite.current));
		++suite.case_index;
		const bool failed = suite.case_failed || suite.case_incomplete;
		const auto failure_policy = argument_string(suite.request, "failure_policy");
		if (suite.case_incomplete && suite.execution_status == "completed")
			suite.execution_status = "failed";
		verification_dirty |= suite.dirty;
		if (suite.dirty || suite.canceled || suite.execution_status == "timeout" ||
			(failed && failure_policy == "stop_suite") ||
			suite.disk_bytes >
				argument_integer(*suite.request.find("limits"), "maximum_retained_bytes")) {
			seal_verification(suite);
			return;
		}
		suite.child_job.clear();
		suite.interaction_job.clear();
		suite.scenario_index = SIZE_MAX;
		suite.checkpoint_index = 0;
		suite.harvested_actions = 0;
		suite.cleanup_started = false;
		suite.case_failed = suite.case_incomplete = false;
		transition("preparing");
	}
}
