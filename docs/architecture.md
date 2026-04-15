# Architecture And Implementation Plan

## 1. Backend architecture

Use a layered monolith:

- **Transport layer**
  - HTTP API now
  - WebSocket gateway later
  - knows about JSON, headers, route params, status codes
- **Application layer**
  - chat use cases
  - auth/session logic
  - validation and access checks
- **Storage layer**
  - SQLite schema
  - repository queries
  - no HTTP concerns

This is the right shape for a student prototype because it stays simple enough to finish, but it already separates concerns the way a larger system would.

## 2. Transport recommendation

Recommended: **HTTP + WebSockets**

- HTTP handles login and data fetches cleanly.
- WebSockets are the natural realtime companion for Electron.
- Raw TCP is unnecessary complexity here.

For the MVP in this repo:

- implement HTTP first
- keep an internal event abstraction
- add WebSockets next without changing storage or use-case code

## 3. Data model

### User

- identity for a person using the app
- fields:
  - `id`
  - `username`
  - `created_at_ms`

### Server

- top-level grouping like a Discord guild
- fields:
  - `id`
  - `name`
  - `created_at_ms`

### Channel

- text channel inside a server
- fields:
  - `id`
  - `server_id`
  - `name`
  - `created_at_ms`

### Message

- one text message in a channel
- fields:
  - `id`
  - `channel_id`
  - `author_id`
  - `content`
  - `created_at_ms`

### Session

- login session for a desktop client
- fields:
  - `id`
  - `user_id`
  - `token`
  - `created_at_ms`
  - `expires_at_ms`

## 4. Recommended folder structure

The current repo already uses the recommended layout:

- `include/hangout/domain`: core models and shared errors
- `include/hangout/application`: business logic interfaces
- `include/hangout/storage`: persistence interfaces
- `include/hangout/transport`: server entrypoints
- `src/...`: implementations matching those layers
- `docs/`: architecture notes
- `scripts/`: verification helpers

## 5. Libraries worth using

Use only what pulls its weight:

- `sqlite3`
- `cpp-httplib`
- `nlohmann/json`

Skip heavier frameworks until you specifically need built-in realtime, ORM support, or a larger middleware ecosystem.

## 6. Build order

Build in this order:

1. SQLite schema and seed data
2. repositories
3. auth service
4. chat service
5. HTTP routes
6. realtime gateway

## 7. Realtime path

The important design move is the `MessageEventBus` interface.

Today:

- `ChatService::send_message(...)`
  - writes to SQLite
  - publishes a domain event to the event bus
- current implementation uses a no-op event bus

Next:

- add a WebSocket transport implementation of that bus
- track client subscriptions by channel/server/session
- fan out `message_created` events to connected Electron clients

That gives you realtime without rewriting the use cases.
