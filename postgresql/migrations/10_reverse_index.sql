CREATE TABLE IF NOT EXISTS working_day.reverse_index (
    key TEXT PRIMARY KEY,
    ids TEXT[]
);

CREATE EXTENSION pg_trgm;
CREATE INDEX trgm_idx ON working_day.reverse_index USING GIST (key gist_trgm_ops);
