ALTER TABLE ${SCHEMA}.documents
ADD COLUMN chain_metadata_new wd_general.chain_metadata_item[] NOT NULL DEFAULT ARRAY[]::wd_general.chain_metadata_item[];

UPDATE ${SCHEMA}.documents
SET chain_metadata_new = (
    SELECT ARRAY(
        SELECT ROW(
            item.employee_id,
            CASE WHEN item.requires_signature THEN 1 ELSE 0 END,
            item.status
        )::wd_general.chain_metadata_item
        FROM unnest(chain_metadata) AS item
    )
);

ALTER TABLE ${SCHEMA}.documents DROP COLUMN chain_metadata;
ALTER TABLE ${SCHEMA}.documents RENAME COLUMN chain_metadata_new TO chain_metadata;
DROP TYPE wd_general.chain_metadata_item_old;