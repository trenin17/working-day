-- public_key_hash is set for NEP; for KEP it is NULL
ALTER TABLE ${SCHEMA}.document_signatures
ALTER COLUMN public_key_hash DROP NOT NULL;
