ALTER TABLE ${SCHEMA}.documents 
ADD COLUMN IF NOT EXISTS visibility_status INT DEFAULT 0;

DROP TABLE IF EXISTS ${SCHEMA}.archive_of_documents;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.archive_of_documents(
    id TEXT PRIMARY KEY,
    document_id TEXT NOT NULL,
    actor_id TEXT NOT NULL,
    action_type TEXT NOT NULL, -- 'archived', 'restored', 'signed'...
    comment TEXT,
    created_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    FOREIGN KEY (document_id) REFERENCES ${SCHEMA}.documents (id) ON DELETE CASCADE,
    FOREIGN KEY (actor_id) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_logs_document_time
ON ${SCHEMA}.archive_of_documents (document_id, created_ts DESC);