# Deployment

`docker-compose.yml` is the local backend stack (PostgreSQL, Redis, Auth and Lobby). Copy
`.env.example` to `.env`, replace every value, then run from the repository root:

```powershell
docker compose --env-file Deploy/.env -f Deploy/docker-compose.yml up --build
```

The dedicated Unreal GameServer is a Windows process, so it is intentionally not placed in the
Linux Compose stack. For a clustered environment, apply the Kubernetes manifests in this order:

1. `namespace-and-config.yaml`
2. a real Secret generated from `secret.example.yaml` (never apply or commit real values)
3. `auth-lobby.yaml`
4. `allocator-windows.yaml`

The Allocator deployment requires a Windows node with the packaged server mounted at
`C:\MahjongServer`. Its state and settlement outbox use persistent host directories. Replace image
tags with immutable digests in production. Auth and Lobby may scale horizontally because durable
identity/room state is in PostgreSQL and Lobby idempotency, presence and event fan-out are in Redis.

All Kubernetes readiness probes call dependency-aware `/health/ready`; liveness calls only
`/health/live`. Do not swap them: a dependency outage must remove a pod from service without
restarting a healthy process.
