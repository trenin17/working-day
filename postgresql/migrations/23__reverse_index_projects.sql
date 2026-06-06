ALTER TABLE ${SCHEMA}.reverse_index
DROP CONSTRAINT IF EXISTS reverse_index_entity_type_check;

ALTER TABLE ${SCHEMA}.reverse_index
ADD CONSTRAINT reverse_index_entity_type_check
CHECK (entity_type IN ('employees', 'tasks', 'projects'));

ALTER TABLE ${SCHEMA}.reverse_index
ADD CONSTRAINT reverse_index_key_entity_uniq
UNIQUE (key, entity_type);

