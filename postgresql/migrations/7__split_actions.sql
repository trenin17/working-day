ALTER TABLE working_day.actions
ADD COLUMN IF NOT EXISTS underlying_action_id TEXT,
ADD COLUMN IF NOT EXISTS blocking_actions_ids TEXT[] NOT NULL DEFAULT ARRAY []::TEXT[];
