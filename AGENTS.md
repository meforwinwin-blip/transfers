# Agent instructions

<!-- status-reporting agent policy v2 -->

## Mandatory agent status reporting (Windows, Linux, macOS and cloud)

Every coding, review or build agent MUST report to the **HTML + GitHub-rendered
README** dashboard in [meforwinwin-blip/status-reporting](https://github.com/meforwinwin-blip/status-reporting).
No JSON reporting feed, GitHub Actions, GitHub Pages, separate server or email
workflow is required or permitted for agent reporting.

**Read the canonical [cross-platform agent reporting guide](https://github.com/meforwinwin-blip/status-reporting/blob/main/docs/agent-reporting.md).**
Do not assume the user's Windows paths, COM ports, `.bat` scripts, installed
toolchain or local checkout exist in a Linux/macOS/container/remote environment.

**With a local source Git worktree (Linux/macOS):**
```bash
# In the source worktree; clone the reporting repo first if needed.
python3 /path/to/status-reporting/tools/report_from_source.py \
  --agent unique-agent-id --state running --phase planning \
  --summary "Starting assigned work"
```

**On Windows:** Run that same script using the source repository's configured
working Python interpreter and your actual reporting checkout location.
`C:\\Users\\Main\\source\\status-reporting` is a Windows-specific *example*,
never a required path on other hosts.

**No local source checkout, but Git/Python networking works:** The updated
wrapper supports explicit source metadata without source autodetection:
```bash
python3 /path/to/status-reporting/tools/report_from_source.py \
  --repo OWNER/REPO --branch ACTUAL_SOURCE_BRANCH \
  --source-sha ACTUAL_HEX_SOURCE_COMMIT \
  --agent unique-agent-id --state running --phase implementation \
  --summary "Working through a GitHub connection"
```
All three metadata values must refer to the real source revision. This mode
still needs authenticated `git clone/fetch/push` to the reporting repository.

**Only GitHub connector/API access, no usable git transport:** Do not assume
`report_from_source.py` succeeded. Use the authorized connector to update the
latest `index.html` agent row and regenerate `README.md`, then commit **both
atomically**, checking the expected old Git HEAD to avoid overwriting other
agents. Follow the linked guide. If the connector cannot write either, report
the publishing failure honestly rather than inventing a report.

Publish `running` when the task starts, after meaningful progress, before a
lengthy build/test and at least every 10–15 minutes on long tasks. Publish one
terminal state `passed`, `failed` or `blocked` with errors/next actions as
appropriate. Use one stable, unique `--agent` ID per agent/task. Include the
actual source repo, branch, commit SHA and **real** build/test commands and
exit codes. Never mark unrun checks as passed: use `skipped`, disclose a missing
toolchain, and distinguish user-confirmed results from agent-executed tests.
Preserve uncommitted work and existing repository/worktree rules. Never
force-push the reporting dashboard. No GitHub Actions.
