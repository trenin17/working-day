DROP TABLE IF EXISTS ${SCHEMA}.employee_chats;

CREATE TABLE IF NOT EXISTS ${SCHEMA}.employee_chats (
  employee_id TEXT NOT NULL,
  chat_id TEXT,
  PRIMARY KEY (employee_id, chat_id),
  FOREIGN KEY (employee_id) REFERENCES ${SCHEMA}.employees (id) ON DELETE CASCADE,
  FOREIGN KEY (chat_id) REFERENCES ${SCHEMA}.messenger_chats (chat_id) ON DELETE CASCADE
);