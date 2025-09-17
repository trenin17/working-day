ALTER TABLE ${SCHEMA}.actions
ADD COLUMN IF NOT EXISTS document_id TEXT;

ALTER TABLE ${SCHEMA}.actions
ADD CONSTRAINT actions_document_id_fkey
FOREIGN KEY (document_id)
REFERENCES ${SCHEMA}.documents(id)
ON DELETE CASCADE;
