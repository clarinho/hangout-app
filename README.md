# Hangout Backend

This repo contains a serious prototype backend for a Discord-style chat app, written in C++ with a clean path to an Electron desktop client.

## Why this architecture

The backend is split into three layers:

1. `transport`: HTTP endpoints, JSON parsing, auth header handling, response formatting.
2. `application`: use-case logic such as login, list channels, load messages, send message.
3. `storage`: SQLite schema, migrations, and repository queries.

That separation keeps Electron-specific concerns out of the core. A future WebSocket gateway can sit beside the current HTTP API and call the same services.

## Protocol choice

For this project, the best choice is **HTTP + WebSockets**.

- Use HTTP for request/response work:
  - login
  - fetch servers
  - fetch channels
  - load message history
- Use WebSockets later for push events:
  - new messages
  - presence
  - typing indicators
  - unread updates

Why not raw TCP?

- Electron already speaks HTTP and WebSocket naturally.
- Browsers, devtools, proxies, auth headers, and JSON payloads fit this model well.
- Raw TCP would force you to invent framing, reconnect behavior, request correlation, and custom client plumbing for no real benefit at this stage.

Why not only HTTP?

- Polling works for a prototype, but message delivery, presence, and typing become awkward quickly.
- WebSockets give Electron a stable realtime channel without coupling the core backend to the frontend framework.

The current MVP implements the HTTP API and keeps a `MessageEventBus` interface in the application layer so realtime transport can be added later without rewriting business logic.

## Core data model

- `User`
  - `id`
  - `username`
  - `created_at_ms`
- `Session`
  - `id`
  - `user_id`
  - `token`
  - `created_at_ms`
  - `expires_at_ms`
- `Server`
  - `id`
  - `name`
  - `created_at_ms`
- `Channel`
  - `id`
  - `server_id`
  - `name`
  - `created_at_ms`
- `Message`
  - `id`
  - `channel_id`
  - `author_id`
  - `content`
  - `created_at_ms`

Supporting table:

- `server_memberships`
  - `(user_id, server_id)`
  - used to model which servers a user can access

## What to build first

1. Persistence and schema
2. Mock auth and session handling
3. Read APIs for servers, channels, message history
4. Send-message API
5. Realtime event transport
6. Presence, DMs, unread state, editing, permissions

That order gives you a believable chat backend quickly, while preserving a stable API boundary for Electron.

## Folder structure

```text
.
|-- CMakeLists.txt
|-- README.md
|-- docs/
|   `-- architecture.md
|-- electron/
|   |-- index.html
|   |-- main.js
|   |-- preload.js
|   |-- renderer.js
|   `-- styles.css
|-- include/
|   `-- hangout/
|       |-- application/
|       |   |-- auth_service.hpp
|       |   |-- chat_service.hpp
|       |   `-- event_bus.hpp
|       |-- domain/
|       |   |-- errors.hpp
|       |   `-- models.hpp
|       |-- storage/
|       |   |-- database.hpp
|       |   `-- repositories.hpp
|       `-- transport/
|           `-- http_server.hpp
|-- scripts/
|   `-- smoke_test.ps1
|-- src/
|   |-- application/
|   |   |-- auth_service.cpp
|   |   `-- chat_service.cpp
|   |-- storage/
|   |   |-- database.cpp
|   |   `-- repositories.cpp
|   |-- transport/
|   |   `-- http_server.cpp
|   `-- main.cpp
`-- third_party/
    |-- httplib.h
    `-- json.hpp
```

## Major backend modules

- `Database`: opens SQLite, runs schema migrations, seeds demo data.
- `Repositories`: small persistence adapters for users, sessions, servers, channels, and messages.
- `AuthService`: mock login and bearer-session validation.
- `ChatService`: chat use cases and validation.
- `HttpServer`: HTTP routing and JSON API surface.
- `MessageEventBus`: abstraction for future realtime delivery.
- `electron`: lightweight desktop shell, secure preload API bridge, and responsive chat UI.

## Dependencies

The dependency set is intentionally small:

- `sqlite3`: worth it for persistent storage in a serious prototype
- `cpp-httplib`: worth it for a tiny self-contained HTTP server
- `nlohmann/json`: worth it for readable JSON handling

I would not add a larger framework yet unless you decide you want built-in WebSockets immediately.

## Build

### Prerequisites

On Windows with MSYS2 UCRT64, install:

```powershell
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-sqlite3
```

### Configure and compile

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

## Run

```powershell
.\build\hangout_backend.exe
```

Optional arguments:

```powershell
.\build\hangout_backend.exe --host 127.0.0.1 --port 8080 --db data/hangout.db
```

## Run the Electron client

Install the frontend dependency once:

```powershell
npm install
```

Start the C++ backend first:

```powershell
.\build\hangout_backend.exe
```

Then launch the desktop client in a second terminal:

```powershell
npm run frontend
```

The Electron client reads from `http://127.0.0.1:8080` by default. To point it at another backend:

