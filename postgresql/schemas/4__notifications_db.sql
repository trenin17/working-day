DROP TABLE IF EXISTS working_day.notifications;

CREATE TABLE IF NOT EXISTS working_day.notifications (
    id TEXT PRIMARY KEY NOT NULL,
    type TEXT NOT NULL,
    text TEXT NOT NULL,
    user_id TEXT NOT NULL,
    is_read BOOLEAN NOT NULL DEFAULT FALSE,
    sender_id TEXT,
    created TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_notifications_by_user_id ON working_day.notifications(user_id);
