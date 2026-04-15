const state = {
  apiBaseUrl: "",
  token: localStorage.getItem("hangout.token") || "",
  user: JSON.parse(localStorage.getItem("hangout.user") || "null"),
  updateDismissTimer: null,
  updateOverlayStartedAt: Date.now(),
  servers: [],
  channels: [],
  messages: [],
  friends: { friends: [], inbound: [], outbound: [] },
  dmConversations: [],
  activeMode: "channel",
  activeConversationId: null,
  activeServerId: null,
  activeChannelId: null,
  pollTimer: null
};

const $ = (id) => document.getElementById(id);

const els = {
  apiBaseText: $("apiBaseText"),
  accountAvatar: $("accountAvatar"),
  accountName: $("accountName"),
  accountStrip: $("accountStrip"),
  channelList: $("channelList"),
  channelName: $("channelName"),
  closeNameButton: $("closeNameButton"),
  closeServerSettingsButton: $("closeServerSettingsButton"),
  closeSettingsButton: $("closeSettingsButton"),
  connectionPill: $("connectionPill"),
  copyInviteButton: $("copyInviteButton"),
  createChannelButton: $("createChannelButton"),
  createServerButton: $("createServerButton"),
  currentUser: $("currentUser"),
  avatarColorInput: $("avatarColorInput"),
  displayNameInput: $("displayNameInput"),
  dmList: $("dmList"),
  emptyState: $("emptyState"),
  closeFriendsButton: $("closeFriendsButton"),
  friendRequestForm: $("friendRequestForm"),
  friendsButton: $("friendsButton"),
  friendUsernameInput: $("friendUsernameInput"),
  friendsList: $("friendsList"),
  friendsOverlay: $("friendsOverlay"),
  loginForm: $("loginForm"),
  logoutButton: $("logoutButton"),
  messageForm: $("messageForm"),
  messageInput: $("messageInput"),
  messagePane: $("messagePane"),
  messageSearchInput: $("messageSearchInput"),
  inviteCodeText: $("inviteCodeText"),
  inboundRequestsList: $("inboundRequestsList"),
  joinServerButton: $("joinServerButton"),
  nameForm: $("nameForm"),
  nameHelpText: $("nameHelpText"),
  nameInput: $("nameInput"),
  nameInputLabel: $("nameInputLabel"),
  nameModalKicker: $("nameModalKicker"),
  nameModalTitle: $("nameModalTitle"),
  nameOverlay: $("nameOverlay"),
  nameSubmitButton: $("nameSubmitButton"),
  outboundRequestsList: $("outboundRequestsList"),
  popoverAvatar: $("popoverAvatar"),
  popoverMeta: $("popoverMeta"),
  popoverName: $("popoverName"),
  profilePopover: $("profilePopover"),
  profileForm: $("profileForm"),
  regenerateInviteButton: $("regenerateInviteButton"),
  refreshDmsButton: $("refreshDmsButton"),
  refreshButton: $("refreshButton"),
  settingsApiBase: $("settingsApiBase"),
  settingsAvatar: $("settingsAvatar"),
  settingsButton: $("settingsButton"),
  settingsOverlay: $("settingsOverlay"),
  settingsUsername: $("settingsUsername"),
  sendButton: $("sendButton"),
  serverPlusButton: $("serverPlusButton"),
  serverPlusMenu: $("serverPlusMenu"),
  serverSettingsButton: $("serverSettingsButton"),
  serverSettingsOverlay: $("serverSettingsOverlay"),
  serverSettingsTitle: $("serverSettingsTitle"),
  serverMembersList: $("serverMembersList"),
  serverList: $("serverList"),
  serverName: $("serverName"),
  serverRail: $("serverRail"),
  statusTextInput: $("statusTextInput"),
  toast: $("toast"),
  updateMessage: $("updateMessage"),
  updateOverlay: $("updateOverlay"),
  updatePercent: $("updatePercent"),
  updateProgress: document.querySelector(".update-progress"),
  updateProgressFill: $("updateProgressFill"),
  updateTitle: $("updateTitle"),
  userStatusInput: $("userStatusInput"),
  usernameInput: $("usernameInput")
};

const showToast = (message) => {
  els.toast.textContent = message;
  els.toast.classList.add("visible");
  window.clearTimeout(showToast.timer);
  showToast.timer = window.setTimeout(() => els.toast.classList.remove("visible"), 2600);
};

const setConnected = (isConnected) => {
  els.connectionPill.textContent = isConnected ? "Backend online" : "Backend offline";
  els.connectionPill.classList.toggle("online", isConnected);
};

const dismissUpdateOverlay = () => {
  els.updateOverlay.classList.add("dismissed");
};

