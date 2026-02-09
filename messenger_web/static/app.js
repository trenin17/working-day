// State
let currentUser = null;   // { id, name, token, role }
let currentChatId = null;
let ws = null;
let chatListRefreshTimer = null;

// DOM refs
const userSelect = document.getElementById("user-select");
const wsStatus = document.getElementById("ws-status");
const chatListEl = document.getElementById("chat-list");
const chatPlaceholder = document.getElementById("chat-placeholder");
const chatView = document.getElementById("chat-view");
const chatTitle = document.getElementById("chat-title");
const messagesEl = document.getElementById("messages");
const msgInput = document.getElementById("msg-input");
const btnSend = document.getElementById("btn-send");
const btnNewChat = document.getElementById("btn-new-chat");
const modalOverlay = document.getElementById("modal-overlay");
const newChatNameInput = document.getElementById("new-chat-name");
const memberCheckboxes = document.getElementById("member-checkboxes");
const btnModalCancel = document.getElementById("btn-modal-cancel");
const btnModalCreate = document.getElementById("btn-modal-create");

// ---------- Initialization ----------

function init() {
    selectUser();
    userSelect.addEventListener("change", () => {
        selectUser();
    });
    btnSend.addEventListener("click", sendMessage);
    msgInput.addEventListener("keydown", (e) => {
        if (e.key === "Enter") sendMessage();
    });
    btnNewChat.addEventListener("click", openNewChatModal);
    btnModalCancel.addEventListener("click", closeNewChatModal);
    btnModalCreate.addEventListener("click", createChat);
}

function selectUser() {
    const opt = userSelect.selectedOptions[0];
    currentUser = {
        id: opt.value,
        token: opt.dataset.token,
        name: opt.dataset.name,
    };
    currentChatId = null;
    showPlaceholder();
    loadChats();
    connectWebSocket();
    startChatListRefresh();
}

// ---------- Chat List ----------

async function loadChats() {
    try {
        const resp = await fetch(`/api/chats?user_id=${currentUser.id}&token=${currentUser.token}`);
        if (!resp.ok) {
            chatListEl.innerHTML = `<p style="padding:16px;color:var(--text-muted)">Failed to load chats</p>`;
            return;
        }
        const data = await resp.json();
        renderChatList(data.chats || []);
    } catch (err) {
        chatListEl.innerHTML = `<p style="padding:16px;color:var(--text-muted)">Error: ${err.message}</p>`;
    }
}

function renderChatList(chats) {
    if (chats.length === 0) {
        chatListEl.innerHTML = `<p style="padding:16px;color:var(--text-muted)">No chats yet</p>`;
        return;
    }
    chatListEl.innerHTML = chats.map(c => {
        const lm = c.last_message || {};
        const preview = lm.content?.content
            ? `${lm.sender_id || "?"}: ${lm.content.content}`
            : "No messages yet";
        const active = c.chat_id === currentChatId ? " active" : "";
        return `
            <div class="chat-item${active}" data-chat-id="${c.chat_id}" data-chat-name="${esc(c.chat_name)}">
                <div class="chat-item-name">${esc(c.chat_name)}</div>
                <div class="chat-item-preview">${esc(preview)}</div>
            </div>
        `;
    }).join("");

    chatListEl.querySelectorAll(".chat-item").forEach(el => {
        el.addEventListener("click", () => {
            openChat(el.dataset.chatId, el.dataset.chatName);
        });
    });
}

function startChatListRefresh() {
    if (chatListRefreshTimer) clearInterval(chatListRefreshTimer);
    chatListRefreshTimer = setInterval(loadChats, 10000);
}

// ---------- Chat View ----------

function showPlaceholder() {
    chatPlaceholder.classList.remove("hidden");
    chatView.classList.add("hidden");
}

async function openChat(chatId, chatName) {
    currentChatId = chatId;
    chatPlaceholder.classList.add("hidden");
    chatView.classList.remove("hidden");
    chatTitle.textContent = chatName;
    msgInput.focus();

    // Highlight active chat in sidebar
    chatListEl.querySelectorAll(".chat-item").forEach(el => {
        el.classList.toggle("active", el.dataset.chatId === chatId);
    });

    await loadMessages();
}

async function loadMessages() {
    if (!currentChatId) return;
    try {
        const resp = await fetch(`/api/messages?chat_id=${currentChatId}&token=${currentUser.token}`);
        if (!resp.ok) {
            messagesEl.innerHTML = `<p style="color:var(--text-muted)">Failed to load messages</p>`;
            return;
        }
        const data = await resp.json();
        renderMessages(data || []);
    } catch (err) {
        messagesEl.innerHTML = `<p style="color:var(--text-muted)">Error: ${err.message}</p>`;
    }
}

