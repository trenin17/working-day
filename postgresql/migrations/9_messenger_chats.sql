DROP TABLE IF EXISTS ${SCHEMA}.messenger_chats;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.messenger_chats (
    chat_id TEXT PRIMARY KEY,
    chat_name TEXT
);