DROP TABLE IF EXISTS working_day.auth_tokens;

CREATE TABLE IF NOT EXISTS working_day.auth_tokens (
    token TEXT PRIMARY KEY NOT NULL,
    user_id TEXT NOT NULL,
    scopes TEXT[] NOT NULL,
    updated TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
