# Limit_Order_Book_Matching_Engine
TY Mini Project Limit Order Book Matching Engine (COEP)

## Worklet 1
Implemented C++17 domain objects and storage using custom AVL trees, intrusive FIFO queues, and separate-chaining integer hash tables. Insert, cancel, modify, volume lookup, best-level lookup, ordered traversal and ownership-transferring pop work. Matching, trades, market orders, UI, networking, databases and benchmarks are outside this worklet. Crossed books are allowed until Worklet 2 implements matching.

## Worklet 2
Matching engine implementation and trade execution.
