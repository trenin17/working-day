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

ALTER TABLE working_day_first.reverse_index
ADD COLUMN entity_type TEXT DEFAULT 'employees' CHECK (entity_type IN ('employees', 'tasks'));

ALTER TABLE working_day_first.reverse_index
DROP CONSTRAINT reverse_index_pkey;

ALTER TABLE working_day_first.reverse_index
ADD CONSTRAINT reverse_index_key_entity_uniq UNIQUE (key, entity_type);

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

ALTER TABLE working_day_first.employees
ADD COLUMN job_position TEXT;

DO $$
BEGIN
    IF EXISTS (
        SELECT 1 FROM information_schema.columns
        WHERE table_schema = 'working_day_first'
        AND table_name = 'employees'
        AND column_name = 'position'
    ) THEN
        UPDATE working_day_first.employees
        SET job_position = position
        WHERE job_position IS NULL;
    END IF;
END $$;

ALTER TABLE working_day_first.employees DROP COLUMN IF EXISTS position;

DROP TABLE IF EXISTS working_day_first.messenger_chats;

CREATE TABLE IF NOT EXISTS working_day_first.messenger_chats (
    chat_id TEXT PRIMARY KEY,
    chat_name TEXT
);

DROP TABLE IF EXISTS working_day_first.messages;

CREATE TABLE IF NOT EXISTS working_day_first.messages (
    chat_id TEXT,
    timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    sender_id TEXT,
    content TEXT,
    PRIMARY KEY (chat_id, timestamp),
    FOREIGN KEY (chat_id) REFERENCES working_day_first.messenger_chats (chat_id) ON DELETE CASCADE
);

CREATE INDEX idx_messages_chat_timestamp
  ON working_day_first.messages (chat_id ASC, timestamp DESC);

DROP TABLE IF EXISTS working_day_first.employee_chats;

CREATE TABLE IF NOT EXISTS working_day_first.employee_chats (
  employee_id TEXT NOT NULL,
  chat_id TEXT,
  PRIMARY KEY (employee_id, chat_id),
  FOREIGN KEY (employee_id) REFERENCES working_day_first.employees (id) ON DELETE CASCADE,
  FOREIGN KEY (chat_id) REFERENCES working_day_first.messenger_chats (chat_id) ON DELETE CASCADE
);

