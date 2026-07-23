# Guiyang Mahjong Agones server-layout implementation workflow

## Objective

Adopt the responsibilities represented by the reference `MahjongGame` layout without renaming the
existing `GuiyangMahjong` modules, reflected classes, maps, or cooked asset paths.

## Architecture decision

- `GuiyangMahjongServer.Target.cs` remains the dedicated-server target.
- `MahjongNetMap` is the authoritative target-neutral replicated room map shared by client and
  dedicated server. It serializes neither client-only nor server-only classes. The server injects
  `GuiyangMahjongGameMode` in its launch URL, while the client creates presentation locally after
  joining.
- The existing durable Allocator remains the local/WSL orchestration backend.
- Agones is an optional production lifecycle backend enabled with
  `-MahjongOrchestrator=Agones`; it must not become active accidentally on local servers.
- Join-ticket verification remains authoritative inside the UE dedicated server and is split from
  Lobby registration/heartbeat/result delivery.

## Phases and dependencies

1. **Targets and configuration**
   - Add `Config/DefaultServer.ini` with headless server and Agones SDK settings.
   - Add explicit `GuiyangMahjongClient.Target.cs` while preserving the existing Game target.
2. **Security responsibility split**
   - Move claims and one-time HMAC join-ticket validation into
     `Server/GuiyangServerTicketVerifier.{h,cpp}`.
   - Keep `GuiyangGameServerBridge.h` as the compatibility include for existing callers/tests.
3. **Agones lifecycle**
   - Add a dedicated-server-only GameInstance subsystem.
   - Bind before connecting, rely on the SDK sidecar for Ready/Health, set capacity to four, track
     connected players, and request graceful Shutdown during world teardown.
   - Activate only with the explicit orchestrator flag or `MAHJONG_ORCHESTRATOR=agones`.
4. **Deployment artifacts**
   - Add a non-root Linux runtime image consuming `Artifacts/LinuxServer`.
   - Add Fleet, FleetAutoscaler, and GameServerAllocation examples under `Deploy/Agones`.
   - Keep secrets out of manifests; inject ticket and registration credentials from Kubernetes
     Secrets.
5. **Quality gates**
   - Editor target compiles.
   - Existing join-ticket replay/scope/expiry tests continue passing.
   - New lifecycle activation tests cover explicit opt-in and safe default-off behavior.
   - YAML parses, Dockerfile references the packaged executable, and `git diff --check` passes.

## Rollback boundaries

The Agones subsystem is additive and flag-gated. Removing the flag restores the current Allocator
behavior without changing room state, Lobby contracts, or client travel URLs.
