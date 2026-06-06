#!/usr/bin/env python3
"""
Messenger Interaction Script
=============================
Exercises the full messenger flow via both REST and WebSocket APIs.

Usage:
    python3 scripts/test_messenger_interaction.py

Requires:
    pip3 install websockets requests

The C++ working_day service must be running on localhost:8080.
"""

import asyncio
import json
import sys
import time

import requests
import websockets

BASE_URL = "http://localhost:8080"
WS_URL = "ws://localhost:8080/chat"

# Test users and their auth tokens
USERS = {
    "first_id": {"token": "first_token", "name": "First A"},
    "second_id": {"token": "second_token", "name": "Second B"},
}

PASS = "\033[92mPASS\033[0m"
FAIL = "\033[91mFAIL\033[0m"
INFO = "\033[94mINFO\033[0m"
WARN = "\033[93mWARN\033[0m"

findings = []
passes = 0


def log(level, msg):
    global passes
    if level == PASS:
        passes += 1
    print(f"  [{level}] {msg}")


def section(title):
    print(f"\n{'='*60}")
    print(f"  {title}")
    print(f"{'='*60}")


def auth_headers(user_id):
    return {"Authorization": f"Bearer {USERS[user_id]['token']}"}


def ws_url(user_id):
    """WebSocket URL with auth token as query parameter."""
    return f"{WS_URL}?token={USERS[user_id]['token']}"


# ---------------------------------------------------------------------------
# 1. Create a chat
# ---------------------------------------------------------------------------
def test_create_chat():
    section("1. Create a Chat")

    payload = {
        "chat_name": "Script Test Chat",
        "id_list": ["first_id", "second_id"],
    }
    resp = requests.post(
        f"{BASE_URL}/v1/messenger/create-chat",
        json=payload,
        headers=auth_headers("first_id"),
    )

    log(INFO, f"POST /v1/messenger/create-chat -> {resp.status_code}")
    log(INFO, f"Response body: {resp.text}")

    if resp.status_code != 200:
        log(FAIL, f"Expected 200, got {resp.status_code}")
        findings.append("create-chat returned non-200 status")
        return None

    data = resp.json()
    chat_id = data.get("chat_id")

    if chat_id:
        log(PASS, f"Chat created with id: {chat_id}")
    else:
        log(FAIL, "Response missing chat_id field")
        findings.append("create-chat response missing chat_id")
        return None

    return chat_id


# ---------------------------------------------------------------------------
# 2. List chats — verify new chat appears, no blank last_message
# ---------------------------------------------------------------------------
def test_list_chats(expected_chat_id):
    section("2. List Chats")

    for user_id in ["first_id", "second_id"]:
        resp = requests.post(
            f"{BASE_URL}/v1/messenger/list-chats?employee_id={user_id}",
            headers=auth_headers(user_id),
        )
        log(INFO, f"list-chats for {user_id} -> {resp.status_code}")

        if resp.status_code != 200:
            log(FAIL, f"Expected 200 for {user_id}, got {resp.status_code}")
            findings.append(f"list-chats failed for {user_id}")
            continue

        data = resp.json()
        chats = data.get("chats", [])
        log(INFO, f"  {user_id} has {len(chats)} chat(s)")

        chat_ids = [c["chat_id"] for c in chats]
        if expected_chat_id in chat_ids:
            log(PASS, f"  New chat {expected_chat_id} visible to {user_id}")
        else:
            log(FAIL, f"  New chat {expected_chat_id} NOT found for {user_id}")
            findings.append(f"New chat missing from list for {user_id}")

        # A newly created chat has no messages, so last_message fields will be empty.
        # This is expected behavior. BUG #5 was about a blank *message row* being
        # inserted; now no row is inserted, so last_message is simply the default.
        for chat in chats:
            if chat["chat_id"] == expected_chat_id:
                lm = chat.get("last_message", {})
                sender = lm.get("sender_id", "")
                content = lm.get("content", {}).get("content", "")
                if not sender and not content:
                    log(PASS, "  New chat has no messages yet (expected, BUG #5 fixed — no blank row inserted)")
                else:
                    log(INFO, f"  last_message: sender={sender}, content={content}")


