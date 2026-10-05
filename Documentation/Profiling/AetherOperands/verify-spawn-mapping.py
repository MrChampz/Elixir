"""Check emission-order mapping against the previous cyclic spawn-range mapping."""
cases = 0
for capacity in range(1, 65):
    for cursor in range(capacity):
        for count in range(capacity + 1):
            previous = {
                index: (index + capacity - cursor) % capacity
                for index in range(capacity)
                if count == capacity or (count and (
                    cursor <= index < cursor + count if cursor + count <= capacity
                    else index >= cursor or index < cursor + count - capacity
                ))
            }
            remaining = capacity - cursor
            current = {
                cursor + order if order < remaining else order - remaining: order
                for order in range(count)
            }
            assert previous == current, (capacity, cursor, count)
            cases += 1
print(f"Validated {cases:,} cursor/capacity/count combinations.")
