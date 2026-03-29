# НЭП: модель безопасности и проверки

## Криптографическая стойкость (тест)

| Элемент | Реализация | Где в коде |
|--------|------------|------------|
| Асимметричная пара | RSA **2048** бит | `src/views/v1/employee/keys/generate/view.cpp` (`EVP_PKEY_CTX_set_rsa_keygen_bits`, 2048) |
| Отпечаток публичного ключа | **SHA-256** (hex 64 символа) | там же, `SHA256` |
| Экспорт приватного ключа в PEM | Шифрование пароля подписи: **AES-256-CBC** (внутри PEM) | `PEM_write_bio_PrivateKey(..., EVP_aes_256_cbc(), ...)` |
| Формат подписи документа | PKCS#7 (CMS), отсоединённая подпись `.p7s` | `python_service/nep_signer/signer.py` |

Это соответствует описанию в `NEP_SIGNATURE.md` (RSA-2048, SHA-256, PKCS#7).

## Аутентификация

Все публичные HTTP-ручки НЭП требуют **Bearer**-токен и scope **`user`**:

- `POST /v1/employee/keys/generate`
- `POST /v1/documents/nep-sign`
- `GET /v1/documents/nep-verify`

Настройка: `configs/static_config.yaml` (`auth.types: bearer`, `scopes: user`).

Идентификатор пользователя и компании для запросов берутся из контекста авторизации (`user_id`, `company_id`), а не из тела запроса — подпись выполняется от имени владельца сессии.

## Авторизация и границы доступа

- **Генерация ключей** — только для текущего пользователя (ключи не выдаются «на другого» через API).
- **Подписание** — используются ключи сотрудника из сессии; приватный ключ расшифровывается только при верном `signature_password` (иначе 401 в сценарии неверного пароля).
- **Проверка подписи** — по `signature_id` в рамках данных компании; тип подписи должен быть `nep`.

Поведение при ошибках (документ не найден, нет ключей, неверный пароль, ошибки pyservice/S3) покрыто автотестами в `tests/test_nep_basic.py` и юнит-тестами Python в `python_service/tests/`.

## Автоматизированное тестирование

| Область | Файлы |
|--------|--------|
| Генерация ключей, nep-sign, nep-verify (в т.ч. ошибки и моки pyservice) | `tests/test_nep_basic.py` |
| HTTP-хендлеры Python (sign/verify, коды ответов) | `python_service/tests/test_nep_handlers.py` |
| Криптография `NEPSigner` без HTTP/S3 | `python_service/tests/test_nep_signer.py` |

Перечень сценариев см. в комментариях в начале `tests/test_nep_basic.py` и `python_service/tests/test_nep_handlers.py`.


## Хранение ключей и пароля в БД

- **`employee_keys`** — закрытый ключ в PEM с шифрованием паролем подписи (AES-256-CBC внутри PEM); открытый ключ и `public_key_hash` (SHA-256). Миграция `postgresql/migrations/26__nep_signature.sql`.
- **`employee_signature_passwords`** — пароль подписи (6 цифр) в отдельной таблице, одна запись на сотрудника. Миграция `postgresql/migrations/27__employee_signature_password.sql`.
- **Аудит изменений пароля** — таблица `employee_signature_passwords_audit` и триггер `trg_employee_signature_passwords_audit` после `INSERT`/`UPDATE`/`DELETE` на `employee_signature_passwords` (в логе: тип операции, `employee_id`, время). Миграция `postgresql/migrations/34__employee_signature_passwords_audit.sql`.
- **Внешний аудит на уровне БД** — расширение **pgAudit** в PostgreSQL (журналирование DDL помимо триггеров в схеме компании). Параметры задаются в инфраструктуре кластера.
