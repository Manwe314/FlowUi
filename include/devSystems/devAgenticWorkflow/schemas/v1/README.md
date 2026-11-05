# Agentic evidence schemas

These Draft 2020-12 schemas describe the version 1 wire contracts. All 64-bit IDs, byte counts, sequence values, and nanoseconds are decimal strings. The C++ codecs additionally check the unsigned 64-bit range, half-open interval order, duplicate IDs/paths, manifest inventory references, exact hashes and root confinement. Native timing CSV and diagnostics artifacts retain their separately declared schemas.

The source dictionary is a JSON array and the scenario declaration is a bare typed object; their manifest inventory supplies schema/version identity. Other evidence records carry their own envelope. Nullable fields and local `$defs` references are validated by the same runtime validator. Schema generation rejects unsupported validation keywords and external references.

- [Capture envelope](Capture.schema.json)
- [Resolved request](CaptureRequest.schema.json)
- [Quality](Quality.schema.json)
- [Frame identity and cadence](Frame.schema.json)
- [Correlation edges](Correlation.schema.json)
- [Image metadata](Image.schema.json)
- [Command response](CommandResult.schema.json)
- [External compiler diagnostics](BuildDiagnostics.schema.json)
- [Owned Flow/Clay tree](Tree.schema.json): topology, exact frame identity, geometry and pointer-presence flags; runtime checks also enforce index bounds.
- [Typed themes](Themes.schema.json): variants, reflected field constraints and authored/effective/preview distinctions.
- [Resource and element catalogue](Catalogue.schema.json): typed resource keys, owned metadata and revision identities.
- [Source dictionary](Sources.schema.json): bounded source locators retained under the capture inventory.
- [Scenario declaration](Scenario.schema.json) and [action/assertion records](Action.schema.json): declared workloads, phase progress and semantic action evidence.
- [Memory sources](MemorySources.schema.json), [events](MemoryEvent.schema.json) and [weighted summary](MemorySummary.schema.json): producer identities, exact quantities and interval-weighting semantics.
- [Performance summary](Performance.schema.json) and [timing contributors](TimingContributors.schema.json): continuous cadence segments, distributions and ranked exclusive scoped costs.
- [Comparison result](Comparison.schema.json) and [immutable comparison manifest](ComparisonManifest.schema.json): comparability, referenced captures and final-byte inventory.
- [Command request](CommandRequest.schema.json): common typed command envelope.
- [Verification request](VerificationRequest.schema.json): sequential scenario or interaction workloads, exact revisions, bounded deadlines and checkpoint budgets; unknown fields and unsupported versions are rejected.
- [Verification report](VerificationReport.schema.json): execution status, independent assertion verdict and coverage, cleanup, resource availability and durable evidence selectors.
- [Interaction plan](InteractionPlan.schema.json): finite source frames/spans or mechanical steps, coordinate/baseline/content policies, and optional deterministic compiler estimates.
- [Selected input receipt](InputFrame.schema.json): complete effective input, optional native assessment, actual consumed frame and explicit text-redaction/replayability facts.
- [Interaction observation](InteractionObservation.schema.json): genuine completed consumer facts, revisioned resolved geometry and optional application-owned target evidence.
- [Interaction result](InteractionResult.schema.json): independent delivery/observation/assertion coverage, cleanup/handoff, first divergence and optional exported-action receipt.
- [Interaction capabilities](InteractionCapabilities.schema.json): advertised mechanics, observation domains, key mapping and input-clock semantics.
- [Store identity](Store.schema.json), [run provenance](Run.schema.json), [session state](Session.schema.json), [instance endpoint](Instance.schema.json) and [rebuildable index](Index.schema.json): lifecycle and discovery metadata.
