ALTER TABLE ${SCHEMA}.documents
ADD COLUMN IF NOT EXISTS author_id TEXT REFERENCES ${SCHEMA}.employees (id) ON DELETE SET NULL;

CREATE INDEX IF NOT EXISTS idx_documents_author_id ON ${SCHEMA}.documents (author_id);