function renderMessages(msgs) {
    // Messages come in DESC order from API; reverse to display chronologically
    const sorted = [...msgs].reverse();
    messagesEl.innerHTML = sorted.map(m => {
        const isOwn = m.sender_id === currentUser.id;
        const senderName = USERS[m.sender_id]?.name || m.sender_id || "Unknown";
        const contentText = m.content?.content || m.content || "";
        const time = m.timestamp ? formatTime(m.timestamp) : "";
        return `
            <div class="message ${isOwn ? "own" : "other"}">
                ${!isOwn ? `<div class="sender">${esc(senderName)}</div>` : ""}
                <div class="text">${esc(contentText)}</div>
                <div class="time">${esc(time)}</div>
            </div>
        `;
    }).join("");
    scrollToBottom();
}

function appendMessage(msg) {
    const isOwn = msg.sender_id === currentUser.id;
    const senderName = USERS[msg.sender_id]?.name || msg.sender_id || "Unknown";
    const contentText = msg.content?.content || msg.content || "";
    const time = msg.timestamp ? formatTime(msg.timestamp) : formatTime(new Date().toISOString());
    const div = document.createElement("div");
    div.className = `message ${isOwn ? "own" : "other"}`;
    div.innerHTML = `
        ${!isOwn ? `<div class="sender">${esc(senderName)}</div>` : ""}
        <div class="text">${esc(contentText)}</div>
        <div class="time">${esc(time)}</div>
    `;
    messagesEl.appendChild(div);
    scrollToBottom();
}

function scrollToBottom() {
    messagesEl.scrollTop = messagesEl.scrollHeight;
}

// ---------- Send Message ----------

function sendMessage() {
    const text = msgInput.value.trim();
    if (!text || !currentChatId || !ws || ws.readyState !== WebSocket.OPEN) return;

    const payload = {
        chat_id: currentChatId,
        content: { content: text },
    };
    ws.send(JSON.stringify(payload));
    msgInput.value = "";
    msgInput.focus();
}

// ---------- WebSocket ----------

function connectWebSocket() {
    if (ws) {
        ws.close();
        ws = null;
    }

    const url = `${WS_BASE_URL}?token=${currentUser.token}`;
    ws = new WebSocket(url);

    ws.onopen = () => {
        wsStatus.className = "status-dot connected";
        wsStatus.title = "WebSocket connected";
    };

    ws.onclose = () => {
        wsStatus.className = "status-dot disconnected";
        wsStatus.title = "WebSocket disconnected";
        // Auto-reconnect after 3s
        setTimeout(() => {
            if (currentUser) connectWebSocket();
        }, 3000);
    };

    ws.onerror = () => {
        wsStatus.className = "status-dot disconnected";
        wsStatus.title = "WebSocket error";
    };

    ws.onmessage = (event) => {
        try {
            const msg = JSON.parse(event.data);
            // If this message belongs to the currently open chat, append it
            if (msg.chat_id === currentChatId) {
                appendMessage(msg);
            }
            // Refresh chat list to update last_message previews
            loadChats();
        } catch (err) {
            console.error("WebSocket message parse error:", err);
        }
    };
}

// ---------- New Chat Modal ----------

function openNewChatModal() {
    newChatNameInput.value = "";
    memberCheckboxes.innerHTML = Object.entries(USERS)
        .filter(([uid]) => uid !== currentUser.id)
        .map(([uid, u]) => `
            <label>
                <input type="checkbox" value="${uid}"> ${esc(u.name)} (${esc(u.role)})
            </label>
        `).join("");
    modalOverlay.classList.remove("hidden");
    newChatNameInput.focus();
}

function closeNewChatModal() {
    modalOverlay.classList.add("hidden");
}

async function createChat() {
    const chatName = newChatNameInput.value.trim();
    if (!chatName) {
        alert("Please enter a chat name");
        return;
    }

    const checked = memberCheckboxes.querySelectorAll("input:checked");
    const members = [currentUser.id];
    checked.forEach(cb => members.push(cb.value));

    if (members.length < 2) {
        alert("Select at least one other member");
        return;
    }

    try {
        const resp = await fetch("/api/chats/create", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
                token: currentUser.token,
                chat_name: chatName,
                id_list: members,
            }),
        });
        if (!resp.ok) {
            const err = await resp.json();
            alert(`Failed to create chat: ${err.error || resp.status}`);
            return;
        }
        const data = await resp.json();
        closeNewChatModal();
        await loadChats();
        // Open the newly created chat
        if (data.chat_id) {
            openChat(data.chat_id, chatName);
        }
    } catch (err) {
        alert(`Error creating chat: ${err.message}`);
    }
}

// ---------- Utilities ----------

function esc(str) {
    if (!str) return "";
    const div = document.createElement("div");
    div.textContent = str;
    return div.innerHTML;
}

function formatTime(ts) {
    try {
        const d = new Date(ts);
        if (isNaN(d.getTime())) return ts;
        return d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
    } catch {
        return ts;
    }
}

// ---------- Start ----------
init();
