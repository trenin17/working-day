DROP TABLE IF EXISTS working_day.actions;

CREATE TABLE IF NOT EXISTS working_day.actions (
    id TEXT PRIMARY KEY NOT NULL,
    type TEXT NOT NULL,
    user_id TEXT NOT NULL,
    start_date TIMESTAMPTZ NOT NULL,
    end_date TIMESTAMPTZ NOT NULL,
    status TEXT
);

CREATE INDEX idx_actions_by_user_id ON working_day.actions(user_id);

CREATE INDEX idx_actions_by_user_id_start_date ON working_day.actions(user_id ASC, start_date ASC);

CREATE INDEX idx_actions_by_user_id_end_date ON working_day.actions(user_id ASC, end_date ASC);
