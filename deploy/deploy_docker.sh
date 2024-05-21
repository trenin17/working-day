#!/bin/bash

# Указываем имя пользователя и имя репозитория
USERNAME="trenin17"
REPO="working_day"

# Получаем текущую последнюю версию
#LAST_VERSION=$(docker images --format "{{.Tag}}" $USERNAME/$REPO | ggrep -oP '\d+' | sort -nr | head -n1)
LAST_VERSION=34

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
