# Agent instructions


<!-- status-reporting agent policy v1 -->

## Required agent status reporting (all coding and build agents)

Report to the central private repository: https://github.com/meforwinwin-blip/status-reporting. This is **mandatory** for every agent working on this repository, including agents building or reviewing feature branches. **Do not use GitHub Actions**, workflows, or notification emails for reporting.

The publisher is \`tools/report_status.py\` in the status-reporting repository. The recommended Windows entry point is \`C:\Users\Main\source\status-reporting\tools\report_from_source.py\`, invoked from the **source worktree**. It detects this repository, its current branch, and its current commit, and gives each agent a separate reporting clone. If that checkout isn't installed, clone \`https://github.com/meforwinwin-blip/status-reporting.git\` to \`C:\Users\Main\source\status-reporting\` first. Use the working Python executable from this repository's own instructions (not a broken Windows Python shim).

At the start of an assigned task, publish \`--state running --phase planning --summary "<task>"\`. Publish again after meaningful implementation progress and when starting a long build/test; for work longer than 15 minutes refresh the status at least every 10–15 minutes. At completion, publish exactly one terminal state: \`passed\`, \`failed\`, or \`blocked\`. Include \`--next-action\` and \`--error\` for failures/blockers. Keep a unique and stable \`--agent\` ID per concurrently executing agent (e.g. \`cache-builder-1\`).

Example from the source worktree (replace the ID and text):
\`\`\`powershell
& 'C:\Users\Main\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' C:\Users\Main\source\status-reporting\tools\report_from_source.py --agent cache-builder-1 --state running --phase build --summary "Compiling firmware"
\`\`\`
After the **actual command finishes**, report its actual exit code:
\`\`\`powershell
# Example only: set code to the build/test process's REAL exit code; do not assume success
& 'C:\Users\Main\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' C:\Users\Main\source\status-reporting\tools\report_from_source.py --agent cache-builder-1 --state passed --phase build --summary "Build passed" --command "build.bat cache" --exit-code 0
\`\`\`

If a build fails, set \`--state failed\`, pass its nonzero exit code and a concise \`--error\`, and never mark unexecuted checks as passed. Capture the SHA of the commit actually tested; if you build uncommitted edits, explain that explicitly and do **not** claim those edits were validated at HEAD. A status is a progress report, not permission to skip checks. If the report push fails, include that failure in the agent's final response and retry when possible. Do not publish secrets or unredacted build logs. Keep all existing project-specific agent rules in force.
