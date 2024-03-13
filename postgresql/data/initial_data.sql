INSERT INTO working_day.employees(id, name, surname)
VALUES ('first_id', 'First', 'A'),
       ('second_id', 'Second', 'B')
ON CONFLICT (id)
DO NOTHING;


INSERT INTO working_day.auth_tokens(token, user_id, scopes, company_id)
VALUES ('first_token', 'first_id', ARRAY ['user', 'admin'], '1'),
       ('second_token', 'second_id', ARRAY ['user'], '1')
ON CONFLICT (token)
DO NOTHING;

-- INSERT INTO working_day.documents(id, name, sign_required, description)
-- VALUES ('first_document', 'First', TRUE, 'First document'),
--        ('second_document', 'Second', FALSE, 'Second document')
    
-- INSERT INTO working_day.employee_document(employee_id, document_id, signed)
-- VALUES ('first_id', 'first_document', TRUE),
--        ('second_id', 'second_document', FALSE)
