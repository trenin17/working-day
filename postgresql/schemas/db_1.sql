DROP SCHEMA IF EXISTS working_day CASCADE;

CREATE SCHEMA IF NOT EXISTS working_day;

DROP TABLE IF EXISTS working_day.auth_tokens;

CREATE TABLE IF NOT EXISTS working_day.auth_tokens (
    token TEXT PRIMARY KEY NOT NULL,
    user_id TEXT NOT NULL,
    scopes TEXT[] NOT NULL,
    updated TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

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

ALTER TABLE working_day.employees
ADD COLUMN IF NOT EXISTS company_id TEXT NOT NULL DEFAULT '1';

ALTER TABLE working_day.auth_tokens
ADD COLUMN IF NOT EXISTS company_id TEXT NOT NULL DEFAULT '1';

CREATE TABLE IF NOT EXISTS working_day.companies (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    ceo_id TEXT
);

ALTER TABLE working_day.actions
ADD COLUMN IF NOT EXISTS underlying_action_id TEXT,
ADD COLUMN IF NOT EXISTS blocking_actions_ids TEXT[] NOT NULL DEFAULT ARRAY []::TEXT[];

DROP TABLE IF EXISTS working_day.payments;

CREATE TABLE IF NOT EXISTS working_day.payments (
    id TEXT PRIMARY KEY NOT NULL,
    user_id TEXT NOT NULL,
    amount DOUBLE PRECISION NOT NULL,
    payroll_date TIMESTAMPTZ NOT NULL
);

CREATE INDEX idx_payments_by_user_id ON working_day.payments(user_id);

ALTER TABLE working_day.employees
DROP COLUMN IF EXISTS phone,
ADD COLUMN IF NOT EXISTS phones TEXT[] NOT NULL DEFAULT ARRAY []::TEXT[],
ADD COLUMN IF NOT EXISTS telegram_id TEXT,
ADD COLUMN IF NOT EXISTS vk_id TEXT,
ADD COLUMN IF NOT EXISTS team TEXT;

CREATE TABLE IF NOT EXISTS working_day.reverse_index (
    key TEXT PRIMARY KEY,
    ids TEXT[]
);

DROP TABLE IF EXISTS working_day.documents;

CREATE TABLE IF NOT EXISTS working_day.documents (
    id TEXT PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    sign_required BOOLEAN NOT NULL,
    description TEXT
);

DROP TABLE IF EXISTS working_day.employee_document;

CREATE TABLE IF NOT EXISTS working_day.employee_document (
  employee_id TEXT NOT NULL,
  document_id TEXT NOT NULL,
  signed BOOLEAN NOT NULL DEFAULT FALSE,
  PRIMARY KEY (employee_id, document_id),
  FOREIGN KEY (employee_id) REFERENCES working_day.employees (id),
  FOREIGN KEY (document_id) REFERENCES working_day.documents (id)
);

ALTER TABLE working_day.documents
ADD COLUMN IF NOT EXISTS type TEXT NOT NULL DEFAULT 'admin_request';