const renderUpdateStatus = (status = {}) => {
  const phase = status.status || "checking";
  const percent = Math.max(0, Math.min(100, Number(status.percent) || 0));
  const isDownloading = phase === "downloading" || phase === "downloaded";
  const isChecking = phase === "checking" || phase === "available" || phase === "idle";
  const canDismiss = ["not-available", "skipped", "error"].includes(phase);

  window.clearTimeout(state.updateDismissTimer);
  els.updateOverlay.classList.remove("dismissed");
  els.updateProgress.classList.toggle("indeterminate", isChecking && !isDownloading);
  els.updateProgressFill.style.width = isDownloading ? `${percent}%` : "0%";

  const titles = {
    idle: "Checking for updates",
    checking: "Checking for updates",
    available: "Update found",
    downloading: "Downloading update",
    downloaded: "Installing update",
    "not-available": "You're up to date",
    skipped: "Starting Hangout",
    error: "Starting Hangout"
  };

  els.updateTitle.textContent = titles[phase] || "Checking for updates";
  els.updateMessage.textContent = status.message || "Making sure you have the newest build.";
  els.updatePercent.textContent = isDownloading ? `${Math.round(percent)}%` : "Please wait";

  if (phase === "not-available") {
    els.updatePercent.textContent = "Current version";
  } else if (phase === "skipped") {
    els.updatePercent.textContent = "Development mode";
  } else if (phase === "error") {
    els.updatePercent.textContent = "Update check unavailable";
  } else if (phase === "downloaded") {
    els.updatePercent.textContent = "Restarting";
  }

  if (canDismiss) {
    const elapsedMs = Date.now() - state.updateOverlayStartedAt;
    const remainingMs = Math.max(0, 2000 - elapsedMs);
    state.updateDismissTimer = window.setTimeout(dismissUpdateOverlay, remainingMs);
  }
};

const initials = (value) =>
  value
    .split(/[\s_-]+/)
    .filter(Boolean)
    .slice(0, 2)
    .map((part) => part[0]?.toUpperCase())
    .join("") || "H";

const formatTime = (ms) =>
  new Intl.DateTimeFormat(undefined, {
    hour: "numeric",
    minute: "2-digit"
  }).format(new Date(ms));

const escapeHtml = (value) =>
  String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");

const activeServer = () => state.servers.find((server) => server.id === state.activeServerId);
const activeChannel = () => state.channels.find((channel) => channel.id === state.activeChannelId);
const activeConversation = () =>
  state.dmConversations.find((conversation) => conversation.id === state.activeConversationId);
let pendingNameModal = null;

const renderUser = () => {
  const username = state.user?.username || "Guest";
  const avatar = initials(username);

  els.currentUser.textContent = username;
  els.accountName.textContent = username;
  els.accountAvatar.textContent = avatar;
  els.settingsUsername.textContent = username;
  els.settingsAvatar.textContent = avatar;
  els.settingsApiBase.textContent = state.apiBaseUrl || "Backend not connected";
  els.displayNameInput.value = state.user?.displayName || username;
  els.statusTextInput.value = state.user?.statusText || "";
  els.userStatusInput.value = state.user?.userStatus || "online";
  els.avatarColorInput.value = state.user?.avatarColor || "#c315d2";
  els.loginForm.classList.toggle("hidden", Boolean(state.token));
  els.accountStrip.classList.toggle("hidden", !state.token);
  els.createServerButton.disabled = !state.token;
  els.joinServerButton.disabled = !state.token;
  els.createChannelButton.disabled = !state.token || !state.activeServerId;
  els.serverPlusButton.disabled = !state.token;
  els.serverSettingsButton.disabled = !state.token || !state.activeServerId;
  els.friendsButton.disabled = !state.token;
};

const renderServers = () => {
  els.serverList.innerHTML = "";
  els.serverRail.innerHTML = "";

  for (const server of state.servers) {
    const nav = document.createElement("button");
    nav.className = `nav-button ${server.id === state.activeServerId ? "active" : ""}`;
    nav.type = "button";
    nav.innerHTML = `
      <span class="hash">+</span>
      <span class="label">${escapeHtml(server.name)}</span>
      <span class="count">${state.channels.length}</span>
    `;
    nav.addEventListener("click", () => selectServer(server.id));
    els.serverList.appendChild(nav);

    const rail = document.createElement("button");
    rail.className = `server-dot ${server.id === state.activeServerId ? "active" : ""}`;
    rail.type = "button";
    rail.title = server.name;
    rail.textContent = initials(server.name);
    rail.addEventListener("click", () => selectServer(server.id));
    els.serverRail.appendChild(rail);
  }
};

const renderChannels = () => {
  els.channelList.innerHTML = "";

  for (const channel of state.channels) {
    const button = document.createElement("button");
    button.className = `nav-button ${channel.id === state.activeChannelId ? "active" : ""}`;
    button.type = "button";
    button.innerHTML = `
      <span class="hash">#</span>
      <span class="label">${escapeHtml(channel.name)}</span>
      <span class="channel-order">
        <button type="button" data-channel-move="${channel.id}" data-direction="-1">Up</button>
        <button type="button" data-channel-move="${channel.id}" data-direction="1">Down</button>
      </span>
    `;
    button.addEventListener("click", (event) => {
      if (event.target.closest("[data-channel-move]")) {
        return;
      }
      selectChannel(channel.id);
    });
    els.channelList.appendChild(button);
  }
};