# ---------------------------------------------------------------------------
# 3. Send a message via WebSocket (with auth)
# ---------------------------------------------------------------------------
async def test_websocket_send(chat_id):
    section("3. Send Message via WebSocket (Authenticated)")

    # Message payload — no sender_id or company_id needed (server sets from auth)
    message_payload = {
        "chat_id": chat_id,
        "content": {
            "content": "Hello from the interaction script!",
        },
    }

    log(INFO, f"Connecting to {ws_url('first_id')} ...")

    try:
        async with websockets.connect(ws_url("first_id")) as ws:
            log(PASS, "WebSocket connected with auth token")

            # Send message
            await ws.send(json.dumps(message_payload))
            log(INFO, f"Sent message: {json.dumps(message_payload, indent=2)}")

            # The server broadcasts the message back to all chat members,
            # including the sender (since sender is in employee_chats).
            try:
                response = await asyncio.wait_for(ws.recv(), timeout=3.0)
                log(INFO, f"Received echo: {response}")
                log(PASS, "Message echoed back to sender")

                echo_data = json.loads(response)
                if echo_data.get("sender_id") == "first_id":
                    log(PASS, "Echo contains correct authenticated sender_id")
                else:
                    log(FAIL, f"Echo sender_id mismatch: expected 'first_id', got '{echo_data.get('sender_id')}'")
                    findings.append(f"WebSocket echo sender_id mismatch: {echo_data.get('sender_id')}")

            except asyncio.TimeoutError:
                log(WARN, "No echo received within 3s (sender may not be in broadcast list)")
                findings.append("WebSocket did not echo message back to sender")

    except Exception as e:
        log(FAIL, f"WebSocket error: {e}")
        findings.append(f"WebSocket connection failed: {e}")


# ---------------------------------------------------------------------------
# 4. Retrieve messages — verify WebSocket message was persisted as proper JSON
# ---------------------------------------------------------------------------
def test_recent_messages(chat_id):
    section("4. Retrieve Messages (recent-messages)")

    resp = requests.post(
        f"{BASE_URL}/v1/messenger/recent-messages",
        json={"chat_id": chat_id},
        headers=auth_headers("first_id"),
    )

    log(INFO, f"POST /v1/messenger/recent-messages -> {resp.status_code}")

    if resp.status_code != 200:
        log(FAIL, f"Expected 200, got {resp.status_code}")
        findings.append("recent-messages returned non-200")
        return

    raw = resp.text
    log(INFO, f"Raw response (first 300 chars): {raw[:300]}")

    outer = json.loads(raw)

    if outer is None:
        outer = []
        log(INFO, "Response is null (no messages)")

    log(INFO, f"Number of messages: {len(outer)}")

    if not isinstance(outer, list):
        log(FAIL, f"Expected JSON array, got {type(outer).__name__}")
        findings.append("recent-messages did not return a JSON array")
        return

    # Check for double-serialization bug (BUG #7)
    if outer and isinstance(outer[0], str):
        log(FAIL, "BUG #7 NOT fixed: messages are still double-serialized JSON strings")
        findings.append("BUG #7 NOT fixed: recent-messages returns array of JSON strings")
        messages = [json.loads(m) for m in outer]
    elif outer and isinstance(outer[0], dict):
        log(PASS, "Messages are proper JSON objects (BUG #7 fixed)")
        messages = outer
    else:
        messages = []

    # Find our WebSocket message
    ws_msg_found = False
    blank_msg_found = False
    for msg in messages:
        content_text = msg.get("content", {}).get("content", "")
        sender = msg.get("sender_id", "")

        if "interaction script" in content_text:
            ws_msg_found = True
            log(PASS, f"Found WebSocket message: sender={sender}, content={content_text}")

        if not sender and not content_text:
            blank_msg_found = True

    if ws_msg_found:
        log(PASS, "WebSocket message was correctly persisted to the database")
    else:
        log(FAIL, "WebSocket message NOT found in recent-messages")
        findings.append("WebSocket message was not persisted")

    if blank_msg_found:
        log(FAIL, "BUG #5 NOT fixed: blank message row found")
        findings.append("BUG #5 NOT fixed: blank message row present in recent-messages")
    else:
        log(PASS, "No blank message rows (BUG #5 fixed)")

    # Check ordering — should be DESC by timestamp
    if len(messages) >= 2:
        timestamps = [m.get("timestamp", "") for m in messages]
        if timestamps == sorted(timestamps, reverse=True):
            log(PASS, "Messages are in DESC timestamp order")
        else:
            log(WARN, "Messages are NOT in DESC timestamp order")
            findings.append("recent-messages ordering may be wrong")


