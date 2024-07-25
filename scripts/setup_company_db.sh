#!/bin/bash

set -e

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <schema_name> <company_name> <db_address>"
    exit 1
fi

export SCHEMA=$1
export COMPANY_NAME=$2
SCRIPT_DIR=$(dirname "$(realpath "$BASH_SOURCE")")
MIGRATIONS_DIR=$(realpath "$SCRIPT_DIR/../postgresql/migrations")
MIGRATIONS=$(find "$MIGRATIONS_DIR" -name '*.sql' | sort)
cd $MIGRATIONS_DIR


cat $MIGRATIONS | sed "s/\${SCHEMA}/${SCHEMA}/g" | sed "s/\${COMPANY_NAME}/${COMPANY_NAME}/g" | psql $3