const renderDmList = () => {
  els.dmList.innerHTML = "";
  for (const conversation of state.dmConversations) {
    const button = document.createElement("button");
    button.className = `nav-button ${
      state.activeMode === "dm" && conversation.id === state.activeConversationId ? "active" : ""
    }`;
    button.type = "button";
    button.innerHTML = `
      <span class="hash">@</span>
      <span class="label">${escapeHtml(conversation.otherUser.username)}</span>
      <span class="count"></span>
    `;
    button.addEventListener("click", () => selectDm(conversation.id));
    els.dmList.appendChild(button);
  }
};

const renderHeader = () => {
  const server = activeServer();
  const channel = activeChannel();
  const conversation = activeConversation();
  if (state.activeMode === "dm" && conversation) {
    els.serverName.textContent = "Direct Message";
    els.channelName.textContent = `@ ${conversation.otherUser.username}`;
    els.messageInput.placeholder = `Message @${conversation.otherUser.username}`;
    return;
  }

  els.serverName.textContent = server?.name || (state.token ? "Connected" : "Not connected");
  els.channelName.textContent = channel ? `# ${channel.name}` : "Choose a channel";
  els.messageInput.placeholder = channel ? `Message #${channel.name}` : "Choose a channel";
};

const renderMessages = () => {
  const hasTarget =
    (state.activeMode === "channel" && Boolean(state.activeChannelId)) ||
    (state.activeMode === "dm" && Boolean(state.activeConversationId));
  els.emptyState.style.display = hasTarget ? "none" : "grid";
  els.messagePane.classList.toggle("active", hasTarget);
  els.messageInput.disabled = !hasTarget;
  els.sendButton.disabled = !hasTarget;

  if (!hasTarget) {
    els.messagePane.innerHTML = "";
    return;
  }

  const nearBottom =
    els.messagePane.scrollHeight - els.messagePane.scrollTop - els.messagePane.clientHeight < 120;

  els.messagePane.innerHTML = "";
  for (const message of state.messages) {
    const row = document.createElement("article");
    row.className = "message";

    const avatar = document.createElement("div");
    avatar.className = "avatar";
    avatar.dataset.profileUsername = message.authorUsername;
    avatar.textContent = initials(message.authorUsername);

    const main = document.createElement("div");
    main.className = "message-main";

    const meta = document.createElement("div");
    meta.className = "message-meta";
    meta.innerHTML = `
      <span class="message-author" data-profile-username="${escapeHtml(message.authorUsername)}">${escapeHtml(message.authorUsername)}</span>
      <span class="message-time">${formatTime(message.createdAtMs)}</span>
      ${message.editedAtMs ? '<span class="message-time">(Edited)</span>' : ""}
    `;

    const content = document.createElement("div");
    content.className = "message-content";
    content.textContent = message.content;

    const reactions = document.createElement("div");
    reactions.className = "reaction-row";
    for (const reaction of message.reactions || []) {
      const chip = document.createElement("button");
      chip.className = `reaction-chip ${reaction.reactedByMe ? "active" : ""}`;
      chip.type = "button";
      chip.dataset.messageId = message.id;
      chip.dataset.emoji = reaction.emoji;
      chip.textContent = `${reaction.emoji} ${reaction.count}`;
      reactions.appendChild(chip);
    }

    const tools = document.createElement("div");
    tools.className = "message-tools";
    for (const emoji of ["+1", "fire", "laugh"]) {
      const button = document.createElement("button");
      button.className = "message-tool";
      button.type = "button";
      button.dataset.messageId = message.id;
      button.dataset.emoji = emoji;
      button.textContent = emoji;
      tools.appendChild(button);
    }
    if (state.user?.id === message.authorId) {
      const edit = document.createElement("button");
      edit.className = "message-tool";
      edit.type = "button";
      edit.dataset.editMessageId = message.id;
      edit.textContent = "Edit";
      tools.appendChild(edit);

      const del = document.createElement("button");
      del.className = "message-tool delete";
      del.type = "button";
      del.dataset.deleteMessageId = message.id;
      del.textContent = "Delete";
      tools.appendChild(del);
    }

    main.append(meta, content, reactions, tools);
    row.append(avatar, main);
    els.messagePane.appendChild(row);
  }

  if (nearBottom) {
    els.messagePane.scrollTop = els.messagePane.scrollHeight;
  }
};

const socialRow = ({ user, subtitle, actions = "" }) => `
  <div class="social-item ${actions ? "with-actions" : ""}">
    <div class="account-avatar">${escapeHtml(initials(user.username))}</div>
    <div>
      <strong>${escapeHtml(user.username)}</strong>
      <span>${escapeHtml(subtitle)}</span>
    </div>
    ${actions}
  </div>
`;

