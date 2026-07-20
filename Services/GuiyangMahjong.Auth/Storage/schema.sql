CREATE TABLE IF NOT EXISTS auth_identities (
    installation_hash CHAR(64) PRIMARY KEY,
    player_id VARCHAR(80) NOT NULL UNIQUE,
    display_name VARCHAR(24) NOT NULL,
    provider VARCHAR(32) NOT NULL,
    created_at_utc TIMESTAMPTZ NOT NULL,
    updated_at_utc TIMESTAMPTZ NOT NULL
);

CREATE TABLE IF NOT EXISTS auth_refresh_sessions (
    session_id CHAR(32) PRIMARY KEY,
    player_id VARCHAR(80) NOT NULL REFERENCES auth_identities(player_id) ON DELETE CASCADE,
    token_hash BYTEA NOT NULL,
    expires_at_utc TIMESTAMPTZ NOT NULL,
    created_at_utc TIMESTAMPTZ NOT NULL,
    revoked_at_utc TIMESTAMPTZ NULL
);

CREATE INDEX IF NOT EXISTS ix_auth_refresh_player_expiry
    ON auth_refresh_sessions(player_id, expires_at_utc DESC);