DROP TABLE IF EXISTS working_day_first.tracker_project_assigned_users;
DROP TABLE IF EXISTS working_day_first.tracker_projects;
CREATE TABLE IF NOT EXISTS working_day_first.tracker_projects (
    project_id TEXT PRIMARY KEY NOT NULL,
    title TEXT NOT NULL,
    description TEXT,
    image_url TEXT,
    creator TEXT NOT NULL,
    tasks_count INT NOT NULL DEFAULT 0,
    status TEXT NOT NULL CHECK (status IN ('Open', 'Pause', 'Closed')) DEFAULT 'Open',
    created_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_updated_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    FOREIGN KEY (creator) REFERENCES working_day_first.employees (id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS working_day_first.tracker_project_assigned_users (
    project_id TEXT NOT NULL,
    employee_id TEXT NOT NULL,
    PRIMARY KEY (project_id, employee_id),
    FOREIGN KEY (project_id) REFERENCES working_day_first.tracker_projects (project_id) ON DELETE CASCADE,
    FOREIGN KEY (employee_id) REFERENCES working_day_first.employees (id) ON DELETE CASCADE
);
CREATE INDEX idx_tracker_project_assigned_users_employee
ON working_day_first.tracker_project_assigned_users (employee_id);

DROP TABLE IF EXISTS working_day_first.tracker_task_observers;
DROP TABLE IF EXISTS working_day_first.tracker_task_related_tasks;
DROP TABLE IF EXISTS working_day_first.tracker_tasks;
CREATE TABLE IF NOT EXISTS working_day_first.tracker_tasks (
    task_id TEXT PRIMARY KEY NOT NULL,
    title TEXT NOT NULL,
    description TEXT,
    project_id TEXT NOT NULL,
    creator TEXT NOT NULL,
    assignee TEXT,
    status TEXT NOT NULL CHECK (status IN ('Open', 'InProgress', 'Review', 'Done', 'Cancelled')),
    priority TEXT NOT NULL CHECK (priority IN ('Low', 'Middle', 'High')),
    media_links TEXT[] DEFAULT ARRAY[]::TEXT[],
    created_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_updated_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    deadline TIMESTAMPTZ,
    action_id TEXT,
    FOREIGN KEY (action_id) REFERENCES working_day_first.actions (id) ON DELETE CASCADE,
    FOREIGN KEY (project_id) REFERENCES working_day_first.tracker_projects (project_id) ON DELETE CASCADE,
    FOREIGN KEY (creator) REFERENCES working_day_first.employees (id) ON DELETE CASCADE,
    FOREIGN KEY (assignee) REFERENCES working_day_first.employees (id) ON DELETE CASCADE
);

CREATE INDEX idx_tracker_tasks_creator
ON working_day_first.tracker_tasks (creator);

CREATE INDEX idx_tracker_tasks_assignee
ON working_day_first.tracker_tasks (assignee);

CREATE TABLE IF NOT EXISTS working_day_first.tracker_task_observers (
    task_id TEXT NOT NULL,
    employee_id TEXT NOT NULL,
    PRIMARY KEY (task_id, employee_id),
    FOREIGN KEY (task_id) REFERENCES working_day_first.tracker_tasks (task_id) ON DELETE CASCADE,
    FOREIGN KEY (employee_id) REFERENCES working_day_first.employees (id) ON DELETE CASCADE
);
CREATE INDEX idx_tracker_task_observers
  ON working_day_first.tracker_task_observers (employee_id);

CREATE TABLE IF NOT EXISTS working_day_first.tracker_task_related_tasks (
    task_id TEXT NOT NULL,
    task_id_related TEXT NOT NULL,
    PRIMARY KEY (task_id, task_id_related),
    FOREIGN KEY (task_id) REFERENCES working_day_first.tracker_tasks (task_id) ON DELETE CASCADE,
    FOREIGN KEY (task_id_related) REFERENCES working_day_first.tracker_tasks (task_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS working_day_first.task_documents (
    task_id TEXT NOT NULL,
    document_id TEXT NOT NULL,
    PRIMARY KEY (task_id, document_id),
    FOREIGN KEY (task_id) REFERENCES working_day_first.tracker_tasks (task_id) ON DELETE CASCADE,
    FOREIGN KEY (document_id) REFERENCES working_day_first.documents (id) ON DELETE CASCADE
);

CREATE INDEX idx_task_documents_task_id ON working_day_first.task_documents (task_id);
CREATE INDEX idx_task_documents_document_id ON working_day_first.task_documents (document_id);

CREATE TYPE wd_general.chain_metadata_item AS (
    employee_id TEXT,
    requires_signature BOOLEAN,
    status INT
);

ALTER TABLE working_day_first.documents
ADD COLUMN chain_metadata wd_general.chain_metadata_item[] NOT NULL DEFAULT ARRAY[]::wd_general.chain_metadata_item[];

CREATE TYPE wd_general.chain_metadata_item_new AS (
    employee_id TEXT,
    requires_signature INT,
    status INT
);

ALTER TABLE working_day_first.documents
ADD COLUMN chain_metadata_new wd_general.chain_metadata_item_new[] NOT NULL DEFAULT ARRAY[]::wd_general.chain_metadata_item_new[];

UPDATE working_day_first.documents
SET chain_metadata_new = (
    SELECT ARRAY(
        SELECT ROW(
            item.employee_id,
            CASE WHEN item.requires_signature THEN 1 ELSE 0 END,
            item.status
        )::wd_general.chain_metadata_item_new
        FROM unnest(chain_metadata) AS item
    )
);

ALTER TABLE working_day_first.documents DROP COLUMN chain_metadata;
DROP TYPE IF EXISTS wd_general.chain_metadata_item CASCADE;

ALTER TABLE working_day_first.documents
ADD COLUMN IF NOT EXISTS visibility_status INT DEFAULT 0;

DROP TABLE IF EXISTS working_day_first.documents_history;

CREATE TABLE IF NOT EXISTS working_day_first.documents_history(
    document_id TEXT NOT NULL,
    actor_id TEXT NOT NULL,
    action_type TEXT NOT NULL,
    comment TEXT,
    created_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    FOREIGN KEY (document_id) REFERENCES working_day_first.documents (id) ON DELETE CASCADE,
    FOREIGN KEY (actor_id) REFERENCES working_day_first.employees (id) ON DELETE CASCADE,
    CHECK (action_type IN ('archived', 'restored'/*, 'signed', etc.*/)),
    PRIMARY KEY (document_id, created_ts)
);

DROP TABLE IF EXISTS working_day_first.employee_permissions;

CREATE TABLE IF NOT EXISTS working_day_first.employee_permissions (
    employee_id TEXT,
    permission_type TEXT NOT NULL,
    permission_value INT DEFAULT 0,
    FOREIGN KEY (employee_id) REFERENCES working_day_first.employees(id) ON DELETE CASCADE,
    CHECK (permission_type IN ('can_remove_documents'/*, etc.*/)),
    PRIMARY KEY (employee_id, permission_type)
);

ALTER TABLE working_day_first.actions
ADD COLUMN IF NOT EXISTS attendance_type TEXT;

ALTER TABLE wd_general.companies
ADD COLUMN IF NOT EXISTS head_template TEXT;

ALTER TABLE working_day_first.actions
ADD COLUMN IF NOT EXISTS document_id TEXT;

ALTER TABLE working_day_first.actions
ADD CONSTRAINT actions_document_id_fkey
FOREIGN KEY (document_id)
REFERENCES working_day_first.documents(id)
ON DELETE CASCADE;

ALTER TABLE working_day_first.reverse_index
DROP CONSTRAINT IF EXISTS reverse_index_entity_type_check;

ALTER TABLE working_day_first.reverse_index
ADD CONSTRAINT reverse_index_entity_type_check
CHECK (entity_type IN ('employees', 'tasks', 'projects'));

ALTER TABLE working_day_first.notifications
ADD COLUMN IF NOT EXISTS task_id TEXT;

ALTER TABLE working_day_first.notifications
ADD CONSTRAINT notifications_task_id_fkey
FOREIGN KEY (task_id)
REFERENCES working_day_first.tracker_tasks(task_id)
ON DELETE CASCADE;

CREATE INDEX idx_actions_user_id_end_date
ON working_day_first.actions (user_id, end_date);

-- Create comments table
CREATE TABLE IF NOT EXISTS working_day_first.comments (
    comment_id TEXT PRIMARY KEY NOT NULL,
    author_id TEXT NOT NULL,
    data TEXT NOT NULL,
    documents_ids TEXT[] DEFAULT ARRAY[]::TEXT[],
    created_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_updated_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    FOREIGN KEY (author_id) REFERENCES working_day_first.employees (id) ON DELETE CASCADE
);

CREATE INDEX idx_comments_author_id ON working_day_first.comments (author_id);
CREATE INDEX idx_comments_created_ts ON working_day_first.comments (created_ts);

-- Create task_comments table for linking tasks with comments
CREATE TABLE IF NOT EXISTS working_day_first.task_comments (
    task_id TEXT NOT NULL,
    comment_id TEXT NOT NULL,
    PRIMARY KEY (task_id, comment_id),
    FOREIGN KEY (task_id) REFERENCES working_day_first.tracker_tasks (task_id) ON DELETE CASCADE,
    FOREIGN KEY (comment_id) REFERENCES working_day_first.comments (comment_id) ON DELETE CASCADE
);

CREATE INDEX idx_task_comments_task_id ON working_day_first.task_comments (task_id);
CREATE INDEX idx_task_comments_comment_id ON working_day_first.task_comments (comment_id);

-- Таблица для хранения ключевых пар сотрудников
DROP TABLE IF EXISTS working_day_first.employee_keys CASCADE;

CREATE TABLE IF NOT EXISTS working_day_first.employee_keys (
    employee_id TEXT PRIMARY KEY NOT NULL,
    private_key TEXT NOT NULL,
    public_key TEXT NOT NULL,
    public_key_hash TEXT NOT NULL,
    created_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    FOREIGN KEY (employee_id) REFERENCES working_day_first.employees (id) ON DELETE CASCADE
);

CREATE INDEX idx_employee_keys_hash ON working_day_first.employee_keys(public_key_hash);

-- Таблица для хранения подписей документов
DROP TABLE IF EXISTS working_day_first.document_signatures CASCADE;

CREATE TABLE IF NOT EXISTS working_day_first.document_signatures (
    id TEXT PRIMARY KEY NOT NULL,
    document_id TEXT NOT NULL,
    employee_id TEXT NOT NULL,
    signature_path TEXT NOT NULL,
    signature_metadata JSONB NOT NULL,
    public_key_hash TEXT NOT NULL,
    created_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    FOREIGN KEY (document_id) REFERENCES working_day_first.documents (id) ON DELETE CASCADE,
    FOREIGN KEY (employee_id) REFERENCES working_day_first.employees (id) ON DELETE CASCADE
);

CREATE INDEX idx_document_signatures_doc ON working_day_first.document_signatures(document_id);
CREATE INDEX idx_document_signatures_emp ON working_day_first.document_signatures(employee_id);
CREATE INDEX idx_document_signatures_created ON working_day_first.document_signatures(created_ts DESC);
