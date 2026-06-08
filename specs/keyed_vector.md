# `keyed_vector`

**Header:** `<vpp/keyed_vector.hpp>`

## Overview

An insertion-ordered container that stores elements of type `T` in contiguous
memory (`std::vector`) and provides keyed lookup via a caller-supplied callable.
Suitable for small collections where stable insertion order and contiguous
storage matter more than lookup speed.

## Template parameters

| Parameter | Description |
|-----------|-------------|
| `T` | Element type |
| `KeyFn` | Compile-time key accessor (non-type template parameter). May be a free-function pointer, pointer to data member, or pointer to const member function. |
| `Key` | Key type; deduced from `KeyFn` via `std::invoke_result` — override only when needed. Must be equality-comparable. |

Because `KeyFn` is a non-type template parameter, lambdas and stateful functors
are not supported (C++17 restriction).

The idiomatic usage is a type alias:

```cpp
using my_container = vpp::keyed_vector<my_data, &my_data::id>;
```

## Invariants

- Keys are unique; `insert()` rejects duplicates.
- Insertion order is preserved across all operations.
- Lookup is O(n); suited for small collections where stable order and contiguous storage matter more than lookup speed.

## API

| Expression | Description |
|-----------|-------------|
| `keyed_vector{a, b, ...}` | Construct from initializer list; duplicate keys are silently ignored (first wins). |
| `kv.insert(t)` | Insert element; returns `{iterator, true}` on insertion, `{iterator-to-existing, false}` on duplicate. |
| `kv.insert_or_assign(t)` | Insert or overwrite; returns `{iterator, true}` on insertion, `{iterator, false}` on assignment. Position in insertion order is preserved on assignment. |
| `kv.erase(key)` | Remove by key; no-op on miss. |
| `kv.erase(ptr)` | Remove by pointer; throws `std::out_of_range` if not owned. |
| `kv.clear()` | Remove all elements. |
| `kv[key]` | Unchecked access; asserts in debug builds on miss. |
| `kv.at(key)` | Checked access by key; throws `std::out_of_range` on miss. |
| `kv.find(key)` | Returns iterator; `end()` on miss. Const overload provided. |
| `kv.find_ptr(key)` | Returns `T*`; `nullptr` on miss. Const overload provided. |
| `kv.contains(key)` | Returns `bool`. |
| `kv.as_vec()` | Returns a reference to the underlying `std::vector`, giving direct access to the full vector API (positional indexing, `data()`, etc.). Const overload provided. Mutating key fields through the non-const overload can violate key uniqueness — same caveat as mutation via iterators or `find_ptr`. |
| `kv.size()` | Number of elements. |
| `kv.empty()` | True when no elements. |
| `begin/end/cbegin/cend` | Standard range iteration in insertion order. |
