INSERT INTO working_day.employees(id, name, surname)
VALUES ('first_id', 'First', 'A'),
       ('second_id', 'Second', 'B')
ON CONFLICT (id)
DO NOTHING;

INSERT INTO working_day.auth_tokens(token, user_id, scopes)
VALUES ('first_token', 'first_id', ARRAY ['user', 'admin'])
ON CONFLICT (token)
DO NOTHING;
