#!/usr/bin/env python3
"""Загрузка локального PDF в S3 (ключ = object key). Учётные данные — стандартные переменные AWS / boto3."""
from __future__ import annotations

import argparse
import os
import sys

import boto3


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument('local_path')
    p.add_argument('bucket')
    p.add_argument('object_key')
    args = p.parse_args()

    endpoint = os.environ.get('AWS_ENDPOINT_URL') or os.environ.get('NEP_BENCH_S3_ENDPOINT')
    region = os.environ.get('AWS_DEFAULT_REGION', 'ru-central1')
    kwargs = {'region_name': region}
    if endpoint:
        kwargs['endpoint_url'] = endpoint

    client = boto3.client('s3', **kwargs)
    client.upload_file(args.local_path, args.bucket, args.object_key)
    print(f"uploaded s3://{args.bucket}/{args.object_key}", file=sys.stderr)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
