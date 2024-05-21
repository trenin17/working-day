CREATE SCHEMA IF NOT EXISTS wd_general;

CREATE TABLE IF NOT EXISTS wd_general.companies (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    ceo_id TEXT
);
