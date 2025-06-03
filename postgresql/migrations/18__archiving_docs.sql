ALTER TABLE ${SCHEMA}.documents
ADD COLUMN IF NOT EXISTS visibility_status INT DEFAULT 0;

DROP TABLE IF EXISTS ${SCHEMA}.documents_history;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.documents_history(
    document_id TEXT NOT NULL,
    actor_id TEXT NOT NULL,
    action_type TEXT NOT NULL,
    comment TEXT,
    created_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    FOREIGN KEY (document_id) REFERENCES ${SCHEMA}.documents (id) ON DELETE CASCADE,
    FOREIGN KEY (actor_id) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE,
    CHECK (action_type IN ('archived', 'restored'/*, 'signed' , etc.*/)),
    PRIMARY KEY (document_id, created_ts)
);
