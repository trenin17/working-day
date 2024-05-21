DROP TABLE IF EXISTS ${SCHEMA}.documents;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.documents (
    id TEXT PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    sign_required BOOLEAN NOT NULL,
    description TEXT,
    type TEXT NOT NULL DEFAULT 'admin_request'
);

DROP TABLE IF EXISTS ${SCHEMA}.employee_document;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.employee_document (
  employee_id TEXT NOT NULL,
  document_id TEXT NOT NULL,
  signed BOOLEAN NOT NULL DEFAULT FALSE,
  PRIMARY KEY (employee_id, document_id),
  FOREIGN KEY (employee_id) REFERENCES ${SCHEMA}.employees (id),
  FOREIGN KEY (document_id) REFERENCES ${SCHEMA}.documents (id)
);