# ---------------------------------------------------------------------------
# 5. Two-user WebSocket broadcast test (with auth)
# ---------------------------------------------------------------------------
async def test_websocket_broadcast(chat_id):
    section("5. WebSocket Broadcast (Two Users, Authenticated)")

    log(INFO, "Connecting first_id and second_id with auth tokens...")

    try:
        async with websockets.connect(ws_url("first_id")) as ws_first, \
                     websockets.connect(ws_url("second_id")) as ws_second:

            log(PASS, "Both users connected with auth tokens")

            # Both users are registered on connect now (ISSUE #8 fixed),
            # so no need for "registration" messages.

            # Small delay for queue registration to take effect
            await asyncio.sleep(0.5)

            # first_id sends a real message
            test_msg = {
                "chat_id": chat_id,
                "content": {
                    "content": "Broadcast test from first_id!",
                },
            }
            await ws_first.send(json.dumps(test_msg))
            log(INFO, "first_id sent broadcast test message")

            # Check if second_id receives it
            try:
                received = await asyncio.wait_for(ws_second.recv(), timeout=3.0)
                received_data = json.loads(received)
                log(PASS, f"second_id received broadcast: {received_data.get('content', {})}")

                # Verify sender_id is the authenticated user, not client-provided (ISSUE #2 fixed)
                if received_data.get("sender_id") == "first_id":
                    log(PASS, "Broadcast sender_id is correct (authenticated, ISSUE #2 fixed)")
                else:
                    log(FAIL, f"Broadcast sender_id mismatch: {received_data.get('sender_id')}")
                    findings.append("sender_id in broadcast not matching authenticated user")

                content = received_data.get("content", {})
                if isinstance(content, dict) and "Broadcast test" in content.get("content", ""):
                    log(PASS, "Broadcast content matches what was sent")
                elif isinstance(content, str) and "Broadcast test" in content:
                    log(PASS, "Broadcast content matches (content is a string, not object)")
                else:
                    log(WARN, f"Broadcast content mismatch: {content}")

            except asyncio.TimeoutError:
                log(FAIL, "second_id did NOT receive broadcast within 3s")
                findings.append("WebSocket broadcast failed: second user did not receive message")

            # Check if first_id also receives their own message (echo)
            try:
                echo = await asyncio.wait_for(ws_first.recv(), timeout=2.0)
                log(PASS, f"first_id received echo of own message")
            except asyncio.TimeoutError:
                log(INFO, "first_id did not receive echo of own message (may be expected)")

    except Exception as e:
        log(FAIL, f"Broadcast test error: {e}")
        findings.append(f"Broadcast test failed: {e}")


# ---------------------------------------------------------------------------
# 6. WebSocket rejects unauthenticated connections
# ---------------------------------------------------------------------------
async def test_websocket_no_auth():
    section("6. WebSocket Rejects Unauthenticated Connections")

    try:
        async with websockets.connect(WS_URL) as ws:
            # If we get here, the connection succeeded without auth — that's bad
            log(FAIL, "WebSocket connected WITHOUT auth token — ISSUE #1 NOT fixed")
            findings.append("ISSUE #1 NOT fixed: WebSocket still allows unauthenticated connections")
    except Exception as e:
        log(PASS, f"WebSocket correctly rejected unauthenticated connection: {type(e).__name__}")

    # Also test with invalid token
    try:
        async with websockets.connect(f"{WS_URL}?token=invalid_token_xyz") as ws:
            log(FAIL, "WebSocket connected with INVALID token — auth not working properly")
            findings.append("WebSocket accepts invalid tokens")
    except Exception as e:
        log(PASS, f"WebSocket correctly rejected invalid token: {type(e).__name__}")


