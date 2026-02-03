-- Create comments table
CREATE TABLE IF NOT EXISTS ${SCHEMA}.comments (
    comment_id TEXT PRIMARY KEY NOT NULL,
    author_id TEXT NOT NULL,
    data TEXT NOT NULL,
    documents_ids TEXT[] DEFAULT ARRAY[]::TEXT[],
    created_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_updated_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    FOREIGN KEY (author_id) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE
);

CREATE INDEX idx_comments_author_id ON ${SCHEMA}.comments (author_id);
CREATE INDEX idx_comments_created_ts ON ${SCHEMA}.comments (created_ts);

-- Create task_comments table for linking tasks with comments
CREATE TABLE IF NOT EXISTS ${SCHEMA}.task_comments (
    task_id TEXT NOT NULL,
    comment_id TEXT NOT NULL,
    PRIMARY KEY (task_id, comment_id),
    FOREIGN KEY (task_id) REFERENCES ${SCHEMA}.tracker_tasks (task_id) ON DELETE CASCADE,
    FOREIGN KEY (comment_id) REFERENCES ${SCHEMA}.comments (comment_id) ON DELETE CASCADE
);

CREATE INDEX idx_task_comments_task_id ON ${SCHEMA}.task_comments (task_id);
CREATE INDEX idx_task_comments_comment_id ON ${SCHEMA}.task_comments (comment_id);