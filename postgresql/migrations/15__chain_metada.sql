ALTER TABLE ${SCHEMA}.documents
ADD COLUMN chain_metadata wd_general.chain_metadata_item[] NOT NULL DEFAULT ARRAY[]::wd_general.chain_metadata_item[];
