DROP TABLE IF EXISTS ${SCHEMA}.documents;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.documents (
    id TEXT PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    sign_required BOOLEAN NOT NULL,
    description TEXT,
    type TEXT NOT NULL DEFAULT 'admin_request',
    parent_id TEXT NOT NULL DEFAULT '',
    FOREIGN KEY (parent_id) REFERENCES ${SCHEMA}.documents (id) ON DELETE CASCADE
);

CREATE INDEX idx_documents_by_parent_id ON ${SCHEMA}.documents(parent_id);

CREATE OR REPLACE FUNCTION set_parent_id()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.parent_id = '' THEN
        NEW.parent_id := NEW.id;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER before_insert_documents
BEFORE INSERT ON ${SCHEMA}.documents
FOR EACH ROW
EXECUTE FUNCTION set_parent_id();

DROP TABLE IF EXISTS ${SCHEMA}.employee_document;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.employee_document (
  employee_id TEXT NOT NULL,
  document_id TEXT NOT NULL,
  signed BOOLEAN NOT NULL DEFAULT FALSE,
  updated_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  PRIMARY KEY (employee_id, document_id),
  FOREIGN KEY (employee_id) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE,
  FOREIGN KEY (document_id) REFERENCES ${SCHEMA}.documents (id) ON DELETE CASCADE
);
