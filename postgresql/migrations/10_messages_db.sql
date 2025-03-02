DROP TABLE IF EXISTS ${SCHEMA}.messages;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.messages (
    chat_id TEXT,
    timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    sender_id TEXT,
    content TEXT,
    PRIMARY KEY (chat_id, timestamp),
    FOREIGN KEY (chat_id) REFERENCES ${SCHEMA}.messenger_chats (chat_id) ON DELETE CASCADE
);

CREATE INDEX idx_messages_chat_timestamp
  ON ${SCHEMA}.messages (chat_id ASC, timestamp DESC);