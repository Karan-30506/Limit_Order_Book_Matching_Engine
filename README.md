# Limit Order Book Matching Engine

A C++ command-line Limit Order Book Matching Engine. The program reads order requests from a JSON file, processes them sequentially, matches compatible orders, records generated trades, and writes a detailed log for the run.

## Features

- Reads order requests from JSON input files.
- Supports `LIMIT` and `MARKET` orders, on both `BUY` and `SELL` sides.
- Processes requests sequentially through the matching engine.
- Records executed trades and reports execution quantities and order outcomes.
- Writes detailed per-order and trade logs to a file.
- Prints a final summary of requests, accepted/rejected orders, and recorded trades.
- Includes hand-designed JSON scenarios and a Python script for generating larger random datasets.

## Requirements

- CMake 3.15 or later
- MinGW-w64 / `g++`
- C++14 support
- Python 3 (only required to generate random test data)

## Project layout

```text
Limit_Order_Book_Matching_Engine/
├── CMakeLists.txt
├── include/                     # Worklet 1 headers
├── src/                         # Worklet 1 source files
├── worklet2/
│   ├── include/                 # Worklet 2 headers, including nlohmann/json.hpp
│   ├── src/                     # Matching engine and related source files
│   ├── main.cpp                 # Command-line entry point
│   └── test/                    # JSON test datasets
├── generate_stress_test.py      # Random test-data generator (project root)
├── build/                       # Generated build files (not committed)
└── log/                         # Generated logs
```

## Build

Run these commands from the project root in PowerShell or a terminal.

Configure CMake for MinGW Makefiles:

```powershell
cmake -S . -B build -G "MinGW Makefiles"
```

Compile:

```powershell
cmake --build build
```

The executable should be created at `build/matchingengine.exe`.

If you change CMake configuration or add/remove source files, re-run the configure command and build.

## Run the program

Pass a JSON file path as the first command-line argument:

```powershell
.\build\matchingengine.exe .\worklet2\test\order_requests.json
```

### Run the designed scenarios

These commands assume the scenario JSON files are present in `worklet2/test/`:

```powershell
.\build\matchingengine.exe .\worklet2\test\01_limit_orders_resting.json
.\build\matchingengine.exe .\worklet2\test\02_limit_full_match.json
.\build\matchingengine.exe .\worklet2\test\03_limit_partial_fill_remainder.json
.\build\matchingengine.exe .\worklet2\test\04_market_buy_multilevel.json
.\build\matchingengine.exe .\worklet2\test\05_market_sell_multilevel.json
.\build\matchingengine.exe .\worklet2\test\06_market_insufficient_liquidity.json
.\build\matchingengine.exe .\worklet2\test\07_same_price_fifo.json
```

## Input JSON format

The input file contains a JSON array. Each request has these fields:

| Field | Description | Example |
|---|---|---|
| `id` | Order identifier | `1001` |
| `type` | `LIMIT` or `MARKET` | `"LIMIT"` |
| `side` | `BUY` or `SELL` | `"BUY"` |
| `price` | Limit price; generated market orders use `0` | `100` |
| `quantity` | Requested quantity | `10` |

Example:

```json
[
  {
    "id": 1001,
    "type": "LIMIT",
    "side": "SELL",
    "price": 100,
    "quantity": 10
  },
  {
    "id": 1002,
    "type": "LIMIT",
    "side": "BUY",
    "price": 105,
    "quantity": 6
  }
]
```

Here, the buy order's limit price (105) is at least the sell order's price (100), so they can match under the engine's matching rules.

## Test datasets

| File | Intended purpose |
|---|---|
| `order_requests.json` | Original project input dataset |
| `01_limit_orders_resting.json` | Non-crossing limit orders expected to rest in the book |
| `02_limit_full_match.json` | Limit orders designed to produce full matches |
| `03_limit_partial_fill_remainder.json` | Exercises partial fills and remaining limit quantity |
| `04_market_buy_multilevel.json` | Market buys consuming sell-side liquidity across price levels |
| `05_market_sell_multilevel.json` | Market sells consuming buy-side liquidity across price levels |
| `06_market_insufficient_liquidity.json` | Market orders with more requested quantity than available opposing liquidity |
| `07_same_price_fifo.json` | Exercises FIFO behavior for orders at the same price |

