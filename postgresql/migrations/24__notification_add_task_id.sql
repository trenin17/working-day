ALTER TABLE ${SCHEMA}.notifications
ADD COLUMN IF NOT EXISTS task_id TEXT;

ALTER TABLE ${SCHEMA}.notifications
ADD CONSTRAINT notifications_task_id_fkey
FOREIGN KEY (task_id)
REFERENCES ${SCHEMA}.tracker_tasks(task_id)
ON DELETE CASCADE;
