#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <schema_name>"
    exit 1
fi

export SCHEMA=$1
SCRIPT_DIR=$(dirname "$(realpath "$BASH_SOURCE")")
MIGRATIONS_DIR=$(realpath "$SCRIPT_DIR/../postgresql/migrations")
MIGRATIONS=$(find "$MIGRATIONS_DIR" -name '*.sql' | sort)

cat $MIGRATIONS | envsubst | psql "host=rc1b-dk3v16aam2cveh01.mdb.yandexcloud.net port=6432 user=trenin17 password=trenin17 dbname=employees sslmode=verify-full"
