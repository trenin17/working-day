CREATE TABLE IF NOT EXISTS ${SCHEMA}.reverse_index (
    key TEXT PRIMARY KEY,
    ids TEXT[],
    entity_type TEXT DEFAULT 'employees' CHECK (entity_type IN ('employees', 'tasks'))
);

CREATE INDEX trgm_idx ON ${SCHEMA}.reverse_index USING GIST (key gist_trgm_ops);
CREATE INDEX entity_type_idx ON ${SCHEMA}.reverse_index (entity_type);