const renderSocialList = (element, items, emptyText, rowFactory) => {
  element.innerHTML = "";
  if (items.length === 0) {
    const empty = document.createElement("div");
    empty.className = "empty-list";
    empty.textContent = emptyText;
    element.appendChild(empty);
    return;
  }

  for (const item of items) {
    element.insertAdjacentHTML("beforeend", rowFactory(item));
  }
};

const renderFriends = () => {
  els.friendsButton.textContent =
    state.friends.inbound.length > 0 ? `Friends (${state.friends.inbound.length})` : "Friends";

  renderSocialList(els.friendsList, state.friends.friends, "No friends yet.", (user) =>
    socialRow({
      user,
      subtitle: "Friend",
      actions: `
        <div class="social-actions">
          <button data-dm-username="${escapeHtml(user.username)}">DM</button>
          <button class="deny" data-remove-friend-id="${user.id}">Remove</button>
        </div>
      `
    })
  );

  renderSocialList(
    els.inboundRequestsList,
    state.friends.inbound,
    "No inbound requests.",
    (request) =>
      socialRow({
        user: request.user,
        subtitle: "Wants to be friends",
        actions: `
          <div class="social-actions">
            <button class="accept" data-friend-action="accept" data-request-id="${request.id}">Accept</button>
            <button class="deny" data-friend-action="deny" data-request-id="${request.id}">Deny</button>
          </div>
        `
      })
  );

  renderSocialList(
    els.outboundRequestsList,
    state.friends.outbound,
    "No outbound requests.",
    (request) => socialRow({ user: request.user, subtitle: "Pending" })
  );
};

const render = () => {
  renderUser();
  renderServers();
  renderChannels();
  renderDmList();
  renderHeader();
  renderMessages();
  renderFriends();
};

const requireToken = () => {
  if (!state.token) {
    throw new Error("Log in first.");
  }
};

const loadServers = async () => {
  requireToken();
  const payload = await window.hangout.servers(state.apiBaseUrl, state.token);
  state.servers = payload.servers || [];
  if (!state.activeServerId && state.servers.length > 0) {
    state.activeServerId = state.servers[0].id;
  }
};

const loadChannels = async () => {
  requireToken();
  if (!state.activeServerId) {
    state.channels = [];
    return;
  }

  const payload = await window.hangout.channels(state.apiBaseUrl, state.token, state.activeServerId);
  state.channels = payload.channels || [];
  if (!state.activeChannelId && state.channels.length > 0) {
    state.activeChannelId = state.channels[0].id;
  }
};

const loadMessages = async () => {
  requireToken();
  if (state.activeMode === "dm") {
    if (!state.activeConversationId) {
      state.messages = [];
      return;
    }
    const payload = await window.hangout.dmMessages(
      state.apiBaseUrl,
      state.token,
      state.activeConversationId,
      80
    );
    state.messages = payload.messages || [];
    return;
  }

  if (!state.activeChannelId) {
    state.messages = [];
    return;
  }

  const payload = await window.hangout.messages(state.apiBaseUrl, state.token, state.activeChannelId, 80);
  state.messages = payload.messages || [];
};

const loadFriends = async () => {
  requireToken();
  state.friends = await window.hangout.friends(state.apiBaseUrl, state.token);
};

const loadDmConversations = async () => {
  requireToken();
  const payload = await window.hangout.dmConversations(state.apiBaseUrl, state.token);
  state.dmConversations = payload.conversations || [];
};

const refreshAll = async ({ quiet = false } = {}) => {
  try {
    await loadServers();
    await loadChannels();
    await loadMessages();
    await loadFriends();
    await loadDmConversations();
    setConnected(true);
    render();
    if (!quiet) {
      showToast("Synced with backend.");
    }
  } catch (error) {
    setConnected(false);
    if (!quiet) {
      showToast(error.message);
    }
    render();
  }
};

const selectServer = async (serverId) => {
  state.activeMode = "channel";
  state.activeConversationId = null;
  state.activeServerId = serverId;
  state.activeChannelId = null;
  state.messages = [];
  render();
  try {
    await loadChannels();
    await loadMessages();
    setConnected(true);
  } catch (error) {
    setConnected(false);
    showToast(error.message);
  }
  render();
};

const selectChannel = async (channelId) => {
  state.activeMode = "channel";
  state.activeConversationId = null;
  state.activeChannelId = channelId;
  state.messages = [];
  render();
  try {
    await loadMessages();
    setConnected(true);
  } catch (error) {
    setConnected(false);
    showToast(error.message);
  }
  render();
};

const selectDm = async (conversationId) => {
  state.activeMode = "dm";
  state.activeConversationId = conversationId;
  state.messages = [];
  render();
  try {
    await loadMessages();
    setConnected(true);
  } catch (error) {
    setConnected(false);
    showToast(error.message);
  }
  render();
};

