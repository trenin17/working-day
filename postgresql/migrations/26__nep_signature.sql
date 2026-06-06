-- Миграция для добавления системы усиленной неквалифицированной электронной подписи (НЭП)
-- Соответствует ФЗ-63 и ст. 22.3 ТК РФ

-- Таблица для хранения ключевых пар сотрудников
DROP TABLE IF EXISTS ${SCHEMA}.employee_keys CASCADE;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.employee_keys (
    employee_id TEXT PRIMARY KEY NOT NULL,
    private_key TEXT NOT NULL,           -- Приватный ключ RSA-2048 в формате PEM
    public_key TEXT NOT NULL,            -- Публичный ключ RSA-2048 в формате PEM
    public_key_hash TEXT NOT NULL,       -- SHA-256 хеш публичного ключа для быстрой идентификации
    created_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    FOREIGN KEY (employee_id) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE
);

CREATE INDEX idx_employee_keys_hash ON ${SCHEMA}.employee_keys(public_key_hash);

-- Таблица для хранения подписей документов
DROP TABLE IF EXISTS ${SCHEMA}.document_signatures CASCADE;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.document_signatures (
    id TEXT PRIMARY KEY NOT NULL,        -- UUID подписи
    document_id TEXT NOT NULL,           -- ID документа
    employee_id TEXT NOT NULL,           -- ID сотрудника, который подписал
    signature_path TEXT NOT NULL,        -- Путь к файлу отсоединенной подписи (.p7s)
    signature_metadata JSONB NOT NULL,   -- Метаданные подписи (timestamp, reason, location, etc)
    public_key_hash TEXT,       -- Хеш публичного ключа на момент подписания. Может быть null для КЭП.
    created_ts TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    FOREIGN KEY (document_id) REFERENCES ${SCHEMA}.documents (id) ON DELETE CASCADE,
    FOREIGN KEY (employee_id) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE
);

CREATE INDEX idx_document_signatures_doc ON ${SCHEMA}.document_signatures(document_id);
CREATE INDEX idx_document_signatures_emp ON ${SCHEMA}.document_signatures(employee_id);
CREATE INDEX idx_document_signatures_created ON ${SCHEMA}.document_signatures(created_ts DESC);