```powershell
$env:HANGOUT_API_BASE_URL = "http://127.0.0.1:8081"
npm run frontend
```

The current client uses HTTP for login, loading servers/channels/messages, and sending messages. It also polls the active channel lightly so it feels live until the WebSocket gateway is added.

## Package The App For Friends

For friends on different networks, do **not** run a separate backend on each computer. Run one public backend on your DigitalOcean droplet, then package the Electron desktop client so everyone connects to that backend.

Architecture:

```text
Friend's PC -> Electron app -> https://your-domain.com -> Nginx -> C++ backend -> SQLite
```

## Installer Website

The project includes a static download page you can upload to Hostinger Personal hosting. Build it from the repo root:

```powershell
.\build-download-page
```

That creates `website-dist/` with a big download button and the newest valid installer copied to:

```text
website-dist/downloads/HangoutSetup.exe
```

Upload the contents of `website-dist/` to Hostinger File Manager:

```text
public_html/
```

or:

```text
public_html/download/
```

If you upload to `public_html/download/`, users can visit:

```text
https://your-domain.com/download/
```

Do not commit `website-dist/`; it contains the large installer and is ignored by Git.

### 1. Configure production URLs

Edit `package.json` before building the installer:

```json
"hangout": {
  "apiBaseUrl": "https://your-domain.com",
  "updateFeedUrl": "https://your-domain.com/updates/"
}
```

Also update the `build.publish[0].url` value to the same update URL:

```json
"publish": [
  {
    "provider": "generic",
    "url": "https://your-domain.com/updates/"
  }
]
```

Local development can still override the API URL:

```powershell
$env:HANGOUT_API_BASE_URL = "http://127.0.0.1:8080"
npm run frontend
```

### 2. Build a Windows installer

```powershell
npm install
npm run dist
```

Installer output goes to:

```text
release/
```

Give your friend the generated `Hangout Setup ... .exe`.

### 3. Auto-update flow

The packaged app checks the update feed on launch. It downloads updates automatically and installs them when the app quits.

The normal release command is:

```powershell
.\update
```

That command bumps the patch version, builds the NSIS installer, uploads `latest.yml`, the matching installer, and the matching blockmap to the droplet, then verifies the public update feed.

Optional flags:

```powershell
.\update -Droplet 45.55.245.3 -RemoteUser root
.\update -SkipVersionBump
```

To publish an update:

1. Bump the version in `package.json`.
2. Run:

```powershell
npm run dist
```

3. Upload these generated files from `release/` to the droplet's update directory:

```text
latest.yml
Hangout Setup <version>.exe
*.blockmap
```

You can use:

```powershell
.\deploy\upload-updates.ps1 -Droplet YOUR_DROPLET_IP
```

The Nginx config in `deploy/nginx-hangout.conf` serves those files from:

```text
/var/www/hangout-updates/
```

## DigitalOcean Deployment

Use a domain if possible, for example `chat.example.com`. HTTPS is strongly recommended because login/session tokens travel between the app and backend.

### 1. Prepare the droplet

On the droplet:

```bash
git clone <your-repo-url> /opt/hangout-src
cd /opt/hangout-src
chmod +x deploy/ubuntu-setup.sh
./deploy/ubuntu-setup.sh
```

### 2. Build the backend on Ubuntu

```bash
cd /opt/hangout-src
cmake -S . -B build
cmake --build build
sudo cp build/hangout_backend /opt/hangout/hangout_backend
sudo chown -R hangout:hangout /opt/hangout /var/lib/hangout
```

### 3. Install the backend service

```bash
sudo cp deploy/hangout-backend.service /etc/systemd/system/hangout-backend.service
sudo systemctl daemon-reload
sudo systemctl enable --now hangout-backend
sudo systemctl status hangout-backend
```

The backend listens privately on:

```text
127.0.0.1:8080
```

### 4. Configure Nginx

Copy and edit the Nginx config:

```bash
sudo cp deploy/nginx-hangout.conf /etc/nginx/sites-available/hangout
sudo nano /etc/nginx/sites-available/hangout
sudo ln -s /etc/nginx/sites-available/hangout /etc/nginx/sites-enabled/hangout
sudo nginx -t
sudo systemctl reload nginx
```

Then enable HTTPS:

```bash
sudo certbot --nginx -d your-domain.com
```

### 5. Test from your PC

```powershell
Invoke-RestMethod https://your-domain.com/healthz
```

If that returns `{ "status": "ok" }`, packaged clients can exchange messages across different networks.

### Test two desktop instances

Start the backend once:

```powershell
.\build\hangout_backend.exe
```

Then launch the Electron client twice, from two terminals:

```powershell
npm run frontend
```

```powershell
npm run frontend
```

Log in as two different users, open the same channel, and send a message from one window. The other window will pick it up on the next lightweight poll. True push delivery is the next WebSocket milestone.