const login = async (username) => {
  const result = await window.hangout.login(state.apiBaseUrl, username);
  state.token = result.session.token;
  state.user = result.user;
  state.activeServerId = null;
  state.activeChannelId = null;

  localStorage.setItem("hangout.token", state.token);
  localStorage.setItem("hangout.user", JSON.stringify(state.user));

  await refreshAll({ quiet: true });
  showToast(`Logged in as ${state.user.username}.`);
};

const logout = () => {
  window.clearInterval(state.pollTimer);
  localStorage.removeItem("hangout.token");
  localStorage.removeItem("hangout.user");

  state.token = "";
  state.user = null;
  state.servers = [];
  state.channels = [];
  state.messages = [];
  state.friends = { friends: [], inbound: [], outbound: [] };
  state.dmConversations = [];
  state.activeMode = "channel";
  state.activeConversationId = null;
  state.activeServerId = null;
  state.activeChannelId = null;

  els.settingsOverlay.classList.add("hidden");
  els.serverSettingsOverlay.classList.add("hidden");
  els.friendsOverlay.classList.add("hidden");
  els.serverPlusMenu.classList.add("hidden");
  els.usernameInput.value = "";
  setConnected(false);
  render();
  startPolling();
  showToast("Logged out.");
};

const openNameModal = ({ title, kicker, label, placeholder, helpText, submitText, initialValue = "" }) => {
  if (pendingNameModal) {
    pendingNameModal.resolve(null);
  }

  els.nameModalTitle.textContent = title;
  els.nameModalKicker.textContent = kicker;
  els.nameInputLabel.textContent = label;
  els.nameInput.placeholder = placeholder;
  els.nameInput.value = initialValue;
  els.nameHelpText.textContent = helpText;
  els.nameSubmitButton.textContent = submitText;
  els.nameOverlay.classList.remove("hidden");

  window.setTimeout(() => {
    els.nameInput.focus();
    const length = els.nameInput.value.length;
    els.nameInput.setSelectionRange(length, length);
  }, 0);

  return new Promise((resolve) => {
    pendingNameModal = { resolve };
  });
};

const closeNameModal = (value = null) => {
  if (!pendingNameModal) {
    els.nameOverlay.classList.add("hidden");
    return;
  }

  const { resolve } = pendingNameModal;
  pendingNameModal = null;
  els.nameOverlay.classList.add("hidden");
  resolve(value);
};

const createServer = async () => {
  els.serverPlusMenu.classList.add("hidden");
  if (!state.token) {
    showToast("Log in first.");
    return;
  }

  const name = await openNameModal({
    title: "Create Server",
    kicker: "Servers",
    label: "Server name",
    placeholder: "Study Group",
    helpText: "Use 2-48 characters. A #general channel will be created automatically.",
    submitText: "Create server"
  });
  if (!name) {
    return;
  }

  try {
    const result = await window.hangout.createServer(state.apiBaseUrl, state.token, name);
    state.activeServerId = result.server.id;
    state.activeChannelId = null;
    await refreshAll({ quiet: true });
    showToast(`Created ${result.server.name}.`);
  } catch (error) {
    showToast(error.message);
  }
};

const joinServer = async () => {
  els.serverPlusMenu.classList.add("hidden");
  if (!state.token) {
    showToast("Log in first.");
    return;
  }

  const inviteCode = await openNameModal({
    title: "Join Server",
    kicker: "Servers",
    label: "Invite code",
    placeholder: "HANGOUT1",
    helpText: "Paste the 8-character invite code you were given.",
    submitText: "Join server"
  });
  if (!inviteCode) {
    return;
  }

  try {
    const result = await window.hangout.joinServer(state.apiBaseUrl, state.token, inviteCode);
    state.activeServerId = result.server.id;
    state.activeChannelId = null;
    await refreshAll({ quiet: true });
    showToast(`Joined ${result.server.name}.`);
  } catch (error) {
    showToast(error.message);
  }
};

const createChannel = async () => {
  if (!state.token) {
    showToast("Log in first.");
    return;
  }
  if (!state.activeServerId) {
    showToast("Choose a server first.");
    return;
  }

  const name = await openNameModal({
    title: "Create Channel",
    kicker: activeServer()?.name || "Channels",
    label: "Channel name",
    placeholder: "homework",
    helpText: "Use 2-32 lowercase letters, numbers, underscores, or dashes. Spaces become dashes.",
    submitText: "Create channel"
  });
  if (!name) {
    return;
  }

  try {
    const result = await window.hangout.createChannel(
      state.apiBaseUrl,
      state.token,
      state.activeServerId,
      name
    );
    state.activeChannelId = result.channel.id;
    await refreshAll({ quiet: true });
    showToast(`Created #${result.channel.name}.`);
  } catch (error) {
    showToast(error.message);
  }
};

