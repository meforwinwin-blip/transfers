# Agent instructions

<!-- status-reporting agent policy v1 -->

## Mandatory coding-agent reporting

Every coding, review and build agent working in this repository must publish progress directly into the **HTML** dashboard in https://github.com/meforwinwin-blip/status-reporting. There is no JSON status feed, separate web server, or GitHub Actions reporting workflow.

The entry point is `C:\Users\Main\source\status-reporting\tools\report_from_source.py`. Run it **from the source Git worktree**, using the working Python executable specified by this repository. It discovers the source repository, branch and HEAD SHA and invokes the direct HTML publisher (`tools/publish_html.py`) through a dedicated clone per agent. If the reporting checkout does not exist, clone `https://github.com/meforwinwin-blip/status-reporting.git` to `C:\Users\Main\source\status-reporting` first.

- Publish `running` when beginning an assigned task, on meaningful progress, and at the start of long builds/tests; refresh at least every 10–15 minutes during lengthy work.
- Publish a truthful terminal `passed`, `failed`, or `blocked` report at completion. Explain next actions and blockers. Never claim an unexecuted build/test succeeded.
- Include real build/test commands and process exit codes in the reports. Do not claim an uncommitted worktree was validated by the HEAD commit alone.
- Use a unique `--agent` ID for every concurrently active agent, and keep it stable for that agent's updates.
- If publishing fails, retry and include the failure in the agent's final response. Never include secrets in status or errors.

Example initial report:

```powershell
& 'C:\Users\Main\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' C:\Users\Main\source\status-reporting\tools\report_from_source.py --agent cache-builder-1 --state running --phase build --summary 'Building firmware'
```

After executing the **actual** build command, capture its exit code immediately and report it (the command shown below is illustrative; use the build command for this repository):

```powershell
cmd /c .\build.bat cache
$buildCode = $LASTEXITCODE
& 'C:\Users\Main\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' C:\Users\Main\source\status-reporting\tools\report_from_source.py --agent cache-builder-1 --state $(if ($buildCode -eq 0) { 'passed' } else { 'failed' }) --phase build --summary 'Firmware build completed' --command 'build.bat cache' --exit-code $buildCode
```

Existing project-specific AGENTS.md rules remain in force. No custom GitHub Actions workflows should be created.
