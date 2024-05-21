#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <schema_name> <db_address>"
    exit 1
fi

export SCHEMA=$1
SCRIPT_DIR=$(dirname "$(realpath "$BASH_SOURCE")")
MIGRATIONS_DIR=$(realpath "$SCRIPT_DIR/../postgresql/migrations")
MIGRATIONS=$(find "$MIGRATIONS_DIR" -name '*.sql' | sort)
# MIGRATIONS="0_test_script.sql"
cd $MIGRATIONS_DIR


cat $MIGRATIONS | sed "s/\${SCHEMA}/${SCHEMA}/g" | psql $2
