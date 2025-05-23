DO $$
BEGIN
    IF EXISTS (
        SELECT 1 FROM information_schema.columns 
        WHERE table_schema = '${SCHEMA}' 
        AND table_name = 'employees' 
        AND column_name = 'position'
    ) THEN
        UPDATE ${SCHEMA}.employees 
        SET job_position = position 
        WHERE job_position IS NULL;
    END IF;
END $$;