# Mini Version Control System (MVCS)

## Overview
MVCS is a simplified version control system implemented in C. It tracks:
- changes to files,
- creates commit history,
- supports branching, and
- allows restoration of previous versions.

This project showcase the application of core DSA principles in a real-world context. It progressively evolves from a basic linked list–based commit tracker into a mini Git-like tool, built entirely from scratch in C.

---

## System Architecture

### Core Components
1. **Repository**
   - Maintains the overall commit history.
   - Contains the head pointer and commit count.

2. **Commit**
   - Stores metadata for each version (ID, message, hash, timestamp).
   - Linked together as a singly linked list.

3. **Hash Module**
   - Computes file signatures to detect changes.
   - Enables efficient change comparison.

---

## Data Structure Mapping

| Component | Data Structure | Purpose |
|------------|----------------|----------|
| Commit History | Singly Linked List | Sequential commit storage |
| Change Detection | Hashing | Detect modified files efficiently |
| Undo/Redo | Stack | Maintain traversal through commit history |
| Branching | Tree/DAG | Represent multiple development paths |

---

## Current Progress

1. **Implemented**

-  Repository initialization
- Commit creation and logging
- Linked list–based commit history

Each commit node stores metadata (ID, message, hash, timestamp) and connects to the previous commit, forming a chain similar to Git’s commit history.

## How to Build and Run
```bash
make clean
make
./mvcs
