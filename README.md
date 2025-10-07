# Mini Version Control System (MVCS)

## Overview
MVCS is a simplified version control system implemented in C for demonstrating Data Structures and Algorithms (DSA) concepts. It tracks:
- changes to files,
- creates commit history,
- supports branching, and
- allows restoration of previous versions.

This project showcase the application of core DSA principles in a real-world context.

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

## DSA Focus
Each of the planned 6 Stages integrates one or more DSA principles:
- Linked List for commit chaining.
- Hashing for quick change detection.
- Stack for version traversal.
- Tree/DAG for branching.
