ALTER TABLE ${SCHEMA}.documents DROP COLUMN chain_metadata;
DROP TYPE IF EXISTS wd_general.chain_metadata_item CASCADE;
