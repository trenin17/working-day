DROP TABLE IF EXISTS ${SCHEMA}.notifications;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.notifications (
    id TEXT PRIMARY KEY NOT NULL,
    type TEXT NOT NULL,
    text TEXT NOT NULL,
    user_id TEXT NOT NULL,
    is_read BOOLEAN NOT NULL DEFAULT FALSE,
    sender_id TEXT,
    action_id TEXT,
    created TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_notifications_by_user_id ON ${SCHEMA}.notifications(user_id);
