ALTER TABLE ${SCHEMA}.employee_permissions
DROP CONSTRAINT IF EXISTS employee_permissions_permission_type_check;

ALTER TABLE ${SCHEMA}.employee_permissions
ADD CONSTRAINT employee_permissions_permission_type_check
CHECK (permission_type IN ('can_remove_documents', 'can_upload_kep_signature'));
