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

CREATE SCHEMA IF NOT EXISTS working_day_first;

CREATE TABLE IF NOT EXISTS working_day_first.employees (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    surname TEXT NOT NULL,
    patronymic TEXT,
    password TEXT,
    head_id TEXT,
    photo_link TEXT,
    phones TEXT[] NOT NULL DEFAULT ARRAY []::TEXT[],
    email TEXT,
    birthday TEXT,
    role TEXT DEFAULT 'user',
    position TEXT,
    telegram_id TEXT,
    vk_id TEXT,
    team TEXT, -- deprecated TODO: remove
    subcompany TEXT NOT NULL DEFAULT 'first'
);

CREATE INDEX idx_employee_by_head ON working_day_first.employees(head_id);
DROP TABLE IF EXISTS working_day_first.notifications;

CREATE TABLE IF NOT EXISTS working_day_first.notifications (
    id TEXT PRIMARY KEY NOT NULL,
    type TEXT NOT NULL,
    text TEXT NOT NULL,
    user_id TEXT NOT NULL,
    is_read BOOLEAN NOT NULL DEFAULT FALSE,
    sender_id TEXT,
    action_id TEXT,
    created TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    FOREIGN KEY (user_id) REFERENCES working_day_first.employees (id) ON DELETE CASCADE
);

CREATE INDEX idx_notifications_by_user_id ON working_day_first.notifications(user_id);
DROP TABLE IF EXISTS working_day_first.actions;

CREATE TABLE IF NOT EXISTS working_day_first.actions (
    id TEXT PRIMARY KEY NOT NULL,
    type TEXT NOT NULL,
    user_id TEXT NOT NULL,
    start_date TIMESTAMPTZ NOT NULL,
    end_date TIMESTAMPTZ NOT NULL,
    status TEXT,
    underlying_action_id TEXT,
    blocking_actions_ids TEXT[] NOT NULL DEFAULT ARRAY []::TEXT[],
    FOREIGN KEY (user_id) REFERENCES working_day_first.employees (id) ON DELETE CASCADE
);

CREATE INDEX idx_actions_by_user_id ON working_day_first.actions(user_id);

CREATE INDEX idx_actions_by_user_id_start_date ON working_day_first.actions(user_id ASC, start_date ASC);

CREATE INDEX idx_actions_by_user_id_end_date ON working_day_first.actions(user_id ASC, end_date ASC);
DROP TABLE IF EXISTS working_day_first.payments;

CREATE TABLE IF NOT EXISTS working_day_first.payments (
    id TEXT PRIMARY KEY NOT NULL,
    user_id TEXT NOT NULL,
    amount DOUBLE PRECISION NOT NULL,
    payroll_date TIMESTAMPTZ NOT NULL,
    FOREIGN KEY (user_id) REFERENCES working_day_first.employees (id) ON DELETE CASCADE
);

CREATE INDEX idx_payments_by_user_id ON working_day_first.payments(user_id);
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
    description TEXT,
    type TEXT NOT NULL DEFAULT 'admin_request',
    parent_id TEXT NOT NULL DEFAULT '',
    created_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    FOREIGN KEY (parent_id) REFERENCES working_day_first.documents (id) ON DELETE CASCADE
);

CREATE INDEX idx_documents_by_parent_id ON working_day_first.documents(parent_id);

CREATE OR REPLACE FUNCTION set_parent_id()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.parent_id = '' THEN
        NEW.parent_id := NEW.id;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER before_insert_documents
BEFORE INSERT ON working_day_first.documents
FOR EACH ROW
EXECUTE FUNCTION set_parent_id();

DROP TABLE IF EXISTS working_day_first.employee_document;

CREATE TABLE IF NOT EXISTS working_day_first.employee_document (
  employee_id TEXT NOT NULL,
  document_id TEXT NOT NULL,
  signed BOOLEAN NOT NULL DEFAULT FALSE,
  updated_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  PRIMARY KEY (employee_id, document_id),
  FOREIGN KEY (employee_id) REFERENCES working_day_first.employees (id) ON DELETE CASCADE,
  FOREIGN KEY (document_id) REFERENCES working_day_first.documents (id) ON DELETE CASCADE
);

DROP TABLE IF EXISTS working_day_first.teams;

CREATE TABLE IF NOT EXISTS working_day_first.teams (
    id TEXT PRIMARY KEY NOT NULL,
    name TEXT NOT NULL
);

DROP TABLE IF EXISTS working_day_first.employee_team;

CREATE TABLE IF NOT EXISTS working_day_first.employee_team (
  employee_id TEXT NOT NULL,
  team_id TEXT NOT NULL,
  PRIMARY KEY (employee_id, team_id),
  FOREIGN KEY (employee_id) REFERENCES working_day_first.employees (id) ON DELETE CASCADE,
  FOREIGN KEY (team_id) REFERENCES working_day_first.teams (id) ON DELETE CASCADE
);

CREATE TYPE wd_general.inventory_item AS (
    name TEXT,
    description TEXT,
    id TEXT
);

ALTER TABLE working_day_first.employees
ADD COLUMN inventory wd_general.inventory_item[] NOT NULL DEFAULT ARRAY[]::wd_general.inventory_item[];
