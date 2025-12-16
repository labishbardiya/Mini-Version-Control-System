## cs - Version Control System

`cs` is a small, fully local version control kernel written in C for learning data structures and algorithms.
It exposes simple, linear history over a hidden `.cs` directory and comes with a frontend that visualizes
commits, blobs, the index, and integrity checks.

### Design philosophy

- **Clarity over features**: only a linear history is supported. No branches, merges, or networking.
- **Determinism**: hashes and commit ids are computed from explicit fields only.
- **Human readable storage**: all metadata is stored as plain text.
- **Data structure focus**: each operation is a transformation on simple maps and lists.

### Repository layout

On `./cs init` the following structure is created:

- `.cs/objects/blobs`: one file per blob, named by a deterministic content hash
- `.cs/objects/commits`: one file per commit, named by commit id
- `.cs/index`: text file, one line per staged file: `path hash`
- `.cs/HEAD`: current commit id (or empty if no commits)
- `.cs/config`: reserved for future course-specific settings

Commit files are plain text with this shape:

```text
parent <commit-id-or-null>
timestamp <unix-seconds>
message <commit message>
files
<path> <blob-hash>
<path> <blob-hash>
...
```

### Data structures

- **Blob**: immutable content, addressed by a 64-character hex hash (FNV-1a–based).
- **File map**: dynamic array of `{ path, hash }` pairs, sorted by path when serialized.
- **Index**: file map representing the staging area.
- **Commit**: struct containing `id`, `parent id`, `timestamp`, `message`, and a file map.
- **History**: a simple linked list of commits, followed from `HEAD` backward via parent ids.

These match standard DSA concepts: dynamic arrays, maps stored as sorted arrays, and singly linked lists.

### Build instructions

From the project root:

```sh
make
```

This produces the `./cs` binary.

### Command list and behavior

- `./cs init`  
  Creates the `.cs` directory tree. Fails if `.cs` already exists. Does not touch working files.

- `./cs add .`  
  Recursively stages all regular files under the current directory. The `.cs` directory is ignored.

- `./cs add file.txt`  
  Stages a single regular file. Errors if the file does not exist or is not a regular file.

- `./cs commit -m "message"`  
  Creates an immutable snapshot:
  - Index must not be empty.
  - Message must be non-empty.
  - File map starts from parent commit (if any), then index changes are applied.
  - Commit id is the hash of: parent id, timestamp, message, and sorted file map.
  - After writing the commit, `HEAD` is updated and the index is cleared.
  - Prints:
    - Commit id
    - Number of files changed
    - Lines added
    - Lines removed
    - Total commits in repository

- `./cs log`  
  Prints linear history from `HEAD` backwards, showing commit id, timestamp, and message.

- `./cs revert <commit_id>`  
  Restores tracked files from the target commit (hard revert of tracked files, untracked files are left untouched),
  updates `HEAD`, and clears the index. Errors if the commit id does not exist.

- `./cs revert HEAD`  
  Restores the working directory to the current `HEAD` snapshot. Errors if no commits exist.

- `./cs trace <filename>`  
  Shows creation and change commits for a file by walking history from oldest to newest and listing commits where the
  blob hash for that file changes. Errors if the file never existed.

- `./cs integrity`  
  Verifies:
  - `HEAD` points to an existing commit (or is empty if no commits).
  - Every reachable commit file exists.
  - Every blob referenced by reachable commits exists.
  Prints either a success message or a specific error.

- `./cs timewarp <timestamp>`  
  Finds the latest commit with `timestamp <= given value` and restores its snapshot (as in `revert`),
  updates `HEAD`, and clears the index. Errors if no such commit exists.

### Frontend visualization

The frontend is in `frontend/` and is pure HTML/CSS/JavaScript with no external frameworks.
It does not run a server; instead, it asks you to load a `.cs` directory using the browser.

Usage:

1. Build and use `cs` to create a repository and some commits.
2. Open `frontend/index.html` in a modern browser (double-click or use `Open File`).
3. Click **“Load .cs directory”** and select the repository root so the `.cs` folder is included.
4. The viewer parses:
   - `.cs/HEAD`
   - `.cs/index`
   - `.cs/objects/commits/*`
   - `.cs/objects/blobs/*`

Visual features:

- **Commit graph**: linear nodes with parent relationships (implicit) and integrity coloring
  (green for valid, red if a commit is missing when walking from `HEAD`).
- **Three-state view**:
  - Committed file map at `HEAD`
  - Staging area (index)
  - Approximate working state (HEAD plus staged changes)
- **Timeline slider**: moves along the linear history and updates the views.
- **File trace**: clicking a path shows all commits where its blob hash changed.
- **Algorithm overlay**: step-by-step text description of actions like loading commits,
  walking history, and tracing a file.

This design avoids any extra tooling: the browser’s directory picker exposes the `.cs` files to JavaScript,
which then builds in-memory data structures mirroring the C structs.

### Testing commands (happy path)

From an empty directory:

```sh
./cs init
./cs add .
./cs add file.txt
./cs commit -m "first commit"
./cs log
./cs trace file.txt
./cs integrity
./cs timewarp <timestamp>
./cs revert HEAD
```

Where `<timestamp>` is a Unix timestamp (seconds since epoch), such as the one printed by `./cs log`.

Automated smoke test:

```sh
cd tests
./run_all.sh
```

### Edge cases to try

- **Commit with empty index**  
  - Run `./cs init` and then `./cs commit -m "msg"` without staging anything.  
  - Expected: `cs` reports that the index is empty and refuses the commit.

- **Invalid commit id for revert**  
  - Run `./cs revert deadbeef` with a non-existent id.  
  - Expected: clear error that the commit does not exist.

- **Non-existent file for add / trace**  
  - `./cs add no_such_file.txt` → error about missing or invalid file.  
  - `./cs trace no_such_file.txt` → error that the file never existed.

All behavior is deterministic and local; there is no branching, merging, or networking by design.



