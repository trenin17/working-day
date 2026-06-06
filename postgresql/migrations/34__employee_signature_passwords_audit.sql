CREATE TABLE IF NOT EXISTS ${SCHEMA}.employee_signature_passwords_audit (
    id BIGSERIAL PRIMARY KEY,
    occurred_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    operation TEXT NOT NULL CHECK (operation IN ('INSERT', 'UPDATE', 'DELETE')),
    employee_id TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_esp_audit_employee_occurred
    ON ${SCHEMA}.employee_signature_passwords_audit (employee_id, occurred_at DESC);

CREATE OR REPLACE FUNCTION ${SCHEMA}.log_employee_signature_passwords_change()
RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'DELETE' THEN
        INSERT INTO ${SCHEMA}.employee_signature_passwords_audit (operation, employee_id)
        VALUES ('DELETE', OLD.employee_id);
        RETURN OLD;
    ELSIF TG_OP = 'UPDATE' THEN
        INSERT INTO ${SCHEMA}.employee_signature_passwords_audit (operation, employee_id)
        VALUES ('UPDATE', NEW.employee_id);
        RETURN NEW;
    ELSIF TG_OP = 'INSERT' THEN
        INSERT INTO ${SCHEMA}.employee_signature_passwords_audit (operation, employee_id)
        VALUES ('INSERT', NEW.employee_id);
        RETURN NEW;
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_employee_signature_passwords_audit
    ON ${SCHEMA}.employee_signature_passwords;

CREATE TRIGGER trg_employee_signature_passwords_audit
    AFTER INSERT OR UPDATE OR DELETE ON ${SCHEMA}.employee_signature_passwords
    FOR EACH ROW
    EXECUTE FUNCTION ${SCHEMA}.log_employee_signature_passwords_change();
