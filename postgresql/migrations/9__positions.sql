ALTER TABLE ${SCHEMA}.employees
ADD COLUMN job_position TEXT;

ALTER TABLE ${SCHEMA}.employees DROP COLUMN IF EXISTS position;