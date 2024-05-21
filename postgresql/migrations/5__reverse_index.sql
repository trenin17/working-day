CREATE TABLE IF NOT EXISTS ${SCHEMA}.reverse_index (
    key TEXT PRIMARY KEY,
    ids TEXT[]
);

CREATE INDEX trgm_idx ON ${SCHEMA}.reverse_index USING GIST (key gist_trgm_ops);
