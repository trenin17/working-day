DROP TABLE IF EXISTS wd_general.auth_tokens;

CREATE TABLE IF NOT EXISTS wd_general.auth_tokens (
    token TEXT PRIMARY KEY NOT NULL,
    user_id TEXT NOT NULL,
    company_id TEXT NOT NULL,
    scopes TEXT[] NOT NULL,
    updated TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
