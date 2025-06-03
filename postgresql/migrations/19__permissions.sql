DROP TABLE IF EXISTS ${SCHEMA}.employee_permissions;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.employee_permissions (
    employee_id TEXT PRIMARY KEY,
    can_delete INT DEFAULT 0,
    FOREIGN KEY (employee_id) REFERENCES ${SCHEMA}.employees(id) ON DELETE CASCADE
);
