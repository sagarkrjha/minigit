# MiniGit Core Algorithms

This document details the exact mathematical and algorithmic principles implemented within MiniGit.

---

## 1. Eugene Myers' $O(ND)$ Greedy Difference Algorithm

### 1.1 Problem Statement
Given two sequences of lines $A = (a_1, a_2, \dots, a_m)$ and $B = (b_1, b_2, \dots, b_n)$, find the Shortest Edit Script (SES) that transforms $A$ into $B$ using two primitive operations:
- **Delete** a line from $A$ (cost 1)
- **Insert** a line into $B$ (cost 1)

### 1.2 Mathematical Formulation & The Edit Graph
The problem maps to finding the shortest path from $(0, 0)$ to $(m, n)$ on a directed grid where:
- Horizontal move $(x, y) \to (x+1, y)$: deletion of line $a_{x+1}$ (cost 1).
- Vertical move $(x, y) \to (x, y+1)$: insertion of line $b_{y+1}$ (cost 1).
- Diagonal move $(x, y) \to (x+1, y+1)$: match where $a_{x+1} == b_{y+1}$ (cost 0, "snake").

We parameterize paths by diagonals $k = x - y$.
- Horizontal moves increase diagonal: $k \to k + 1$.
- Vertical moves decrease diagonal: $k \to k - 1$.
- Diagonal moves stay on the same diagonal $k$.

### 1.3 Implementation in MiniGit (`src/diff/diff_engine.cpp`)
MiniGit implements Eugene Myers' 1986 greedy strategy:
1. An array $V[-D \dots D]$ stores the furthest-reaching $x$-coordinate on each diagonal $k$.
2. For each edit distance $D = 0, 1, 2, \dots, m + n$:
   - For each diagonal $k \in \{-D, -D+2, \dots, D\}$:
     - Determine whether to step down from diagonal $k+1$ or right from diagonal $k-1$:
       $$x = \begin{cases} V[k+1] & \text{if } k = -D \lor (k \ne D \land V[k-1] < V[k+1]) \\ V[k-1] + 1 & \text{otherwise} \end{cases}$$
     - $y = x - k$.
     - Slide down the zero-cost diagonal snake as far as lines match:
       $$\text{while } x < m \land y < n \land a_{x+1} == b_{y+1} \implies x++, y++$$
     - Store $V[k] = x$.
     - If $(x, y) == (m, n)$, record $D$ and terminate search.
3. **Trace Backtracking**: The history of $V$ across each step $d \le D$ is preserved in a 2D trace vector, from which the optimal sequence of `Keep`, `Add`, and `Remove` operations is extracted.

### 1.4 Asymptotic Complexity
- **Time**: $O(ND)$ worst-case, where $N = m + n$ and $D$ is the length of the edit script. If sequences are identical ($D = 0$), execution completes in linear $O(N)$ time.
- **Space**: $O(D^2)$ to store the search trace for backtracking.

---

## 2. Lowest Common Ancestor (LCA) & 3-Way Line Merge

### 2.1 Graph Topology & Merge Base
Given two diverging branch tips:
- `Ours` (local commit $C_{ours}$)
- `Theirs` (remote commit $C_{theirs}$)

MiniGit traverses the commit DAG backwards using a queue-based Breadth-First Search (BFS) to compute the **Lowest Common Ancestor (LCA)** $C_{base}$.

```text
       C_base (Common Ancestor)
       /    \
      /      \
  C_ours    C_theirs
      \      /
       \    /
    Merge Commit (LCA 3-way result)
```

### 2.2 Three-Way Line Resolution Strategy (`src/merge/merge_engine.cpp`)
1. Compute Myers diff $\Delta(Base, Ours)$ and $\Delta(Base, Theirs)$.
2. Align changes against the original base lines using a three-way cursor walk:
   - **Case 1 (Unchanged)**: Line is identical across Base, Ours, and Theirs $\to$ Keep base line.
   - **Case 2 (Unilateral Change)**: Line was modified only in Ours $\to$ Accept Ours change cleanly.
   - **Case 3 (Unilateral Change)**: Line was modified only in Theirs $\to$ Accept Theirs change cleanly.
   - **Case 4 (Identical Mutual Change)**: Both Ours and Theirs made the exact same edit $\to$ Deduplicate and accept.
   - **Case 5 (Conflicting Changes)**: Both branches modified the same base span differently $\to$ Insert standard Git conflict markers:
     ```text
     <<<<<<< HEAD
     [Ours lines]
     =======
     [Theirs lines]
     >>>>>>> branch-name
     ```

---

## 3. Packfile Delta Compression & Fan-Out Indexing

### 3.1 Sliding-Window Delta Representation (`src/storage/pack.cpp`)
To minimize repository storage footprint, MiniGit packs multiple loose objects into contiguous `.pack` archives using byte-level delta compression:
- **Base Object**: Full uncompressed or zlib-compressed object.
- **Delta Object**: Instructions relative to a preceding base object in the pack window.
- **Instruction Bytecodes**:
  - `COPY (offset, size)`: Reuses `size` bytes from the base object starting at `offset`.
  - `INSERT (size, data)`: Appends literal bytes unique to this object.

### 3.2 256-Entry First-Level Fan-Out Table (`.idx` v2)
To achieve fast $O(\log N)$ object lookups without scanning the entire multi-megabyte pack file:
1. **Fan-Out Table**: 256 32-bit big-endian integers storing the cumulative count of objects whose SHA-256 hash starts with a byte $\le i$ (for $i \in [0, 255]$).
2. **Binary Search**:
   - For an object hash starting with byte `0x4a`, bounds are restricted to $[\text{fanout}[0x49], \text{fanout}[0x4a])$.
   - Perform binary search within this narrow slice over the sorted SHA-256 table.
3. **Offset Resolution**: Once index position $k$ is found, read the corresponding 32-bit packfile byte offset and decompress the object or delta chain.
