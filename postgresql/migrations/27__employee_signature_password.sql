DROP TABLE IF EXISTS ${SCHEMA}.employee_signature_passwords CASCADE;

CREATE TABLE ${SCHEMA}.employee_signature_passwords (
    employee_id TEXT PRIMARY KEY NOT NULL,
    signature_password CHAR(6) NOT NULL CHECK (signature_password ~ '^\d{6}$'),
    created_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    FOREIGN KEY (employee_id) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE
);
