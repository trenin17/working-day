ALTER TABLE ${SCHEMA}.employees
ADD COLUMN inventory wd_general.inventory_item[] NOT NULL DEFAULT ARRAY[]::wd_general.inventory_item[];
