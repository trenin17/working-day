import json
from Crypto.PublicKey import RSA
from Crypto.Cipher import PKCS1_OAEP
import base64
import requests

# Генерация ключей
def generate_keys():
    key = RSA.generate(2048)
    private_key = key.export_key()
    public_key = key.publickey().export_key()
    return public_key, private_key

# Шифрование
def encrypt(public_key, message):
    recipient_key = RSA.import_key(public_key)
    cipher_rsa = PKCS1_OAEP.new(recipient_key)
    encrypted_message = cipher_rsa.encrypt(message.encode('utf-8'))
    return base64.b64encode(encrypted_message).decode()

# Расшифрование
def decrypt(private_key, encrypted_message):
    key = RSA.import_key(private_key)
    cipher_rsa = PKCS1_OAEP.new(key)
    decrypted_message = cipher_rsa.decrypt(base64.b64decode(encrypted_message.encode()))
    return decrypted_message.decode('utf-8')

# Рекурсивная функция для шифрования значений JSON
def encrypt_json_values(public_key, obj):
    for key in obj:
        if isinstance(obj[key], dict):
            encrypt_json_values(public_key, obj[key])
        elif isinstance(obj[key], list):
            for i in range(len(obj[key])):
                if isinstance(obj[key][i], dict):
                    encrypt_json_values(public_key, obj[key][i])
                else:
                    obj[key][i] = encrypt(public_key, str(obj[key][i]))
        else:
            obj[key] = encrypt(public_key, str(obj[key]))

# Рекурсивная функция для расшифрования значений JSON
def decrypt_json_values(private_key, obj):
    for key in obj:
        if isinstance(obj[key], dict):
            decrypt_json_values(private_key, obj[key])
        elif isinstance(obj[key], list):
            for i in range(len(obj[key])):
                if isinstance(obj[key][i], dict):
                    decrypt_json_values(private_key, obj[key][i])
                else:
                    decrypted_value = decrypt(private_key, obj[key][i])
                    # Преобразуем обратно в исходный тип, если это число
                    if decrypted_value.isdigit():
                        obj[key][i] = int(decrypted_value)
                    else:
                        try:
                            obj[key][i] = float(decrypted_value)
                        except ValueError:
                            obj[key][i] = decrypted_value
        else:
            decrypted_value = decrypt(private_key, obj[key])
            # Преобразуем обратно в исходный тип, если это число
            if decrypted_value.isdigit():
                obj[key] = int(decrypted_value)
            else:
                try:
                    obj[key] = float(decrypted_value)
                except ValueError:
                    obj[key] = decrypted_value

# Функция для получения JSON с сервера
def fetch_json_from_server():
    url = 'http://localhost/InfoBase/hs/working_day/salaries'
    response = requests.get(url)
    
    if response.status_code == 200:
        return response.json()
    else:
        print(f"Failed to get data from server. Status code: {response.status_code}")
        return None

# Пример использования
if __name__ == "__main__":
    public_key, private_key = generate_keys()

    json_str = '{"id": "random_id", "user_id": "random_user_id", "amount": 1110.123, "user_name": "random_user_name"}'
    json_obj = json.loads(json_str)

    print("Original JSON:", json_obj)

    encrypt_json_values(public_key, json_obj)
    print("Encrypted JSON:", json_obj)

    decrypt_json_values(private_key, json_obj)
    print("Decrypted JSON:", json_obj)
