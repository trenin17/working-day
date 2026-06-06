INSERT INTO working_day_first.document_signatures (
  id, document_id, employee_id, signature_path, signature_metadata, public_key_hash, signature_type
)
VALUES (
  'sig_nep_stranger',
  'empty_chain_doc',
  'stranger_id',
  'stranger.p7s',
  '{"timestamp": "2025-06-01T12:00:00Z", "user_name": "Stranger S", "user_id": "stranger_id"}'::jsonb,
  'deadbeef',
  'nep'
)
ON CONFLICT (id) DO NOTHING;
