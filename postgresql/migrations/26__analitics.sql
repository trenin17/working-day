-- -- Create user's event context table
-- CREATE TABLE IF NOT EXISTS working_day_first.request_cache (
--     id             BIGSERIAL PRIMARY KEY,

--     user_id        TEXT NOT NULL,
--     url            TEXT NOT NULL,
--     request_data   TEXT,
--     response_data  TEXT NOT NULL,
--     created_at     TIMESTAMPTZ NOT NULL DEFAULT NOW(),


--     -- FOREIGN KEY (user_id) REFERENCES working_day_first.employees(id) ON DELETE CASCADE
-- );

-- CREATE INDEX idx_request_cache_user_time ON working_day_first.request_cache (user_id, created_at DESC);