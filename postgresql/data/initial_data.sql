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
  1,
  'Test document with approval chain',
  ARRAY[
    ('first_id', 1, 0)::wd_general.chain_metadata_item_new,
    ('second_id', 0, 0)::wd_general.chain_metadata_item_new
  ]
),(
  'rejected_doc',
  'Rejected document',
  1,
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
  0,
  'Document without approval chain',
  ARRAY[]::wd_general.chain_metadata_item_new[]
);

INSERT INTO working_day_first.employee_document(employee_id, document_id, signed)
VALUES
  ('first_id', 'doc_with_chain', FALSE),
  ('second_id', 'doc_with_chain', FALSE),
  ('first_id', 'rejected_doc', FALSE),
  ('second_id', 'rejected_doc', FALSE);

-- NEP-заглушка для first_id (миграции 26, 27): ключи и пароль для подписания НЭП
INSERT INTO working_day_first.employee_signature_passwords(employee_id, signature_password)
VALUES ('first_id', '123456')
ON CONFLICT (employee_id) DO NOTHING;

INSERT INTO working_day_first.employee_keys(employee_id, private_key, public_key, public_key_hash)
VALUES (
  'first_id',
  '-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIFLTBXBgkqhkiG9w0BBQ0wSjApBgkqhkiG9w0BBQwwHAQIa74+vCfedwkCAggA
MAwGCCqGSIb3DQIJBQAwHQYJYIZIAWUDBAEqBBDih9092A3AOlw0C0x3si1eBIIE
0KphB3Otjm9US02Xc51+AjmosYyS9oZlhRvo7bBydYCRdnyjvOr+Exc1AxI22sE2
9OBtM1JQkutryR+UEAiFhSMEYPF/1Bewycmh6cIdySqmZfrVH9Y/RsGjwv70Wr4H
0nLXVRCF3j6bKtkz3sijsC/U3OR0HV8oizbIoWm5hI67K8ooTV1CTG/07QVL0QZK
lvuerYJ4avn90ClIc3RDYmNmF8WinW0OZKnsbh9gM5hQStzfGxT7al5xPBzIbv+h
VHoWbPeLlzwkMTYfDD7+cepwhyoYMaGfkLz+URKnTzq2mh79BaanlkLarREI1HXU
WRL/FIzKhEC9fM2s8bRVaeJ83ByznDOmAYtR/YvHGz3GCewIp0/vLpFbk6j3IUHq
/l5zTQvp1VXubGzq0muABgzp3eXZcWt2JsycP7XjsnR4wqT5C8xlUnFWKpfuZgS7
8bGIFNhmwq91l8KkA53LI1SJzDdWq1mF5yo81yOVPUOggbShlHUpoqCh2RACV//T
n++UccHILWFk3YMQ+OAkmh8uiVivug1hGlOh3AAI4PuRdt73+8ChX1rQDVIZz4nK
PhWSflGFe4SryMfagpEWwHkPqEjODlwo6kg/RQ1K4UtHjQFH5nRFFttKA3/D4XVS
WCvIy+oNptYK8+oR86XeC9gLlAxESlge2hiBiZEjPdU4KOItS9FufgBUmcYZo3ku
bMq4shmCgNMbJ0DaWT3Y2ccMN71akUUe4wxgU7SZMZYOkelRbT5xveE4HDguxlj6
DcgDsaoHiYxlZjwXWES2dsTXjFkQsDyIlIcl4zMXS8eJLs5mzg+Zk2u8oG0qVBnC
3AfPPmy7SXIEmQp2Lmtale15aJpdCNEGlyCXPmAWBWmOdl3oapDzMdBdKbWTJQdf
ERrEhA2UcnitUzg1Y34oerngVhVg1e1416u/uHmWrueRYfWLZpUaeHR7qIpbI3K+
yjhehBNXpcB88R+ZxkEK9Lj3oeE8ab5y1ibF6v02XCROh8fQp7ttnhEpkRBKdLWM
rd0M7Vj1ldFpirMa7yO+rAG2Xpyc7MuVGG379YL5iODyCcKqx1+2URugBOui1PGo
GP+C9sVqUuaHOKIeKYGcQGYIB10IS4Do2vRkxW6S/sCW4mcZke1q46ersw3WYHfC
3c4DheAjY+HqMFSAwb7vETmYoEtp0GBhg8xNNplihaGOMykhW+PD20grpag5TzQL
P5MhaWchafmL4Jh/x7G8ClNexQeSnuIasLM7WbabL85jyydpslGMZgmznLdrYZ6O
/lltpW/6aZQS2rTBDU4ExnAgpfSdLP/P8yqWyn6MZ/WOjrmVnaLbkv6ltdofaJSg
Hs+2ii63RWAdprZ9oIO6FR+3gcvUo5A46ttOIvyfRQ60XSGcLXUfRqb4h3Ws+6bG
9nEkl/iev2Jp7yyzJnkw7Dn4CTos9NbDb87jjXorown7bYaLIBqi9VMC0ES9Aa/3
5vsvrOxBOgP8hCmxHFZw9BpzH6AUNjibzKrbt2PEusjLyszGfgThCXTBUfSLMTgE
VpE+kqVowVTFuI/ceBD+huA44oqFHf+2uspJNu2CTdvlxwCbxvEp/bdjY26Pp1kd
g6ntllSFMdGF4/QRUU/s5SUgKVudryqALsJ1PaKXcGpa
-----END ENCRYPTED PRIVATE KEY-----',
  '-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAmeu6EHktf2QDkbDXh9LQ
v8riTqJ8rhbSH3AYq6y8/myBOyroIFguDyIcWqWogGx9NI/u4D+d7PzQF1ydJgQY
o+jNC3deLEbNfAS6ypO68Do4NbenWAtHmH6NbxfWY5FfrHsOpMXsDCkPoPBd83Zn
7RZII89Ee+l+SQ7N4cpo6TlXRykMu9xu6uC/BtQhqvli8Uv0njJOGJq2+WbWOoNZ
84hntsFQBHzryTHU3vnc2E/k12/65//hhzi5U7ZH4lsPf2Bx/H73WJOv2/Dh30Pj
vhgNPI/4hJPMUzXkXiSGWWKhCk9Gblef+2UN72sxRmvQaHN1kkPzXLXqURsaf+IK
LwIDAQAB
-----END PUBLIC KEY-----',
  'd0bb458dbc6720d8a7062e234cc1c47a85c2ee80e137681fe93b346c60c82d33'
)
ON CONFLICT (employee_id) DO NOTHING;

INSERT INTO working_day_first.document_signatures(id, document_id, employee_id, signature_path, signature_metadata, signature_type)
VALUES (
  'sig_1',
  'doc_with_chain',
  'first_id',
  'doc_with_chain_first_id_kep.p7s',
  '{"user_name": "First A", "user_id": "first_id", "signature_path": "doc_with_chain_first_id_kep.p7s", "detached": true}',
  'kep'
);
INSERT INTO working_day_first.document_signatures(id, document_id, employee_id, signature_path, signature_metadata, signature_type)
VALUES (
  'sig_2',
  'empty_chain_doc',
  'first_id',
  'empty_chain_doc_first_id_nep.p7s',
  '{"timestamp": "2025-06-01T12:00:00Z", "user_name": "First A", "user_id": "first_id", "reason": "approval", "location": "Moscow"}',
  'nep'
);

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