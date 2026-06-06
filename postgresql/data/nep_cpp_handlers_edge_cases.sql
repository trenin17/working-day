-- Сценарии для tests/test_nep_basic.py: ошибки pyservice, ответ verify «недействительна», битые метаданные

INSERT INTO working_day_first.documents(id, name, sign_required, description, chain_metadata_new)
VALUES (
  'doc_nep_pyservice_sign_500',
  'NEP mock sign pyservice 500',
  0,
  '',
  ARRAY[]::wd_general.chain_metadata_item_new[]
)
ON CONFLICT (id) DO NOTHING;

INSERT INTO working_day_first.documents(id, name, sign_required, description, chain_metadata_new)
VALUES (
  'doc_nep_verify_mocks',
  'NEP verify mock paths',
  0,
  '',
  ARRAY[]::wd_general.chain_metadata_item_new[]
)
ON CONFLICT (id) DO NOTHING;

INSERT INTO working_day_first.document_signatures (
  id, document_id, employee_id, signature_path, signature_metadata, public_key_hash, signature_type
)
VALUES
  (
    'sig_nep_verify_mock_invalid',
    'doc_nep_verify_mocks',
    'first_id',
    '__MOCK_VERIFY_RETURN_INVALID__',
    '{"user_name": "Mock User", "timestamp": "2024-01-01T00:00:00Z"}'::jsonb,
    NULL,
    'nep'
  ),
  (
    'sig_nep_verify_mock_http502',
    'doc_nep_verify_mocks',
    'first_id',
    '__MOCK_VERIFY_HTTP_502__',
    '{"user_name": "Mock User2", "timestamp": "2024-01-02T00:00:00Z"}'::jsonb,
    NULL,
    'nep'
  ),
  (
    'sig_nep_verify_bad_metadata',
    'doc_nep_verify_mocks',
    'first_id',
    'any.p7s',
    '[]'::jsonb,
    NULL,
    'nep'
  )
ON CONFLICT (id) DO NOTHING;
