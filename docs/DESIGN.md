# MatchCore — Design Notes

## Overview
A multithreaded C++ limit order book and matching engine with a TCP
interface, authentication, persistence, and multi-symbol support.

## Architecture

- **OrderBook** — core matching engine (price-time priority, price
  improvement). Single-instrument; `Server` holds one `OrderBook` per
  symbol in an `unordered_map`.
- **Server** — TCP server, one thread pool (custom `ThreadPool`,
  reused from an earlier project) servicing connections.
- **UserStore / TradeLogger / OrderStore** — SQLite-backed persistence
  for accounts, completed trades, and resting orders respectively.
- **Logger** — leveled, timestamped logging to console + file.

## Key design decisions

**Thread-per-connection, not event-driven.** Each client connection
occupies one pool thread for its entire session (`handleClient` blocks
in a read loop). Simple and correct, but pool size caps the number of
*simultaneously connected* clients, confirmed directly by the
concurrency stress test (10 threads failed against a pool of 4; passed
at 20). A production system would use non-blocking I/O (epoll) so a
small fixed thread count can service many connections by reacting to
readiness events instead of blocking per-connection. Documented
trade-off, not an oversight — kept simple deliberately for this
project's scope.

**`double` for price.** Floating-point can't represent most decimal
fractions exactly. Real trading systems typically use integer
ticks/cents. Used `double` here for approachability; a real system
would not.

**SHA-256 + per-user salt for passwords, not bcrypt/argon2.** SHA-256
is fast, which is exactly the wrong property for password hashing —
real systems use deliberately slow KDFs to resist brute force. Salting
and constant-time comparison are implemented correctly; the hash
algorithm choice is a documented simplification.

**Passwords sent in plaintext over the wire.** No TLS. Hashing happens
server-side only; the raw password still crosses the network
unencrypted. Real fix is TLS termination — meaningful scope, out of
bounds for this project.

**Snapshot-based order persistence, not incremental.** After every
`addOrder`/`cancelOrder`, the *entire* resting-order set for that
symbol is deleted and reinserted (`OrderStore::save`). Simple and
easy to reason about; not efficient at scale — an incremental
update/delete-by-id approach would be the production version.

**Synchronous market-data broadcast.** `broadcast()` holds
`clientsMutex_` while writing to every connected client's socket
inline. One slow client (full TCP buffer) can stall broadcasts to
everyone and briefly block new connections registering. Production
fix: per-client outbound queue drained by a dedicated writer thread,
decoupling broadcast latency from any single client's read rate.

**Account numbers derived from SQLite's `AUTOINCREMENT` id**, formatted
as `MC` + zero-padded 10 digits — reads like a real account number,
guaranteed unique without extra collision-checking logic.

**WAL mode + busy timeout on all three SQLite connections.** Default
SQLite locking caused `database is locked` failures under concurrent
writes from `TradeLogger`/`UserStore`/`OrderStore` (three separate
connections to one file). Fixed via `PRAGMA journal_mode=WAL` and a
5-second busy timeout — confirmed via the concurrency stress test
(1000 concurrent orders, 0 lock failures after the fix).

## Known limitations / future work

- No TLS (passwords/orders sent in plaintext over the wire)
- Thread-per-connection caps concurrent clients at pool size
- No order expiry beyond IOC (no scheduled cancel-after-time)
- Rate limiting is per-session in-memory only, resets on reconnect
- `double` price representation (documented, not fixed — see above)
- Client is a simple synchronous request/response loop; a broadcasted
  market-data push can only be seen on the client's *next* read, not
  instantly, since there's no dedicated reader thread on the client side

## Testing

- `matchcore_tests` — OrderBook matching logic (full/partial fills,
  price-time priority, cancellation)
- `matchcore_userstore_tests` — registration, login, wrong-password
  rejection
- `matchcore_tradelogger_tests` — trade persistence correctness
- `matchcore_stress_test` — 10 concurrent clients, 1000 total orders,
  verifies exact trade count and empty final book state