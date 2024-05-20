DROP TABLE IF EXISTS ${SCHEMA}.payments;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.payments (
    id TEXT PRIMARY KEY NOT NULL,
    user_id TEXT NOT NULL,
    amount DOUBLE PRECISION NOT NULL,
    payroll_date TIMESTAMPTZ NOT NULL
);

CREATE INDEX idx_payments_by_user_id ON ${SCHEMA}.payments(user_id);
