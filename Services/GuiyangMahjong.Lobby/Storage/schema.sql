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

CREATE INDEX IF NOT EXISTS ix_lobby_rooms_player_ids
    ON lobby_rooms USING GIN ((payload->'playerIds'));

CREATE TABLE IF NOT EXISTS match_results (
    match_id VARCHAR(80) NOT NULL,
    result_sequence BIGINT NOT NULL,
    room_id VARCHAR(80) NOT NULL,
    payload JSONB NOT NULL,
    created_at_utc TIMESTAMPTZ NOT NULL,
    PRIMARY KEY (match_id, result_sequence)
);

CREATE INDEX IF NOT EXISTS ix_match_results_room
    ON match_results(room_id, created_at_utc DESC);