const moveChannel = async (channelId, direction) => {
  const sorted = [...state.channels].sort((a, b) => (a.position ?? 0) - (b.position ?? 0));
  const index = sorted.findIndex((channel) => channel.id === channelId);
  const swapIndex = index + direction;
  if (index < 0 || swapIndex < 0 || swapIndex >= sorted.length) {
    return;
  }

  const current = sorted[index];
  const other = sorted[swapIndex];
  try {
    await window.hangout.setChannelPosition(state.apiBaseUrl, state.token, current.id, other.position ?? swapIndex);
    await window.hangout.setChannelPosition(state.apiBaseUrl, state.token, other.id, current.position ?? index);
    await loadChannels();
    renderChannels();
  } catch (error) {
    showToast(error.message);
  }
};

const openServerSettings = async () => {
  const server = activeServer();
  if (!state.token || !server) {
    showToast("Choose a server first.");
    return;
  }

  els.serverSettingsTitle.textContent = server.name;
  els.inviteCodeText.textContent = "Loading";
  els.serverSettingsOverlay.classList.remove("hidden");

  try {
    const result = await window.hangout.serverInvite(state.apiBaseUrl, state.token, server.id);
    els.inviteCodeText.textContent = result.invite.inviteCode;
    const members = await window.hangout.serverMembers(state.apiBaseUrl, state.token, server.id);
    els.serverMembersList.innerHTML = "";
    for (const member of members.members || []) {
      els.serverMembersList.insertAdjacentHTML(
        "beforeend",
        socialRow({ user: member.user, subtitle: `${member.role} · ${member.user.userStatus || "online"}` })
      );
    }
  } catch (error) {
    els.inviteCodeText.textContent = "Unavailable";
    showToast(error.message);
  }
};

const openFriends = async () => {
  if (!state.token) {
    showToast("Log in first.");
    return;
  }

  els.friendsOverlay.classList.remove("hidden");
  try {
    await loadFriends();
    renderFriends();
  } catch (error) {
    showToast(error.message);
  }
};

const sendFriendRequest = async (username) => {
  await window.hangout.sendFriendRequest(state.apiBaseUrl, state.token, username);
  await loadFriends();
  renderFriends();
  showToast(`Friend request sent to ${username}.`);
};

const openDmWithUser = async (username) => {
  const result = await window.hangout.openDm(state.apiBaseUrl, state.token, username);
  await loadDmConversations();
  state.activeMode = "dm";
  state.activeConversationId = result.conversation.id;
  els.friendsOverlay.classList.add("hidden");
  await loadMessages();
  render();
  showToast(`Opened DM with ${username}.`);
};

const respondToFriendRequest = async (action, requestId) => {
  if (action === "accept") {
    await window.hangout.acceptFriendRequest(state.apiBaseUrl, state.token, requestId);
    showToast("Friend request accepted.");
  } else {
    await window.hangout.denyFriendRequest(state.apiBaseUrl, state.token, requestId);
    showToast("Friend request denied.");
  }
  await loadFriends();
  renderFriends();
};

els.loginForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  const username = els.usernameInput.value.trim();
  if (!username) {
    showToast("Enter a username.");
    return;
  }

  try {
    await login(username);
    setConnected(true);
  } catch (error) {
    setConnected(false);
    showToast(error.message);
  }
});

els.settingsButton.addEventListener("click", () => {
  els.settingsOverlay.classList.remove("hidden");
});

els.closeSettingsButton.addEventListener("click", () => {
  els.settingsOverlay.classList.add("hidden");
});

els.settingsOverlay.addEventListener("click", (event) => {
  if (event.target === els.settingsOverlay) {
    els.settingsOverlay.classList.add("hidden");
  }
});

els.logoutButton.addEventListener("click", logout);
els.serverPlusButton.addEventListener("click", (event) => {
  event.stopPropagation();
  if (!state.token) {
    showToast("Log in first.");
    return;
  }
  els.serverPlusMenu.classList.toggle("hidden");
});
els.createServerButton.addEventListener("click", createServer);
els.joinServerButton.addEventListener("click", joinServer);
els.createChannelButton.addEventListener("click", createChannel);
els.channelList.addEventListener("click", (event) => {
  const button = event.target.closest("[data-channel-move]");
  if (!button) {
    return;
  }
  event.stopPropagation();
  moveChannel(Number(button.dataset.channelMove), Number(button.dataset.direction));
});
els.serverSettingsButton.addEventListener("click", openServerSettings);

els.closeServerSettingsButton.addEventListener("click", () => {
  els.serverSettingsOverlay.classList.add("hidden");
});

els.serverSettingsOverlay.addEventListener("click", (event) => {
  if (event.target === els.serverSettingsOverlay) {
    els.serverSettingsOverlay.classList.add("hidden");
  }
});

