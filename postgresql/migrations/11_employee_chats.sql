DROP TABLE IF EXISTS ${SCHEMA}.employee_chats;

CREATE TABLE IF NOT EXISTS working_day_first.employee_chats (
  employee_id TEXT NOT NULL,
  chat_id TEXT,
  PRIMARY KEY (employee_id, chat_id),
  FOREIGN KEY (employee_id) REFERENCES working_day_first.employees (id) ON DELETE CASCADE
  FOREIGN KEY (chat_id) REFERENCES working_day_first.messenger_chats (chat_id) ON DELETE CASCADE
);