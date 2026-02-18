DROP TABLE IF EXISTS ${SCHEMA}.tracker_task_observers;
DROP TABLE IF EXISTS ${SCHEMA}.tracker_task_related_tasks;
DROP TABLE IF EXISTS ${SCHEMA}.tracker_tasks;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.tracker_tasks (
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
    FOREIGN KEY (action_id) REFERENCES ${SCHEMA}.actions (id) ON DELETE CASCADE,
    FOREIGN KEY (project_id) REFERENCES ${SCHEMA}.tracker_projects (project_id) ON DELETE CASCADE,
    FOREIGN KEY (creator) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE,
    FOREIGN KEY (assignee) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE
);

CREATE INDEX idx_tracker_tasks_creator
ON ${SCHEMA}.tracker_tasks (creator);

CREATE INDEX idx_tracker_tasks_assignee
ON ${SCHEMA}.tracker_tasks (assignee);

CREATE TABLE IF NOT EXISTS ${SCHEMA}.tracker_task_observers (
    task_id TEXT NOT NULL,
    employee_id TEXT NOT NULL,
    PRIMARY KEY (task_id, employee_id),
    FOREIGN KEY (task_id) REFERENCES ${SCHEMA}.tracker_tasks (task_id) ON DELETE CASCADE,
    FOREIGN KEY (employee_id) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE
);
CREATE INDEX idx_tracker_task_observers
  ON ${SCHEMA}.tracker_task_observers (employee_id);

CREATE TABLE IF NOT EXISTS ${SCHEMA}.tracker_task_related_tasks (
    task_id TEXT NOT NULL,
    task_id_related TEXT NOT NULL,
    PRIMARY KEY (task_id, task_id_related),
    FOREIGN KEY (task_id) REFERENCES ${SCHEMA}.tracker_tasks (task_id) ON DELETE CASCADE,
    FOREIGN KEY (task_id_related) REFERENCES ${SCHEMA}.tracker_tasks (task_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS ${SCHEMA}.task_documents (
    task_id TEXT NOT NULL,
    document_id TEXT NOT NULL,
    PRIMARY KEY (task_id, document_id),
    FOREIGN KEY (task_id) REFERENCES ${SCHEMA}.tracker_tasks (task_id) ON DELETE CASCADE,
    FOREIGN KEY (document_id) REFERENCES ${SCHEMA}.documents (id) ON DELETE CASCADE
);

CREATE INDEX idx_task_documents_task_id ON ${SCHEMA}.task_documents (task_id);
CREATE INDEX idx_task_documents_document_id ON ${SCHEMA}.task_documents (document_id);