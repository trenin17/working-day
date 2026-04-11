"""Структура zip при сборке выгрузки (без реального S3)."""
import io
import os
import shutil
import tempfile
import zipfile
from unittest.mock import patch

from build_archive.handler import (
    INNER_ARCHIVE_NAME,
    PEP_FOLDER_NAME,
    README_TEXT,
    _build_bundle_sync,
    _build_pep_pair_folder_sync,
)


@patch("build_archive.handler.upload_and_presign")
@patch("build_archive.handler.download_file")
def test_build_bundle_structure(mock_dl, mock_upload):
    calls = []

    def fake_download(key, path):
        calls.append((key, path))
        with open(path, "wb") as f:
            f.write(b"fake-bytes-for-" + key.encode())

    mock_dl.side_effect = fake_download

    persistent_outer = None

    def fake_upload(path, object_name):
        nonlocal persistent_outer
        fd, persistent_outer = tempfile.mkstemp(suffix=".zip")
        os.close(fd)
        shutil.copy2(path, persistent_outer)
        return "https://example/presigned"

    mock_upload.side_effect = fake_upload

    url = _build_bundle_sync(
        root_document_id="doc-root",
        original_display_name="Contract.pdf",
        stamped_document_id="doc-stamped",
        signatures=[
            {"path": "sig1.p7s", "type": "nep"},
            {"path": "sig2.sig", "type": "kep"},
        ],
    )
    assert url == "https://example/presigned"

    # Копия архива: после возврата хендлера временная директория уже удалена.
    outer_local = persistent_outer
    with zipfile.ZipFile(outer_local, "r") as z:
        names = set(z.namelist())
        assert "README.txt" in names
        assert "Contract_визуальная_копия.pdf" in names
        assert INNER_ARCHIVE_NAME in names
        assert z.read("README.txt").decode("utf-8") == README_TEXT

        inner_buf = io.BytesIO(z.read(INNER_ARCHIVE_NAME))
        with zipfile.ZipFile(inner_buf, "r") as zi:
            inner_names = zi.namelist()
            assert "Contract.pdf" in inner_names
            assert any(n.startswith("подписи/01_nep_") for n in inner_names)
            assert any(n.startswith("подписи/02_kep_") for n in inner_names)

    try:
        os.unlink(persistent_outer)
    except OSError:
        pass


@patch("build_archive.handler.upload_and_presign")
@patch("build_archive.handler.download_file")
def test_pep_pair_folder(mock_dl, mock_upload):
    def fake_download(key, path):
        with open(path, "wb") as f:
            f.write(b"x-" + key.encode())

    mock_dl.side_effect = fake_download

    persistent_outer = None

    def fake_upload(path, object_name):
        nonlocal persistent_outer
        fd, persistent_outer = tempfile.mkstemp(suffix=".zip")
        os.close(fd)
        shutil.copy2(path, persistent_outer)
        return "https://example/pep.zip"

    mock_upload.side_effect = fake_upload

    url = _build_pep_pair_folder_sync("root", "Report.pdf", "stamped-id")
    assert url == "https://example/pep.zip"

    outer_local = persistent_outer
    with zipfile.ZipFile(outer_local, "r") as z:
        names = z.namelist()
        assert f"{PEP_FOLDER_NAME}/Report.pdf" in names
        assert f"{PEP_FOLDER_NAME}/Report_визуальная_копия.pdf" in names

    try:
        os.unlink(persistent_outer)
    except OSError:
        pass
