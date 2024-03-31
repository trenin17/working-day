DROP TABLE IF EXISTS working_day.documents;

CREATE TABLE IF NOT EXISTS working_day.documents (
    id TEXT PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    sign_required BOOLEAN NOT NULL,
    description TEXT
);

DROP TABLE IF EXISTS working_day.employee_document;

CREATE TABLE IF NOT EXISTS working_day.employee_document (
  employee_id TEXT NOT NULL,
  document_id TEXT NOT NULL,
  signed BOOLEAN NOT NULL DEFAULT FALSE,
  PRIMARY KEY (employee_id, document_id),
  FOREIGN KEY (employee_id) REFERENCES working_day.employees (id),
  FOREIGN KEY (document_id) REFERENCES working_day.documents (id)
);

ALTER TABLE working_day.documents
ADD COLUMN IF NOT EXISTS type TEXT NOT NULL DEFAULT 'admin_request';
