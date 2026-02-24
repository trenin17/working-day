-- sign_required: 0 - no signature, 1 - qualified (KEP), 2 - unqualified (NEP)
ALTER TABLE ${SCHEMA}.documents
ALTER COLUMN sign_required TYPE INT USING (CASE WHEN sign_required THEN 2 ELSE 0 END);

ALTER TABLE ${SCHEMA}.documents
ALTER COLUMN sign_required SET DEFAULT 0;
