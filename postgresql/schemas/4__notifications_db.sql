DROP TABLE IF EXISTS working_day.notifications;

CREATE TABLE IF NOT EXISTS working_day.notifications (
    id TEXT PRIMARY KEY NOT NULL,
    kind TEXT NOT NULL,
    user_id TEXT NOT NULL,
    is_read BOOLEAN NOT NULL DEFAULT FALSE,
    created TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
