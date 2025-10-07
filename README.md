# Mini Version Control System (MVCS)

## Overview
MVCS is a simplified version control system implemented in C for demonstrating Data Structures and Algorithms (DSA) concepts. It tracks changes to files, creates commit history, supports branching, and allows restoration of previous versions.

This project is designed for the "Data Structures and Algorithms" course to practically showcase the application of core DSA principles in a real-world context.

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

## Planned Stages

1. **Design & Scaffolding (Current Stage)** – Define architecture and data models.  
2. **Linked List Commit System** – Implement commit creation and traversal.  
3. **Hashing for Change Detection** – Introduce hashing and file version comparison.  
4. **Snapshot & Checkout** – Enable saving and restoring file states.  
5. **Branching & Merge** – Add multi-branch version management.  
6. **Integration & Analysis** – Combine modules and evaluate performance.

---

## DSA Focus
Each stage integrates one or more DSA principles:
- Linked List for commit chaining.
- Hashing for quick change detection.
- Stack for version traversal.
- Tree/DAG for branching.
