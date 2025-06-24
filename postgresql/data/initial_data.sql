INSERT INTO working_day_first.employees(id, name, surname)
VALUES ('first_id', 'First', 'A'),
       ('second_id', 'Second', 'B'),
       ('stranger_id', 'Stranger', 'S')
ON CONFLICT (id)
DO NOTHING;

INSERT INTO wd_general.auth_tokens(token, user_id, company_id, scopes)
VALUES ('zero_token', 'first_id', 'zero', ARRAY ['user', 'superuser']),
       ('first_token', 'first_id', 'first', ARRAY ['user', 'admin']),
       ('second_token', 'second_id', 'first', ARRAY ['user'])
ON CONFLICT (token)
DO NOTHING;

INSERT INTO working_day_first.tracker_projects(project_name, tasks_count)
VALUES ('first', 2),
       ('second', 0)
ON CONFLICT (project_name)
DO NOTHING;

INSERT INTO working_day_first.tracker_tasks(id, title, description, project_name, creator, assignee, status, media_links, created_ts, deadline)
VALUES ('first-1', 'old task', 'description for old task', 'first', 'first_id', 'stranger_id', 'Open', ARRAY ['link 1', 'link 2'], '2025-04-12 10:00:00+00', '2025-04-12 10:00:00+00'),
       ('first-2', 'young task', 'description for young task', 'first', 'second_id', 'stranger_id', 'InProgress', ARRAY ['link'], '2025-04-12 10:00:00+00', '2025-04-12 10:00:00+00')

ON CONFLICT (id)
DO NOTHING;

-- INSERT INTO working_day_first.documents(id, name, sign_required, description)
-- VALUES ('first_document', 'First', TRUE, 'First document'),
--        ('second_document', 'Second', FALSE, 'Second document')

-- INSERT INTO working_day_first.employee_document(employee_id, document_id, signed)
-- VALUES ('first_id', 'first_document', TRUE),
--        ('second_id', 'second_document', FALSE)

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
