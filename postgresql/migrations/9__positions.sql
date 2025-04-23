ALTER TABLE ${SCHEMA}.employees
ADD COLUMN job_position TEXT;

UPDATE ${SCHEMA}.employees 
SET job_position = position 
WHERE job_position IS NULL AND position IS NOT NULL;

ALTER TABLE ${SCHEMA}.employees DROP COLUMN IF EXISTS position;