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