els.copyInviteButton.addEventListener("click", async () => {
  const code = els.inviteCodeText.textContent.trim();
  if (!code || code === "Loading" || code === "Unavailable") {
    showToast("Invite code is not ready yet.");
    return;
  }

  await window.hangout.copyText(code);
  showToast("Invite code copied.");
});

els.regenerateInviteButton.addEventListener("click", async () => {
  const server = activeServer();
  if (!server) {
    showToast("Choose a server first.");
    return;
  }
  try {
    const result = await window.hangout.regenerateServerInvite(state.apiBaseUrl, state.token, server.id);
    els.inviteCodeText.textContent = result.invite.inviteCode;
    showToast("Invite code regenerated.");
  } catch (error) {
    showToast(error.message);
  }
});

els.profileForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  try {
    const result = await window.hangout.updateProfile(state.apiBaseUrl, state.token, {
      displayName: els.displayNameInput.value.trim(),
      statusText: els.statusTextInput.value.trim(),
      userStatus: els.userStatusInput.value,
      avatarColor: els.avatarColorInput.value.trim()
    });
    state.user = result.user;
    localStorage.setItem("hangout.user", JSON.stringify(state.user));
    renderUser();
    showToast("Profile saved.");
  } catch (error) {
    showToast(error.message);
  }
});

els.friendsButton.addEventListener("click", openFriends);

els.closeFriendsButton.addEventListener("click", () => {
  els.friendsOverlay.classList.add("hidden");
});

els.friendsOverlay.addEventListener("click", (event) => {
  if (event.target === els.friendsOverlay) {
    els.friendsOverlay.classList.add("hidden");
  }
});

els.friendRequestForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  const username = els.friendUsernameInput.value.trim();
  if (!username) {
    showToast("Enter a username.");
    return;
  }

  try {
    await sendFriendRequest(username);
    els.friendUsernameInput.value = "";
  } catch (error) {
    showToast(error.message);
  }
});

els.inboundRequestsList.addEventListener("click", async (event) => {
  const button = event.target.closest("[data-friend-action]");
  if (!button) {
    return;
  }

  try {
    await respondToFriendRequest(button.dataset.friendAction, Number(button.dataset.requestId));
  } catch (error) {
    showToast(error.message);
  }
});

els.friendsList.addEventListener("click", async (event) => {
  const remove = event.target.closest("[data-remove-friend-id]");
  if (remove) {
    try {
      await window.hangout.removeFriend(state.apiBaseUrl, state.token, Number(remove.dataset.removeFriendId));
      await loadFriends();
      renderFriends();
      showToast("Friend removed.");
    } catch (error) {
      showToast(error.message);
    }
    return;
  }

  const button = event.target.closest("[data-dm-username]");
  if (!button) {
    return;
  }
  try {
    await openDmWithUser(button.dataset.dmUsername);
  } catch (error) {
    showToast(error.message);
  }
});

document.addEventListener("click", (event) => {
  if (!event.target.closest(".plus-wrap")) {
    els.serverPlusMenu.classList.add("hidden");
  }
  if (!event.target.closest("[data-profile-username]") && !event.target.closest("#profilePopover")) {
    els.profilePopover.classList.add("hidden");
  }
});

els.closeNameButton.addEventListener("click", () => closeNameModal());

els.nameOverlay.addEventListener("click", (event) => {
  if (event.target === els.nameOverlay) {
    closeNameModal();
  }
});

els.nameForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const value = els.nameInput.value.trim();
  if (!value) {
    showToast("Enter a name first.");
    els.nameInput.focus();
    return;
  }
  closeNameModal(value);
});

els.messageForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  const content = els.messageInput.value.trim();
  const hasTarget =
    (state.activeMode === "channel" && state.activeChannelId) ||
    (state.activeMode === "dm" && state.activeConversationId);
  if (!content || !hasTarget) {
    return;
  }

  els.messageInput.value = "";
  try {
    const result =
      state.activeMode === "dm"
        ? await window.hangout.sendDmMessage(
            state.apiBaseUrl,
            state.token,
            state.activeConversationId,
            content
          )
        : await window.hangout.sendMessage(
            state.apiBaseUrl,
            state.token,
            state.activeChannelId,
            content
          );
    state.messages = [...state.messages, result.message];
    setConnected(true);
    renderMessages();
  } catch (error) {
    setConnected(false);
    els.messageInput.value = content;
    showToast(error.message);
  }
});

els.refreshButton.addEventListener("click", () => refreshAll());
els.messageSearchInput.addEventListener("keydown", async (event) => {
  if (event.key !== "Enter") {
    return;
  }
  const query = els.messageSearchInput.value.trim();
  if (state.activeMode !== "channel" || !state.activeChannelId) {
    return;
  }
  try {
    if (!query) {
      await loadMessages();
      renderMessages();
      showToast("Search cleared.");
      return;
    }
    const payload = await window.hangout.searchMessages(
      state.apiBaseUrl,
      state.token,
      state.activeChannelId,
      query,
      80
    );
    state.messages = payload.messages || [];
    renderMessages();
    showToast("Search results loaded.");
  } catch (error) {
    showToast(error.message);
  }
});
els.refreshDmsButton.addEventListener("click", async () => {
  try {
    await loadDmConversations();
    renderDmList();
    showToast("Direct messages synced.");
  } catch (error) {
    showToast(error.message);
  }
});

