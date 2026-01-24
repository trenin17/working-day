INSERT INTO working_day_first.employees(id, name, surname)
VALUES ('first_id', 'First', 'A'),
       ('second_id', 'Second', 'B'),
       ('third_id', 'Third', 'C'),
       ('stranger_id', 'Stranger', 'S')
ON CONFLICT (id)
DO NOTHING;

INSERT INTO wd_general.auth_tokens(token, user_id, company_id, scopes)
VALUES ('zero_token', 'first_id', 'zero', ARRAY ['user', 'superuser']),
       ('first_token', 'first_id', 'first', ARRAY ['user', 'admin']),
       ('second_token', 'second_id', 'first', ARRAY ['user']),
       ('third_token', 'third_id', 'first', ARRAY['user'])
ON CONFLICT (token)
DO NOTHING;

INSERT INTO working_day_first.tracker_projects(project_id, title, creator, tasks_count, created_ts, last_updated_ts)
VALUES ('FIRST', 'first project name', 'first_id', 2, '2025-04-12 11:00:00+00', '2025-04-12 11:00:00+00'),
       ('SECOND', 'second project name', 'second_id', 0, '2025-04-12 12:00:00+00', '2025-04-12 12:00:00+00')
ON CONFLICT (project_id)
DO NOTHING;

INSERT INTO working_day_first.tracker_tasks(task_id, title, description, project_id, creator, assignee, status, priority, media_links, created_ts, deadline)
VALUES ('FIRST-1', 'old task', 'description for old task', 'FIRST', 'first_id', 'stranger_id', 'Open', 'Low', ARRAY ['link 1', 'link 2'], '2025-04-12 10:00:00+00', '2025-04-12 10:00:00+00'),
       ('FIRST-2', 'young task', 'description for young task', 'FIRST', 'second_id', 'stranger_id', 'InProgress', 'Middle',  ARRAY ['link'], '2025-04-12 10:00:00+00', '2025-04-12 10:00:00+00')
ON CONFLICT (task_id)
DO NOTHING;

INSERT INTO working_day_first.documents(id, name, sign_required, description, chain_metadata_new)
VALUES (
  'doc_with_chain',
  'Document with chain',
  TRUE,
  'Test document with approval chain',
  ARRAY[
    ('first_id', 1, 0)::wd_general.chain_metadata_item_new,
    ('second_id', 0, 0)::wd_general.chain_metadata_item_new
  ]
),(
  'rejected_doc',
  'Rejected document',
  TRUE,
  '',
  ARRAY[
    ('first_id', 1, 2)::wd_general.chain_metadata_item_new,
    ('second_id', 0, 0)::wd_general.chain_metadata_item_new
  ]
);

INSERT INTO working_day_first.documents(id, name, sign_required, description, chain_metadata_new)
VALUES (
  'empty_chain_doc',
  'Empty chain doc',
  FALSE,
  'Document without approval chain',
  ARRAY[]::wd_general.chain_metadata_item_new[]
);

INSERT INTO working_day_first.employee_document(employee_id, document_id, signed)
VALUES
  ('first_id', 'doc_with_chain', FALSE),
  ('second_id', 'doc_with_chain', FALSE),
  ('first_id', 'rejected_doc', FALSE),
  ('second_id', 'rejected_doc', FALSE);


INSERT INTO working_day_first.teams(id, name) VALUES ('default_team', 'Default team'), ('stranger_team', 'Stranger team');
INSERT INTO working_day_first.employee_team(employee_id, team_id) VALUES ('first_id', 'default_team'), ('second_id', 'default_team'), ('stranger_id', 'stranger_team');

INSERT INTO working_day_first.messenger_chats (chat_id, chat_name)
VALUES
  ('chat1', 'Test Chat');

INSERT INTO working_day_first.messages (chat_id, timestamp, sender_id, content)
VALUES
  ('chat1', '2025-02-25 10:00:00+00', 'user1', 'Hello world!');

INSERT INTO working_day_first.employee_permissions(employee_id, permission_type, permission_value)
VALUES
  ('first_id', 'can_remove_documents', 1),
  ('second_id', 'can_remove_documents', 0)
ON CONFLICT (employee_id, permission_type)
DO UPDATE SET permission_value = EXCLUDED.permission_value;

INSERT INTO working_day_first.comments(comment_id, author_id, data)
VALUES
  ('comment1', 'first_id', 'test comment');