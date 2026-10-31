# Security policy for Basalt Server

## Report vulnerabilities privately

**Do not disclose suspected vulnerabilities through public issues, pull requests,
logs or discussions.** Use [GitHub's private vulnerability reporting form](https://github.com/BasaltProject/Server/security/advisories/new).
It sends the report privately to the repository's security maintainers, not to the
public issue tracker.

If you cannot use that form, email
[hello@basaltmc.org](mailto:hello@basaltmc.org) with **Basalt Server security report** in the subject.
Email is a fallback private contact, not an end-to-end encrypted channel; avoid
sending live secrets and ask how to share sensitive evidence if necessary.

Ordinary bugs without a security impact can use the
[bug report form](https://github.com/BasaltProject/Server/issues/new?template=bug_report.yml).
For problems in Basalt Proxy, use its
[security policy](https://github.com/BasaltProject/Proxy/blob/main/SECURITY.md).
If a shared component such as Wire is involved, identify both affected products;
you do not need to publish a separate public report in each repository.

## Versions and scope

Basalt is actively developed. Security investigation focuses on the latest
published stable release and the current `main`/nightly build. Older versions
may need to be upgraded; backports and long-term support are not guaranteed.
Nightlies are development builds, not a guarantee of production readiness.
Report the exact affected release or commit even if you cannot test a newer build.

Potential vulnerabilities include remote code execution or memory-safety failures,
authentication or permission bypasses, unsafe packet handling, Lua sandbox escapes,
compatibility-layer boundary failures, unauthorized remote-management access, and
file or world access outside the intended permissions.

Bukkit-style plugins execute code through the compatibility runtime and must be
chosen carefully. Do not assume that an untrusted plugin jar is safely isolated
just because Lua plugins have a sandbox. A sandbox or permission-boundary bypass
in Basalt itself is still in scope.

A denial of service is potentially security-relevant when an untrusted client or
other lower-trust actor can trigger it. Explain the access required and impact;
an ordinary configuration mistake or a trusted administrator stopping a process
is not automatically a vulnerability.

## What to include

- A concise description, affected component, impact and required attacker access.
- Exact Basalt version or commit, OS, CPU architecture and relevant plugin versions.
- Client editions/versions and world or dimension details needed to reproduce the issue.
- Minimal steps or a small proof of concept tested only in an environment you control.
- Redacted logs or native crash traces, and any suspected regression or mitigation.

Do not include live credentials, license keys, authentication tokens, private keys,
forwarding secrets or personal data. Use synthetic values wherever possible.
Do not attack other people's servers, collect their data or disrupt public services
to demonstrate a finding. If credentials have leaked, revoke or rotate them rather
than relying only on deleting the report.

## Coordinated handling

Maintainers may ask for a minimal reproducer or additional environment details,
then assess impact and coordinate a fix or mitigation. Keep exploit details private
while that work is underway and agree on public disclosure before publishing them.
We will discuss attribution with you before identifying you in an advisory.

This policy does not promise a response deadline, a fix schedule, a bounty or
legal authorization to test systems you do not own. If you need to follow up,
reply through the original private channel instead of posting the exploit publicly.
