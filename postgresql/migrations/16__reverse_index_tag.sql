ALTER TABLE ${SCHEMA}.reverse_index
ADD COLUMN entity_type TEXT DEFAULT 'employees' CHECK (entity_type IN ('employees', 'tasks'));

ALTER TABLE ${SCHEMA}.reverse_index
DROP CONSTRAINT reverse_index_pkey;

ALTER TABLE ${SCHEMA}.reverse_index
ADD CONSTRAINT reverse_index_key_entity_uniq UNIQUE (key, entity_type);
