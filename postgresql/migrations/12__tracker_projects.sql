DROP TABLE IF EXISTS ${SCHEMA}.tracker_project_assigned_users;
DROP TABLE IF EXISTS ${SCHEMA}.tracker_projects;
CREATE TABLE IF NOT EXISTS ${SCHEMA}.tracker_projects (
    project_id TEXT PRIMARY KEY NOT NULL,
    title TEXT NOT NULL,
    description TEXT,
    image_url TEXT,
    creator TEXT NOT NULL,
    tasks_count INT NOT NULL DEFAULT 0,
    status TEXT CHECK (status IN ('Open', 'Pause', 'Closed')) DEFAULT 'Open',
    created_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_updated_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    FOREIGN KEY (creator) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS ${SCHEMA}.tracker_project_assigned_users (
    project_id TEXT NOT NULL,
    employee_id TEXT NOT NULL,
    PRIMARY KEY (project_id, employee_id),
    FOREIGN KEY (project_id) REFERENCES ${SCHEMA}.tracker_projects (project_id) ON DELETE CASCADE,
    FOREIGN KEY (employee_id) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE
);
CREATE INDEX idx_tracker_project_assigned_users_employee
  ON ${SCHEMA}.tracker_project_assigned_users (employee_id);