# ---------------------------------------------------------------------------
# 7. REST unauthorized access check
# ---------------------------------------------------------------------------
def test_unauthorized_access():
    section("7. Unauthorized REST Access Check")

    # list-chats without auth
    resp = requests.post(f"{BASE_URL}/v1/messenger/list-chats?employee_id=first_id")
    log(INFO, f"list-chats without auth -> {resp.status_code}")
    if resp.status_code == 401:
        log(PASS, "list-chats correctly returns 401 without auth")
    else:
        log(FAIL, f"Expected 401, got {resp.status_code}")
        findings.append("list-chats does not require auth")

    # create-chat without auth
    resp = requests.post(
        f"{BASE_URL}/v1/messenger/create-chat",
        json={"chat_name": "unauth", "id_list": ["first_id"]},
    )
    log(INFO, f"create-chat without auth -> {resp.status_code}")
    if resp.status_code == 401:
        log(PASS, "create-chat correctly returns 401 without auth")
    else:
        log(FAIL, f"Expected 401, got {resp.status_code}")
        findings.append("create-chat does not require auth")

    # recent-messages without auth
    resp = requests.post(
        f"{BASE_URL}/v1/messenger/recent-messages",
        json={"chat_id": "chat1"},
    )
    log(INFO, f"recent-messages without auth -> {resp.status_code}")
    if resp.status_code == 401:
        log(PASS, "recent-messages correctly returns 401 without auth")
    else:
        log(FAIL, f"Expected 401, got {resp.status_code}")
        findings.append("recent-messages does not require auth")


# ---------------------------------------------------------------------------
# 8. Group chat (3 members)
# ---------------------------------------------------------------------------
def test_create_chat_with_three_members():
    section("8. Create Group Chat (3 members)")

    payload = {
        "chat_name": "Group Chat Test",
        "id_list": ["first_id", "second_id", "stranger_id"],
    }
    resp = requests.post(
        f"{BASE_URL}/v1/messenger/create-chat",
        json=payload,
        headers=auth_headers("first_id"),
    )
    log(INFO, f"create-chat (3 members) -> {resp.status_code}")

    if resp.status_code == 200:
        group_chat_id = resp.json().get("chat_id")
        log(PASS, f"Group chat created: {group_chat_id}")

        # Verify members can see it
        for uid in ["first_id", "second_id"]:
            resp2 = requests.post(
                f"{BASE_URL}/v1/messenger/list-chats?employee_id={uid}",
                headers=auth_headers(uid),
            )
            if resp2.status_code == 200:
                chat_ids = [c["chat_id"] for c in resp2.json().get("chats", [])]
                if group_chat_id in chat_ids:
                    log(PASS, f"  {uid} can see group chat")
                else:
                    log(FAIL, f"  {uid} cannot see group chat")
            else:
                log(FAIL, f"  list-chats failed for {uid}: {resp2.status_code}")
    else:
        log(FAIL, f"Group chat creation failed: {resp.status_code}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
async def main():
    print("\n" + "=" * 60)
    print("  MESSENGER INTERACTION SCRIPT")
    print("  Testing REST + WebSocket APIs on localhost:8080")
    print("=" * 60)

    # Verify service is up
    try:
        resp = requests.get(f"{BASE_URL}/ping", timeout=3)
        if resp.status_code == 200:
            log(PASS, "Service is running (ping OK)")
        else:
            log(FAIL, f"Ping returned {resp.status_code}")
            sys.exit(1)
    except requests.ConnectionError:
        log(FAIL, "Cannot connect to service at localhost:8080")
        sys.exit(1)

    # Step 1: Create a chat
    chat_id = test_create_chat()
    if not chat_id:
        log(FAIL, "Cannot continue without a chat_id")
        sys.exit(1)

    # Step 2: List chats
    test_list_chats(chat_id)

    # Step 3: Send message via WebSocket (with auth)
    await test_websocket_send(chat_id)

    # Step 4: Retrieve messages
    test_recent_messages(chat_id)

    # Step 5: Two-user broadcast (with auth)
    await test_websocket_broadcast(chat_id)

    # Step 6: WebSocket auth rejection
    await test_websocket_no_auth()

    # Step 7: Unauthorized REST access
    test_unauthorized_access()

    # Step 8: Group chat
    test_create_chat_with_three_members()

    # Summary
    section("RESULTS SUMMARY")
    if findings:
        print(f"\n  Issues found ({len(findings)}):")
        for i, f in enumerate(findings, 1):
            print(f"  {i}. {f}")
    else:
        print("  All checks passed! No issues found.")

    print(f"\n  Passed: {passes}")
    print(f"  Findings: {len(findings)}")
    print()


if __name__ == "__main__":
    asyncio.run(main())