els.messagePane.addEventListener("click", async (event) => {
  const profileTarget = event.target.closest("[data-profile-username]");
  if (profileTarget) {
    const username = profileTarget.dataset.profileUsername;
    els.popoverAvatar.textContent = initials(username);
    els.popoverName.textContent = username;
    els.popoverMeta.textContent = username === state.user?.username ? "You" : "Chat member";
    const rect = profileTarget.getBoundingClientRect();
    els.profilePopover.style.left = `${Math.min(rect.left, window.innerWidth - 240)}px`;
    els.profilePopover.style.top = `${Math.min(rect.bottom + 8, window.innerHeight - 90)}px`;
    els.profilePopover.classList.remove("hidden");
    return;
  }

  const deleteButton = event.target.closest("[data-delete-message-id]");
  const editButton = event.target.closest("[data-edit-message-id]");
  const reactionButton = event.target.closest("[data-emoji][data-message-id]");

  try {
    if (editButton) {
      const messageId = Number(editButton.dataset.editMessageId);
      const current = state.messages.find((message) => message.id === messageId);
      const content = await openNameModal({
        title: "Edit Message",
        kicker: "Messages",
        label: "Message",
        placeholder: current?.content || "",
        initialValue: current?.content || "",
        helpText: "Update your message text.",
        submitText: "Save edit"
      });
      if (!content) {
        return;
      }
      const result =
        state.activeMode === "dm"
          ? await window.hangout.editDmMessage(state.apiBaseUrl, state.token, messageId, content)
          : await window.hangout.editMessage(state.apiBaseUrl, state.token, messageId, content);
      if (result?.message) {
        state.messages = state.messages.map((message) =>
          message.id === messageId ? result.message : message
        );
        renderMessages();
      }
      return;
    }

    if (deleteButton) {
      const messageId = Number(deleteButton.dataset.deleteMessageId);
      if (state.activeMode === "dm") {
        await window.hangout.deleteDmMessage(state.apiBaseUrl, state.token, messageId);
      } else {
        await window.hangout.deleteMessage(state.apiBaseUrl, state.token, messageId);
      }
      state.messages = state.messages.filter((message) => message.id !== messageId);
      renderMessages();
      showToast("Message deleted.");
      return;
    }

    if (reactionButton) {
      const messageId = Number(reactionButton.dataset.messageId);
      const emoji = reactionButton.dataset.emoji;
      const result =
        state.activeMode === "dm"
          ? await window.hangout.reactToDmMessage(state.apiBaseUrl, state.token, messageId, emoji)
          : await window.hangout.reactToMessage(state.apiBaseUrl, state.token, messageId, emoji);
      state.messages = state.messages.map((message) =>
        message.id === messageId ? { ...message, reactions: result.reactions } : message
      );
      renderMessages();
    }
  } catch (error) {
    showToast(error.message);
  }
});

const startPolling = () => {
  window.clearInterval(state.pollTimer);
  state.pollTimer = window.setInterval(() => {
    if (state.token) {
      window.hangout.heartbeat(state.apiBaseUrl, state.token).catch(() => {});
      loadFriends()
        .then(() => renderFriends())
        .catch(() => {});
    }

    const hasTarget =
      state.token &&
      ((state.activeMode === "channel" && state.activeChannelId) ||
        (state.activeMode === "dm" && state.activeConversationId));
    if (hasTarget) {
      loadMessages()
        .then(() => {
          setConnected(true);
          renderMessages();
        })
        .catch(() => setConnected(false));
    }
  }, 2500);
};

const boot = async () => {
  window.hangout.onUpdateStatus(renderUpdateStatus);
  renderUpdateStatus(await window.hangout.getUpdateState());

  const config = await window.hangout.getConfig();
  state.apiBaseUrl = config.apiBaseUrl;
  els.apiBaseText.textContent = state.apiBaseUrl;
  renderUser();

  if (state.token) {
    try {
      const me = await window.hangout.me(state.apiBaseUrl, state.token);
      state.user = me.user;
      localStorage.setItem("hangout.user", JSON.stringify(state.user));
      await refreshAll({ quiet: true });
      setConnected(true);
    } catch {
      localStorage.removeItem("hangout.token");
      localStorage.removeItem("hangout.user");
      state.token = "";
      state.user = null;
      setConnected(false);
      render();
    }
  } else {
    render();
  }

  startPolling();
};

boot().catch((error) => {
  setConnected(false);
  showToast(error.message);
  render();
});
