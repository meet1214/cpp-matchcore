# MatchCore

A multithreaded C++ limit order book and matching engine with a TCP
interface — implements price-time priority matching, price
improvement, authentication, SQLite-backed persistence, and
multi-symbol trading.

## Features

- Price-time priority matching engine with price improvement
- IOC (immediate-or-cancel) and GTC order types
- Multi-symbol order books, each independently matched
- TCP server, thread pool-backed, handling concurrent clients
- Account registration/login with salted SHA-256 password hashing
- Real-format account numbers (e.g. `MC0000000001`)
- SQLite persistence: completed trades, resting orders (crash
  recovery), user accounts
- Per-session rate limiting
- Live market-data broadcast to all connected clients on trade execution
- Config file for port/thread count/DB path
- Structured, leveled logging (console + file)
- Interactive CLI client with a login/register menu
- Unit tests (matching engine, auth, persistence) + a concurrency
  stress test

See [`docs/DESIGN.md`](docs/DESIGN.md) for architecture, key design
decisions, and known limitations.

## Build

Requires: CMake ≥ 3.16, a C++17 compiler, SQLite3 dev headers, OpenSSL
dev headers.
sudo dnf install cmake sqlite-devel openssl-devel # Fedora
mkdir build && cd build
cmake ..
cmake --build .
Produces: `matchcore_demo`, `matchcore_tests`,
`matchcore_userstore_tests`, `matchcore_tradelogger_tests`,
`matchcore_server`, `matchcore_client`, `matchcore_stress_test`.

## Configuration

Create `matchcore.conf` in the directory you run the server from:
port=9000
threads=8
db_path=matchcore.db
Falls back to sensible defaults (port 9000, 4 threads,
`matchcore.db`) if the file is absent.

## Running

Standalone demo (no server, no networking — exercises the matching
engine directly):
./matchcore_demo

Run the server (from the project root, so it finds `matchcore.conf`):

./build/matchcore_server

In a separate terminal, connect with the interactive client:
./build/matchcore_client

Register or log in, then trade:
matchcore> BUY AAPL 50 10
matchcore> SELL AAPL 50 5
matchcore> BOOK AAPL
matchcore> BOOK
matchcore> CANCEL AAPL 3
matchcore> BUY AAPL 51 10 IOC
matchcore> HELP

## Testing

./build/matchcore_tests # OrderBook matching logic
./build/matchcore_userstore_tests # auth
./build/matchcore_tradelogger_tests # trade persistence
./build/matchcore_stress_test # 10 concurrent clients, 1000 orders

## Protocol

Plain-text, newline-delimited, over TCP:

| Command | Usage |
|---|---|
| `REGISTER` | `REGISTER <password> <full name>` |
| `LOGIN` | `LOGIN <accountNumber> <password>` |
| `BUY` / `SELL` | `BUY <symbol> <price> <qty> [IOC]` |
| `CANCEL` | `CANCEL <symbol> <orderId>` |
| `BOOK` | `BOOK [symbol]` — omit symbol to list all active symbols |
| `HELP` | lists all commands |
| `QUIT` | disconnect |

## Project structure

include/ headers (Order, Trade, OrderBook, Server, UserStore,
TradeLogger, OrderStore, Logger, ThreadPool)
src/ implementations + entry points (main.cpp, main_server.cpp,
client.cpp)
tests/ unit tests + concurrency stress test
docs/ design notes and known limitations
