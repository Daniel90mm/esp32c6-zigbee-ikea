# Agent Instructions


<!-- BEGIN MANAGED: agent-delegation -->
## DeepSeek Delegation

This repository may use DeepSeek only through `agent-delegate`.

Before delegating, write a task file with YAML front matter declaring `provider`, `topic`, `allowed_read`, `allowed_write`, and `output`. Run `agent-delegate deepseek --dry-run <task-file>` before the first real call.

Use DeepSeek only for bounded, reviewable work. Do not call DeepSeek directly, and do not send secrets, credentials, private logs, compliance-sensitive material, or files outside the declared allowlist.

DeepSeek output is advisory. The active Codex/Claude agent owns final review, tests, edits, and commits.
<!-- END MANAGED: agent-delegation -->
