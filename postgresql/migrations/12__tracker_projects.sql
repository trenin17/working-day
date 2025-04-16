DROP TABLE IF EXISTS ${SCHEMA}.tracker_projects;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.tracker_projects (
    project_name TEXT PRIMARY KEY,
    tasks_count INT NOT NULL DEFAULT 0
);
