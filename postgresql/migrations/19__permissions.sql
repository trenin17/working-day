DROP TABLE IF EXISTS ${SCHEMA}.employee_permissions;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.employee_permissions (
    employee_id TEXT,
    permission_type TEXT NOT NULL,
    permission_value INT DEFAULT 0,
    FOREIGN KEY (employee_id) REFERENCES ${SCHEMA}.employees(id) ON DELETE CASCADE,
    CHECK (permission_type IN ('can_delete_documents' /*, etc.*/)),
    PRIMARY KEY (employee_id, permission_type)
);
