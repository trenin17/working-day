CREATE TYPE wd_general.chain_metadata_item_new AS (
    employee_id TEXT,
    requires_signature INT,
    status INT
);

ALTER TABLE ${SCHEMA}.documents
ADD COLUMN chain_metadata_new wd_general.chain_metadata_item_new[] NOT NULL DEFAULT ARRAY[]::wd_general.chain_metadata_item_new[];

UPDATE ${SCHEMA}.documents
SET chain_metadata_new = (
    SELECT ARRAY(
        SELECT ROW(
            item.employee_id,
            CASE WHEN item.requires_signature THEN 1 ELSE 0 END,
            item.status
        )::wd_general.chain_metadata_item_new
        FROM unnest(chain_metadata) AS item
    )
);  
