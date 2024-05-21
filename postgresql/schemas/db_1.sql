CREATE SCHEMA IF NOT EXISTS wd_general;

CREATE TABLE IF NOT EXISTS wd_general.companies (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    ceo_id TEXT
);

DROP TABLE IF EXISTS wd_general.auth_tokens;

CREATE TABLE IF NOT EXISTS wd_general.auth_tokens (
    token TEXT PRIMARY KEY NOT NULL,
    user_id TEXT NOT NULL,
    company_id TEXT NOT NULL,
    scopes TEXT[] NOT NULL,
    updated TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE EXTENSION pg_trgm;

DROP SCHEMA IF EXISTS working_day_first CASCADE;

CREATE SCHEMA IF NOT EXISTS working_day_first;

CREATE TABLE IF NOT EXISTS working_day_first.employees (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    surname TEXT NOT NULL,
    patronymic TEXT,
    password TEXT,
    head_id TEXT,
    photo_link TEXT
);

CREATE INDEX idx_employee_by_head ON working_day_first.employees(head_id);

ALTER TABLE working_day_first.employees
ADD COLUMN IF NOT EXISTS phone TEXT,
ADD COLUMN IF NOT EXISTS email TEXT,
ADD COLUMN IF NOT EXISTS birthday TEXT,
ADD COLUMN IF NOT EXISTS role TEXT DEFAULT 'user',
ADD COLUMN IF NOT EXISTS position TEXT;

DROP TABLE IF EXISTS working_day_first.notifications;

CREATE TABLE IF NOT EXISTS working_day_first.notifications (
    id TEXT PRIMARY KEY NOT NULL,
    type TEXT NOT NULL,
    text TEXT NOT NULL,
    user_id TEXT NOT NULL,
    is_read BOOLEAN NOT NULL DEFAULT FALSE,
    sender_id TEXT,
    action_id TEXT,
    created TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_notifications_by_user_id ON working_day_first.notifications(user_id);

DROP TABLE IF EXISTS working_day_first.actions;

CREATE TABLE IF NOT EXISTS working_day_first.actions (
    id TEXT PRIMARY KEY NOT NULL,
    type TEXT NOT NULL,
    user_id TEXT NOT NULL,
    start_date TIMESTAMPTZ NOT NULL,
    end_date TIMESTAMPTZ NOT NULL,
    status TEXT
);

CREATE INDEX idx_actions_by_user_id ON working_day_first.actions(user_id);

CREATE INDEX idx_actions_by_user_id_start_date ON working_day_first.actions(user_id ASC, start_date ASC);

CREATE INDEX idx_actions_by_user_id_end_date ON working_day_first.actions(user_id ASC, end_date ASC);

CREATE TABLE IF NOT EXISTS working_day_first.companies (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    ceo_id TEXT
);

ALTER TABLE working_day_first.actions
ADD COLUMN IF NOT EXISTS underlying_action_id TEXT,
ADD COLUMN IF NOT EXISTS blocking_actions_ids TEXT[] NOT NULL DEFAULT ARRAY []::TEXT[];

DROP TABLE IF EXISTS working_day_first.payments;

CREATE TABLE IF NOT EXISTS working_day_first.payments (
    id TEXT PRIMARY KEY NOT NULL,
    user_id TEXT NOT NULL,
    amount DOUBLE PRECISION NOT NULL,
    payroll_date TIMESTAMPTZ NOT NULL
);

CREATE INDEX idx_payments_by_user_id ON working_day_first.payments(user_id);

ALTER TABLE working_day_first.employees
DROP COLUMN IF EXISTS phone,
ADD COLUMN IF NOT EXISTS phones TEXT[] NOT NULL DEFAULT ARRAY []::TEXT[],
ADD COLUMN IF NOT EXISTS telegram_id TEXT,
ADD COLUMN IF NOT EXISTS vk_id TEXT,
ADD COLUMN IF NOT EXISTS team TEXT;

CREATE TABLE IF NOT EXISTS working_day_first.reverse_index (
    key TEXT PRIMARY KEY,
    ids TEXT[]
);

CREATE INDEX trgm_idx ON working_day_first.reverse_index USING GIST (key gist_trgm_ops);

DROP TABLE IF EXISTS working_day_first.documents;

CREATE TABLE IF NOT EXISTS working_day_first.documents (
    id TEXT PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    sign_required BOOLEAN NOT NULL,
    description TEXT
);

DROP TABLE IF EXISTS working_day_first.employee_document;

CREATE TABLE IF NOT EXISTS working_day_first.employee_document (
  employee_id TEXT NOT NULL,
  document_id TEXT NOT NULL,
  signed BOOLEAN NOT NULL DEFAULT FALSE,
  PRIMARY KEY (employee_id, document_id),
  FOREIGN KEY (employee_id) REFERENCES working_day_first.employees (id),
  FOREIGN KEY (document_id) REFERENCES working_day_first.documents (id)
);

ALTER TABLE working_day_first.documents
ADD COLUMN IF NOT EXISTS type TEXT NOT NULL DEFAULT 'admin_request';
