ALTER TABLE ${SCHEMA}.notifications
ADD COLUMN IF NOT EXISTS document_id TEXT;

ALTER TABLE ${SCHEMA}.notifications
ADD CONSTRAINT notifications_document_id_fkey
FOREIGN KEY (document_id)
REFERENCES ${SCHEMA}.documents (id)
ON DELETE CASCADE;
