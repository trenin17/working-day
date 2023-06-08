DROP SCHEMA IF EXISTS working_day CASCADE;

CREATE SCHEMA IF NOT EXISTS working_day;

CREATE TABLE IF NOT EXISTS working_day.employees (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    surname TEXT NOT NULL,
    patronymic TEXT,
    password TEXT,
    head_id TEXT,
    photo_link TEXT
);

CREATE INDEX idx_employee_by_head ON working_day.employees(head_id);

ALTER TABLE working_day.employees
ADD COLUMN IF NOT EXISTS phone TEXT,
ADD COLUMN IF NOT EXISTS email TEXT,
ADD COLUMN IF NOT EXISTS birthday TEXT,
ADD COLUMN IF NOT EXISTS role TEXT DEFAULT 'user',
ADD COLUMN IF NOT EXISTS position TEXT;

DROP TABLE IF EXISTS working_day.auth_tokens;

CREATE TABLE IF NOT EXISTS working_day.auth_tokens (
    token TEXT PRIMARY KEY NOT NULL,
    user_id TEXT NOT NULL,
    scopes TEXT[] NOT NULL,
    updated TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

DROP TABLE IF EXISTS working_day.notifications;

CREATE TABLE IF NOT EXISTS working_day.notifications (
    id TEXT PRIMARY KEY NOT NULL,
    type TEXT NOT NULL,
    text TEXT NOT NULL,
    user_id TEXT NOT NULL,
    is_read BOOLEAN NOT NULL DEFAULT FALSE,
    sender_id TEXT,
    action_id TEXT,
    created TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_notifications_by_user_id ON working_day.notifications(user_id);

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
