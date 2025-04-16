DROP TABLE IF EXISTS ${SCHEMA}.tracker_tasks;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.tracker_tasks (
    id TEXT PRIMARY KEY NOT NULL,
    title TEXT NOT NULL,
    description TEXT,
    project_name TEXT NOT NULL,
    creator TEXT NOT NULL,
    assignee TEXT,
    status TEXT CHECK (status IN ('Open', 'InProgress', 'Review', 'Done')),
    media_links TEXT[] DEFAULT ARRAY[]::TEXT[],
    created_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    deadline TIMESTAMPTZ,
    FOREIGN KEY (creator) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE,
    FOREIGN KEY (assignee) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE
);
