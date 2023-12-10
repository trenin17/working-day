ALTER TABLE working_day.employees
DROP COLUMN IF EXISTS phone,
ADD COLUMN IF NOT EXISTS phones TEXT[] NOT NULL DEFAULT ARRAY []::TEXT[],
ADD COLUMN IF NOT EXISTS telegram_id TEXT,
ADD COLUMN IF NOT EXISTS vk_id TEXT,
ADD COLUMN IF NOT EXISTS team TEXT;
