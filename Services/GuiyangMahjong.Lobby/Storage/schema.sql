CREATE TABLE IF NOT EXISTS lobby_rooms (
    room_id VARCHAR(80) PRIMARY KEY,
    room_code CHAR(6) NOT NULL UNIQUE,
    lifecycle VARCHAR(24) NOT NULL,
    state_sequence BIGINT NOT NULL,
    payload JSONB NOT NULL,
    updated_at_utc TIMESTAMPTZ NOT NULL
);

CREATE INDEX IF NOT EXISTS ix_lobby_rooms_lifecycle_updated
    ON lobby_rooms(lifecycle, updated_at_utc DESC);