The scenario descriptions explain their intent. Verify the actual result using the generated logs.

### Why can the trade count be zero?

An accepted order does not necessarily generate a trade. For example, if the highest buy price is 89 and the lowest sell price is 110, the prices do not cross (`89 < 110`). Orders can remain in the book without any execution, so zero recorded trades is expected.

A buy limit order can match when its price is greater than or equal to the best sell price. A sell limit order can match when its price is less than or equal to the best buy price. Market orders consume available opposing liquidity; under the current project rules, an unfilled market-order remainder is discarded rather than resting in the book.

## Generate random stress-test data

Keep `generate_stress_test.py` in the project root. Generate 500 requests:

```powershell
python generate_stress_test.py 500
```

Default output:

```text
worklet2/test/test_random_500.json
```

Generate 10,000 requests:

```powershell
python generate_stress_test.py 10000
```

Use a different seed:

```powershell
python generate_stress_test.py 500 --seed 7
```

Choose a custom output path:

```powershell
python generate_stress_test.py 500 --output .\worklet2\test\my_stress_test.json
```

Run the generated file:

```powershell
.\build\matchingengine.exe .\worklet2\test\test_random_500.json
```

The generator creates a workload; it does not guarantee every order will match or every edge case will be covered.

## Logs

For input `worklet2/test/02_limit_full_match.json`, the log is written to:

```text
log/02_limit_full_match_logs.log
```

The program creates the `log/` directory if needed and overwrites the corresponding log on each run of the same input basename.

Detailed per-order logs include order information, trades generated by that submission, executed quantity, order outcome, remaining quantity, and whether the remainder rests in the book or is discarded. Trade details include trade ID, buy order ID, sell order ID, execution price, and quantity.

The console shows startup information and the final summary; detailed per-order and trade information is written to the log.

Read a log in PowerShell:

```powershell
Get-Content .\log\02_limit_full_match_logs.log
```

Read only the last 50 lines:

```powershell
Get-Content .\log\02_limit_full_match_logs.log -Tail 50
```

## Understanding the final summary

The summary reports total requests, accepted requests, rejected requests, and recorded trades.

Accepted/rejected requests and trades are different counts. An accepted order can generate zero trades if there is no compatible opposing order. One incoming order can also generate multiple trades if it matches multiple resting orders.

## Troubleshooting

### CMake or compiler not found

```powershell
cmake --version
g++ --version
python --version
```

Python is only needed for the stress-test generator.

### Input file cannot be opened

- Confirm the file exists and the path is correct.
- Run the executable from the project root when using the relative paths above.
- Check the filename and extension.

### JSON parsing fails

- Ensure the file is a JSON array.
- Check the required fields: `id`, `type`, `side`, `price`, and `quantity`.
- Use supported values: `LIMIT`/`MARKET` and `BUY`/`SELL`.
- Check commas, quotes, and brackets.

### A scenario reports zero trades

Check whether its limit prices cross. Start with `02_limit_full_match.json` or a market-order scenario to exercise matching. If a scenario designed to match still produces no trades, inspect its log and debug the matching and trade-recording path.

### Rebuild after changes

```powershell
cmake --build build
```

If you need to reconfigure as well:

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

## Git hygiene

Generated build files, executables, and logs generally should not be committed. Typical `.gitignore` entries:

```gitignore
/build/
/log/
*.exe
*.o
```

Commit source files, headers, `CMakeLists.txt`, test JSON files, `generate_stress_test.py`, and the vendored `nlohmann/json.hpp` header.

## Scope

This is a command-line matching-engine project. Commands and paths reflect the current CMake/MinGW workflow described for the project. For exact validation and matching semantics, refer to the current source code and tests.