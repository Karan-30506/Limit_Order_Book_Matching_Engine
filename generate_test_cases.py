#!/usr/bin/env python3
"""
Generate random JSON order requests for the Limit Order Book Matching Engine.

Usage:
    python generate_stress_test.py 500
    python generate_stress_test.py 10000
    python generate_stress_test.py 500 --seed 7
    python generate_stress_test.py 500 --output worklet2/test/custom.json
"""

import argparse
import json
import random
from pathlib import Path


def make_order(order_id, order_type, side, price, quantity):
    return {
        "id": order_id,
        "type": order_type,
        "side": side,
        "price": price,
        "quantity": quantity,
    }


def generate_orders(count, seed):
    rng = random.Random(seed)
    orders = []

    for index in range(count):
        order_id = 100001 + index
        side = rng.choice(("BUY", "SELL"))
        order_type = "LIMIT" if rng.random() < 0.70 else "MARKET"
        quantity = rng.randint(1, 500)
        price = rng.randint(80, 120) if order_type == "LIMIT" else 0

        orders.append(make_order(order_id, order_type, side, price, quantity))

    return orders


def main():
    parser = argparse.ArgumentParser(
        description="Generate a JSON stress-test dataset with the requested number of orders."
    )
    parser.add_argument(
        "count",
        type=int,
        help="number of order requests to generate, e.g. 500 or 10000",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="random seed for reproducible data (default: 42)",
    )
    parser.add_argument(
        "--output",
        help="optional output path; defaults to test_random_<count>.json in worklet2/test/",
    )
    args = parser.parse_args()

    if args.count <= 0:
        parser.error("count must be greater than zero")

    output_path = (
        Path(args.output)
        if args.output
        else Path("worklet2") / "test" / f"test_random_{args.count}.json"
    )
    output_path.parent.mkdir(parents=True, exist_ok=True)

    orders = generate_orders(args.count, args.seed)
    with output_path.open("w", encoding="utf-8") as file:
        json.dump(orders, file, indent=2)
        file.write("\n")

    limit_count = sum(order["type"] == "LIMIT" for order in orders)
    market_count = len(orders) - limit_count
    buy_count = sum(order["side"] == "BUY" for order in orders)
    sell_count = len(orders) - buy_count

    print(f"Created: {output_path}")
    print(f"Total requests: {len(orders)}")
    print(f"Limit orders: {limit_count}")
    print(f"Market orders: {market_count}")
    print(f"Buy orders: {buy_count}")
    print(f"Sell orders: {sell_count}")
    print(f"Random seed: {args.seed}")


if __name__ == "__main__":
    main()