### Settings and profile

After login, the username form is hidden. Use `Settings` in the left sidebar to view the current profile/session and log out. You must log out before signing in as a different user.

### Friends

Use `Friends` in the top bar to open the friends window.

- Send a request by username.
- Sent requests appear under `Outbound Requests`.
- Received requests appear under `Inbound Requests`.
- If the receiver denies the request, it disappears for both users.
- If the receiver accepts the request, both users appear in each other's `Friends` list.
- Use `DM` beside a friend to open a direct-message conversation.

### Messages

Message rows support reactions and author-only deletion.

- Click a reaction button on any visible server or direct message to toggle your reaction.
- If you authored a message, use `Delete` on that message row to remove it.
- If you authored a server-channel message, use `Edit`; edited messages show an `Edited` marker.
- If you authored a direct message, use `Edit`; edited messages show an `(Edited)` marker.
- Use the top-bar search box and press Enter to search the current server channel.
- Press Enter in an empty search box to reload the normal channel history.
- Click a message author/avatar to open a small profile popover.
- Direct messages are available between accepted friends.

### Server Management

`Server Settings` shows the invite code and member list. Server creators are marked as `owner`; joined users are `member`. Owners can regenerate invite codes.

Channel rows expose `Up` and `Down` controls on hover. Server owners can use them to change channel order.

### Profile And Status

Use `Settings` to update display name, status text, user status, and avatar color. Member/friend surfaces use the profile fields returned by the backend.

### Create servers and channels

Use the `+` beside `Servers` to create or join a server. Each new server gets a default `general` channel.

To invite another user, select a server, open `Server Settings`, and copy the invite code. The other user can choose `+` beside `Servers`, select `Join server`, and paste that code.

Use `New` beside `Channels` to create a channel in the selected server.

## API summary

- `POST /api/v1/auth/login`
  - body: `{ "username": "alice" }`
- `GET /api/v1/me`
- `POST /api/v1/me/profile`
  - body: `{ "displayName": "Alice", "statusText": "Studying", "userStatus": "idle", "avatarColor": "#38d8d0" }`
- `POST /api/v1/me/heartbeat`
- `GET /api/v1/friends`
- `POST /api/v1/friends/requests`
  - body: `{ "username": "bob" }`
- `POST /api/v1/friends/requests/{requestId}/accept`
- `POST /api/v1/friends/requests/{requestId}/deny`
- `DELETE /api/v1/friends/{friendId}`
- `GET /api/v1/dms`
- `POST /api/v1/dms`
  - body: `{ "username": "bob" }`
- `GET /api/v1/dms/{conversationId}/messages?limit=50`
- `POST /api/v1/dms/{conversationId}/messages`
  - body: `{ "content": "hello" }`
- `POST /api/v1/dm-messages/{messageId}/reactions`
  - body: `{ "emoji": "+1" }`
- `DELETE /api/v1/dm-messages/{messageId}`
- `GET /api/v1/servers`
- `POST /api/v1/servers`
  - body: `{ "name": "Study Group" }`
- `POST /api/v1/servers/join`
  - body: `{ "inviteCode": "ABCD1234" }`
- `GET /api/v1/servers/{serverId}/invite`
- `GET /api/v1/servers/{serverId}/channels`
- `POST /api/v1/servers/{serverId}/channels`
  - body: `{ "name": "homework" }`
- `GET /api/v1/servers/{serverId}/members`
- `POST /api/v1/servers/{serverId}/invite/regenerate`
- `POST /api/v1/channels/{channelId}/position`
  - body: `{ "position": 2 }`
- `GET /api/v1/channels/{channelId}/messages?limit=50`
- `GET /api/v1/channels/{channelId}/messages?q=search`
- `POST /api/v1/channels/{channelId}/messages`
  - body: `{ "content": "hello world" }`
- `POST /api/v1/messages/{messageId}/edit`
  - body: `{ "content": "updated text" }`
- `POST /api/v1/messages/{messageId}/reactions`
  - body: `{ "emoji": "+1" }`
- `DELETE /api/v1/messages/{messageId}`

Protected routes expect:

```text
Authorization: Bearer <token>
```

## Electron integration later

Electron should treat this backend as a local service with a stable JSON API:

1. Renderer or preload calls HTTP endpoints for login and initial data loading.
2. The future desktop client opens one WebSocket connection after login.
3. The backend pushes message-created, presence, and typing events over that socket.
4. The frontend updates local state stores from those events.

This keeps the Electron app thin and prevents frontend code from knowing anything about SQLite or internal service wiring.

## Next milestones

1. Add a WebSocket gateway backed by the existing `MessageEventBus`.
2. Add server membership invitations and per-user server lists beyond the seeded demo server.
3. Add direct-message conversations with a separate `conversations` model.
4. Add edit/delete message flows and audit-friendly timestamps.
5. Add unread tracking and presence state.
6. Add tests around services and repository edge cases.
