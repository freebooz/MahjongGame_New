# Agones allocation integration execution plan

## Objective

Connect the existing Lobby/Allocator transaction and ticket flow to Agones without removing the
local-process backend used by Windows and WSL development.

## Executed phases

1. Add explicit `LocalProcess` and `Agones` allocator backend modes.
2. Create an in-cluster Kubernetes API client for `GameServerAllocation`, Fleet readiness, resource
   reconciliation, and GameServer deletion.
3. Feed the allocated address and dynamic host port into the existing persisted instance state
   machine, one-time registration, heartbeat, failure notification, and drain operations.
4. Add Allocation metadata containing room, match, server instance, registration, Lobby endpoint,
   and build context. The join-ticket signing key remains a Kubernetes Secret and is never written
   to GameServer metadata.
5. Watch Agones GameServer updates in UE. Keep admission closed while the server is merely `Ready`;
   initialize the managed bridge only after an `Allocated` update passes metadata validation.
6. Add namespace-scoped RBAC, Fleet environment injection, real Agones readiness, tests, and
   deployment documentation.

## Compatibility boundary

- Local and WSL deployments default to `Allocator__Backend=LocalProcess`.
- Kubernetes deployment selects `Allocator__Backend=Agones`.
- Lobby and client API contracts are unchanged.
- Redis/PostgreSQL room constraints and Lobby idempotency remain authoritative.

## Validation gates

- All .NET projects compile with warnings treated as errors.
- Auth, Lobby, and Allocator unit tests pass.
- Unreal Editor and dedicated Server targets compile.
- `GuiyangMahjong.GameServer` automation tests pass.
- All deployment YAML parses as multi-document YAML.
