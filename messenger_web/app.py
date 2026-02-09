#!/usr/bin/env python3
"""
Messenger Web UI — Flask proxy to the C++ working_day service.

Usage:
    python3 messenger_web/app.py

Access at http://localhost:5000
The C++ service must be running on localhost:8080.
"""

import json
import requests
from flask import Flask, render_template, request, jsonify

app = Flask(__name__)

SERVICE_URL = "http://localhost:8080"

# Known test users
USERS = {
    "first_id": {"name": "First A", "token": "first_token", "role": "admin"},
    "second_id": {"name": "Second B", "token": "second_token", "role": "user"},
    "stranger_id": {"name": "Stranger S", "token": "stranger_token", "role": "viewer"},
}


@app.route("/")
def index():
    return render_template("index.html", users=USERS, service_ws_url="ws://localhost:8080/chat")


@app.route("/api/users")
def api_users():
    return jsonify(USERS)


@app.route("/api/chats")
def api_chats():
    user_id = request.args.get("user_id")
    token = request.args.get("token")
    if not user_id or not token:
        return jsonify({"error": "user_id and token required"}), 400

    try:
        resp = requests.post(
            f"{SERVICE_URL}/v1/messenger/list-chats",
            params={"employee_id": user_id},
            headers={"Authorization": f"Bearer {token}"},
            timeout=5,
        )
        return jsonify(resp.json()), resp.status_code
    except requests.ConnectionError:
        return jsonify({"error": "Cannot connect to service"}), 502


@app.route("/api/chats/create", methods=["POST"])
def api_create_chat():
    token = request.json.get("token")
    chat_name = request.json.get("chat_name")
    id_list = request.json.get("id_list", [])
    if not token or not chat_name or not id_list:
        return jsonify({"error": "token, chat_name, id_list required"}), 400

    try:
        resp = requests.post(
            f"{SERVICE_URL}/v1/messenger/create-chat",
            json={"chat_name": chat_name, "id_list": id_list},
            headers={"Authorization": f"Bearer {token}"},
            timeout=5,
        )
        return jsonify(resp.json()), resp.status_code
    except requests.ConnectionError:
        return jsonify({"error": "Cannot connect to service"}), 502


@app.route("/api/messages")
def api_messages():
    chat_id = request.args.get("chat_id")
    token = request.args.get("token")
    if not chat_id or not token:
        return jsonify({"error": "chat_id and token required"}), 400

    try:
        resp = requests.post(
            f"{SERVICE_URL}/v1/messenger/recent-messages",
            json={"chat_id": chat_id},
            headers={"Authorization": f"Bearer {token}"},
            timeout=5,
        )
        data = resp.json()
        # Handle null response (no messages)
        if data is None:
            data = []
        return jsonify(data), resp.status_code
    except requests.ConnectionError:
        return jsonify({"error": "Cannot connect to service"}), 502


if __name__ == "__main__":
    print("Messenger Web UI starting on http://localhost:5000")
    print("Make sure the C++ service is running on localhost:8080")
    app.run(host="0.0.0.0", port=5000, debug=True)
