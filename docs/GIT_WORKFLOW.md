# Git Workflow & Commit Conventions

## 1. Commit Messages

### Atomicity

Each commit must contain **one logical change**. A commit should bring the project from one stable state to another.

```bash
# Good (one logical change)
feat(ecs): add basic archetype storage

# Bad (two changes in one commit)
Added some ECS stuff and fixed a bug in the query system also updated docs
```

### Format

```
<type>(<scope>): <short summary>
  │          │               │
  │          │               └─ imperative present tense, ≤72 chars
  │          └───────────────── optional scope (area of engine)
  └──────────────────────────── type from table below
```

If a body is needed, separate it with a blank line and explain **why** the change was made, not just what changed. Wrap the body at 80 characters.

```text
fix(renderer): correct uniform buffer alignment on Mali GPUs

Mali G-series requires 256-byte alignment for UBOs, but we were only
guaranteeing 128. This caused rendering corruption on devices like
the Pixel 8. Align all UBO allocations to 256.
```

### Allowed Types

| Type       | Usage                                      |
| ---------- | ------------------------------------------ |
| `feat`     | New feature                                |
| `fix`      | Bug fix                                    |
| `docs`     | Documentation only                         |
| `refactor` | Code change that neither fixes nor adds    |
| `test`     | Adding or correcting tests                 |
| `build`    | Build system, CI, dependencies             |
| `perf`     | Performance improvement                    |
| `style`    | Formatting, whitespace (no logic change)   |
| `chore`    | Tooling, config, maintenance               |
| `revert`   | Revert a previous commit                   |

### AI Co-Authoring: Forbidden

**Never** include `Co-authored-by`, `Generated with`, `Grok`, `Claude`, or similar lines in any commit message. The human developer is the sole author. AI may draft or suggest messages, but the final commit is always owned by a human.

```bash
# NEVER
feat(ecs): add component query API
Co-authored-by: Claude <claude@anthropic.com>

# Correct
feat(ecs): add component query API
```

## 2. Branching Model

### Feature Branches

All new work starts on a feature branch:

```
feature/<short-description>
feat/<short-description>
```

Examples:

```
feature/ecs-archetype-storage
fix/jolt-alignment-regression
docs/git-workflow
```

### Branches Stay Open After Merge

After a feature branch is merged into `master`, **do not delete it**. Instead:

1. Rebase the feature branch on top of the latest `master`:

   ```bash
   git checkout feature/my-feature
   git rebase master
   ```

2. Force-push (only on your feature branch, never to master):

   ```bash
   git push --force-with-lease
   ```

This keeps the branch up to date and preserves it for future hotfixes, follow-up work, or reference. A closed PR is not the end of a branch's useful life.

### Avoiding Divergence

- Rebase feature branches onto `master` frequently instead of merging `master` in.
- Avoid long-lived branches that drift far from `master`.
- Use `git rebase -i` to clean up local history before pushing.

## 3. Merging & Squash Policy

### Squash Merging Preferred

Feature branches are **squash-merged** into `master` to keep history clean. The squashed commit message **must be a high-quality summary** written by the person merging.

| Role | Responsibility |
|---|---|
| Author | Writes good individual commits on the branch |
| Merging person | Writes the final squash commit message that summarizes the entire feature |

**Never** use auto-generated messages like "Merge pull request #123" or "Squashed commit of the following:".

```bash
# Good squash message
feat(ecs): implement archetype-based ECS with queries

Adds a archetype storage, component queries, and a system scheduler.
Entities are grouped by component layout for cache-friendly iteration.
No Redot integration yet — pure Zig subproject.

# Bad squash message
Merge pull request #42 from user/ecs-stuff
```

## 4. General Hygiene

| Practice | Rule |
|---|---|
| Force-push to master | **Never** |
| Force-push to feature branch | Allowed with `--force-with-lease` |
| Merge commits from master into feature branch | Avoid; prefer rebase |
| `git pull --rebase` | Always use instead of plain `git pull` |
| Push frequency | Push feature branches early and often |
| Clean history before PR | Use `git rebase -i` to squash fixups |

## 5. Workflow Summary

```
[Start] → Create feature branch from master
        → Make atomic commits with good messages
        → Rebase onto master frequently
        → Open PR
        → Squash-merge into master
        → Rebase feature branch on new master
        → Keep branch for future work
```
