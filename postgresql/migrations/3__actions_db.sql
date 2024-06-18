DROP TABLE IF EXISTS ${SCHEMA}.actions;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.actions (
    id TEXT PRIMARY KEY NOT NULL,
    type TEXT NOT NULL,
    user_id TEXT NOT NULL,
    start_date TIMESTAMPTZ NOT NULL,
    end_date TIMESTAMPTZ NOT NULL,
    status TEXT,
    underlying_action_id TEXT,
    blocking_actions_ids TEXT[] NOT NULL DEFAULT ARRAY []::TEXT[],
    FOREIGN KEY (user_id) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE
);

CREATE INDEX idx_actions_by_user_id ON ${SCHEMA}.actions(user_id);

CREATE INDEX idx_actions_by_user_id_start_date ON ${SCHEMA}.actions(user_id ASC, start_date ASC);

CREATE INDEX idx_actions_by_user_id_end_date ON ${SCHEMA}.actions(user_id ASC, end_date ASC);
