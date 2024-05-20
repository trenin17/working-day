#!/bin/bash

# Указываем имя пользователя и имя репозитория
USERNAME="trenin17"
REPO="working_day"

# Получаем текущую последнюю версию
LAST_VERSION=$(sudo docker images --format "{{.Tag}}" $USERNAME/$REPO | grep -oP '\d+' | sort -nr | head -n1)

# Проверяем, что LAST_VERSION является числом
if ! [[ "$LAST_VERSION" =~ ^[0-9]+$ ]]; then
    echo "Невозможно извлечь последнюю версию образа"
    exit 1
fi

# Инкрементируем версию
NEW_VERSION=$((LAST_VERSION + 1))

# Собираем новый образ
sudo docker build -t $USERNAME/$REPO:v$NEW_VERSION .

# Отправляем новый образ в Docker Hub
sudo docker push $USERNAME/$REPO:v$NEW_VERSION

echo "Образ с версией v$NEW_VERSION успешно отправлен в Docker Hub."
