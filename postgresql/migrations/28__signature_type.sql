ALTER TABLE ${SCHEMA}.document_signatures
ADD COLUMN signature_type TEXT NOT NULL DEFAULT 'nep' CHECK (signature_type IN ('nep', 'kep'));
