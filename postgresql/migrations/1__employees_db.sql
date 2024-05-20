CREATE SCHEMA IF NOT EXISTS ${SCHEMA};

CREATE TABLE IF NOT EXISTS ${SCHEMA}.employees (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    surname TEXT NOT NULL,
    patronymic TEXT,
    password TEXT,
    head_id TEXT,
    photo_link TEXT,
    phones TEXT[] NOT NULL DEFAULT ARRAY []::TEXT[],
    email TEXT,
    birthday TEXT,
    role TEXT DEFAULT 'user',
    position TEXT,
    telegram_id TEXT,
    vk_id TEXT,
    team TEXT
);

CREATE INDEX idx_employee_by_head ON ${SCHEMA}.employees(head_id);
