# Security policy

## Scope

DX11 Overlay Inspector is an educational, same-process, read-only inspection
project. Memory writes, third-party process control, protection bypasses, and
anti-cheat evasion are intentionally out of scope.

## Reporting

Please report vulnerabilities through the repository's private security
advisory feature. Include:

- the affected commit,
- a minimal reproduction,
- expected and observed behavior,
- whether the issue affects loader-lock safety, shutdown, memory validation,
  or hook lifetime.

Do not include secrets, personal data, or samples targeting software you do not
own or have permission to test.

## Supported versions

Until the first stable release, only the latest commit on the default branch is
supported.
