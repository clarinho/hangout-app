const { contextBridge, ipcRenderer } = require("electron");

const request = async (apiBaseUrl, path, options = {}) => {
  const response = await fetch(`${apiBaseUrl}${path}`, {
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...(options.headers || {})
    }
  });

  const text = await response.text();
  const payload = text.length > 0 ? JSON.parse(text) : {};

  if (!response.ok) {
    const message = payload?.error?.message || `Request failed with ${response.status}`;
    throw new Error(message);
  }

  return payload;
};

contextBridge.exposeInMainWorld("hangout", {
  getConfig: () => ipcRenderer.invoke("config:get"),
  getUpdateState: () => ipcRenderer.invoke("updates:getState"),
  onUpdateStatus: (callback) => {
    const listener = (_event, status) => callback(status);
    ipcRenderer.on("updates:status", listener);
    return () => ipcRenderer.removeListener("updates:status", listener);
  },
  login: (apiBaseUrl, username) =>
    request(apiBaseUrl, "/api/v1/auth/login", {
      method: "POST",
      body: JSON.stringify({ username })
    }),
  me: (apiBaseUrl, token) =>
    request(apiBaseUrl, "/api/v1/me", {
      headers: { Authorization: `Bearer ${token}` }
    }),
  updateProfile: (apiBaseUrl, token, profile) =>
    request(apiBaseUrl, "/api/v1/me/profile", {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` },
      body: JSON.stringify(profile)
    }),
  heartbeat: (apiBaseUrl, token) =>
    request(apiBaseUrl, "/api/v1/me/heartbeat", {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` }
    }),
  servers: (apiBaseUrl, token) =>
    request(apiBaseUrl, "/api/v1/servers", {
      headers: { Authorization: `Bearer ${token}` }
    }),
  createServer: (apiBaseUrl, token, name) =>
    request(apiBaseUrl, "/api/v1/servers", {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` },
      body: JSON.stringify({ name })
    }),
  joinServer: (apiBaseUrl, token, inviteCode) =>
    request(apiBaseUrl, "/api/v1/servers/join", {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` },
      body: JSON.stringify({ inviteCode })
    }),
  serverInvite: (apiBaseUrl, token, serverId) =>
    request(apiBaseUrl, `/api/v1/servers/${serverId}/invite`, {
      headers: { Authorization: `Bearer ${token}` }
    }),
  regenerateServerInvite: (apiBaseUrl, token, serverId) =>
    request(apiBaseUrl, `/api/v1/servers/${serverId}/invite/regenerate`, {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` }
    }),
  serverMembers: (apiBaseUrl, token, serverId) =>
    request(apiBaseUrl, `/api/v1/servers/${serverId}/members`, {
      headers: { Authorization: `Bearer ${token}` }
    }),
  friends: (apiBaseUrl, token) =>
    request(apiBaseUrl, "/api/v1/friends", {
      headers: { Authorization: `Bearer ${token}` }
    }),
  sendFriendRequest: (apiBaseUrl, token, username) =>
    request(apiBaseUrl, "/api/v1/friends/requests", {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` },
      body: JSON.stringify({ username })
    }),
  acceptFriendRequest: (apiBaseUrl, token, requestId) =>
    request(apiBaseUrl, `/api/v1/friends/requests/${requestId}/accept`, {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` }
    }),
  denyFriendRequest: (apiBaseUrl, token, requestId) =>
    request(apiBaseUrl, `/api/v1/friends/requests/${requestId}/deny`, {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` }
    }),
  removeFriend: (apiBaseUrl, token, friendId) =>
    request(apiBaseUrl, `/api/v1/friends/${friendId}`, {
      method: "DELETE",
      headers: { Authorization: `Bearer ${token}` }
    }),
  channels: (apiBaseUrl, token, serverId) =>
    request(apiBaseUrl, `/api/v1/servers/${serverId}/channels`, {
      headers: { Authorization: `Bearer ${token}` }
    }),
  createChannel: (apiBaseUrl, token, serverId, name) =>
    request(apiBaseUrl, `/api/v1/servers/${serverId}/channels`, {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` },
      body: JSON.stringify({ name })
    }),
  messages: (apiBaseUrl, token, channelId, limit = 60) =>
    request(apiBaseUrl, `/api/v1/channels/${channelId}/messages?limit=${limit}`, {
      headers: { Authorization: `Bearer ${token}` }
    }),
  searchMessages: (apiBaseUrl, token, channelId, query, limit = 60) =>
    request(apiBaseUrl, `/api/v1/channels/${channelId}/messages?limit=${limit}&q=${encodeURIComponent(query)}`, {
      headers: { Authorization: `Bearer ${token}` }
    }),
  editMessage: (apiBaseUrl, token, messageId, content) =>
    request(apiBaseUrl, `/api/v1/messages/${messageId}/edit`, {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` },
      body: JSON.stringify({ content })
    }),
  setChannelPosition: (apiBaseUrl, token, channelId, position) =>
    request(apiBaseUrl, `/api/v1/channels/${channelId}/position`, {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` },
      body: JSON.stringify({ position })
    }),
  deleteMessage: (apiBaseUrl, token, messageId) =>
    request(apiBaseUrl, `/api/v1/messages/${messageId}`, {
      method: "DELETE",
      headers: { Authorization: `Bearer ${token}` }
    }),
  reactToMessage: (apiBaseUrl, token, messageId, emoji) =>
    request(apiBaseUrl, `/api/v1/messages/${messageId}/reactions`, {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` },
      body: JSON.stringify({ emoji })
    }),
  sendMessage: (apiBaseUrl, token, channelId, content) =>
    request(apiBaseUrl, `/api/v1/channels/${channelId}/messages`, {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` },
      body: JSON.stringify({ content })
    }),
  dmConversations: (apiBaseUrl, token) =>
    request(apiBaseUrl, "/api/v1/dms", {
      headers: { Authorization: `Bearer ${token}` }
    }),
  openDm: (apiBaseUrl, token, username) =>
    request(apiBaseUrl, "/api/v1/dms", {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` },
      body: JSON.stringify({ username })
    }),
  dmMessages: (apiBaseUrl, token, conversationId, limit = 60) =>
    request(apiBaseUrl, `/api/v1/dms/${conversationId}/messages?limit=${limit}`, {
      headers: { Authorization: `Bearer ${token}` }
    }),
  sendDmMessage: (apiBaseUrl, token, conversationId, content) =>
    request(apiBaseUrl, `/api/v1/dms/${conversationId}/messages`, {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` },
      body: JSON.stringify({ content })
    }),
  deleteDmMessage: (apiBaseUrl, token, messageId) =>
    request(apiBaseUrl, `/api/v1/dm-messages/${messageId}`, {
      method: "DELETE",
      headers: { Authorization: `Bearer ${token}` }
    }),
  editDmMessage: (apiBaseUrl, token, messageId, content) =>
    request(apiBaseUrl, `/api/v1/dm-messages/${messageId}/edit`, {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` },
      body: JSON.stringify({ content })
    }),
  reactToDmMessage: (apiBaseUrl, token, messageId, emoji) =>
    request(apiBaseUrl, `/api/v1/dm-messages/${messageId}/reactions`, {
      method: "POST",
      headers: { Authorization: `Bearer ${token}` },
      body: JSON.stringify({ emoji })
    }),
  copyText: (text) => ipcRenderer.invoke("clipboard:writeText", text)
});
