DROP TABLE IF EXISTS working_day.payments;

CREATE TABLE IF NOT EXISTS working_day.payments (
    id TEXT PRIMARY KEY NOT NULL,
    user_id TEXT NOT NULL,
    amount DOUBLE PRECISION NOT NULL,
    payroll_date TIMESTAMPTZ NOT NULL
);

CREATE INDEX idx_payments_by_user_id ON working_day.payments(user_id);
