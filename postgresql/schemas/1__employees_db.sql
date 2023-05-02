DROP SCHEMA IF EXISTS working_day CASCADE;

CREATE SCHEMA IF NOT EXISTS working_day;

CREATE TABLE IF NOT EXISTS working_day.employees (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    surname TEXT NOT NULL,
    patronymic TEXT,
    password TEXT,
    head_id TEXT,
    photo_link TEXT
);

CREATE INDEX idx_employee_by_head ON working_day.employees(head_id);
