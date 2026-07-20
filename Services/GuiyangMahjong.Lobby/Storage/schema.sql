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

-- A player can own exactly one active-room lease. Every room membership mutation
-- updates this table in the same PostgreSQL transaction as the room snapshot.
CREATE TABLE IF NOT EXISTS active_player_rooms (
    player_id VARCHAR(80) PRIMARY KEY,
    room_id VARCHAR(80) NOT NULL REFERENCES lobby_rooms(room_id) ON DELETE CASCADE,
    match_id VARCHAR(80) NOT NULL,
    updated_at_utc TIMESTAMPTZ NOT NULL
);

CREATE INDEX IF NOT EXISTS ix_active_player_rooms_room
    ON active_player_rooms(room_id);

DELETE FROM active_player_rooms AS active
USING lobby_rooms AS room
WHERE active.room_id = room.room_id
  AND (room.lifecycle NOT IN ('Allocating', 'Waiting', 'Playing', 'Settling')
       OR NOT (room.payload->'playerIds' ? active.player_id));

-- Upgrade existing installations deterministically. If historical data contains
-- duplicate active memberships, the most recently updated room owns the lease.
INSERT INTO active_player_rooms(player_id, room_id, match_id, updated_at_utc)
SELECT DISTINCT ON (player.value)
       player.value, room.room_id, room.payload->>'matchId', room.updated_at_utc
FROM lobby_rooms AS room
CROSS JOIN LATERAL jsonb_array_elements_text(room.payload->'playerIds') AS player(value)
WHERE room.lifecycle IN ('Allocating', 'Waiting', 'Playing', 'Settling')
  AND COALESCE(room.payload->>'matchId', '') <> ''
ORDER BY player.value, room.updated_at_utc DESC
ON CONFLICT (player_id) DO NOTHING;

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
