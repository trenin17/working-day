ALTER TABLE working_day.employees
ADD COLUMN IF NOT EXISTS company_id TEXT NOT NULL DEFAULT '1';

ALTER TABLE working_day.auth_tokens
ADD COLUMN IF NOT EXISTS company_id TEXT NOT NULL DEFAULT '1';

CREATE TABLE IF NOT EXISTS working_day.companies (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    ceo_id TEXT
);
