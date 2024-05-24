#!/bin/bash

# Указываем имя пользователя и имя репозитория
USERNAME="trenin17"
REPO="working_day"

# Получаем текущую последнюю версию
TOKEN=$(curl -s "https://auth.docker.io/token?service=registry.docker.io&scope=repository:trenin17/working_day:pull" | jq -r .token)
LAST_VERSION=$(curl -s -H "Authorization: Bearer $TOKEN" https://registry-1.docker.io/v2/trenin17/working_day/tags/list | jq -r '.tags[]' | sort -V | tail -n1 | sed 's/v//')

# Проверяем, что LAST_VERSION является числом
if ! [[ "$LAST_VERSION" =~ ^[0-9]+$ ]]; then
    echo "Невозможно извлечь последнюю версию образа"
    exit 1
fi

# Инкрементируем версию
NEW_VERSION=$((LAST_VERSION + 1))

echo "Начинается сборка образа с версией v$NEW_VERSION"

# Собираем новый образ
docker buildx build --platform linux/amd64  -t $USERNAME/$REPO:v$NEW_VERSION . --push

echo "Образ с версией v$NEW_VERSION успешно отправлен в Docker Hub."
