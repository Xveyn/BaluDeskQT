# Security Policy

## Scope

BaluDesk QT is the desktop synchronisation client for a [BaluHost](https://github.com/Xveyn/BaluHost)
server. It is **in development** and has not had a stable release; there is no supported
version line yet. Until then, only the tip of `main` receives fixes.

Vulnerabilities in the **server** belong in the
[BaluHost repository](https://github.com/Xveyn/BaluHost/security) — this repository covers
the client.

## Reporting a Vulnerability

**Please do not report security vulnerabilities through public GitHub issues, discussions,
or pull requests.**

Instead, report privately via one of these channels:

- **GitHub Security Advisories** (preferred): open a draft advisory at
  <https://github.com/Xveyn/BaluDeskQT/security/advisories/new>
- **Email**: contact the maintainer via the address on the
  [Xveyn GitHub profile](https://github.com/Xveyn)

Please include as much of the following as possible:

- Type of issue (credential exposure, TLS bypass, path traversal, privilege escalation, …)
- Affected component and, if you have it, the commit
- Steps to reproduce, ideally as a failing test
- What an attacker gains

You will get an acknowledgement. Since this is a spare-time project, please allow a few
days before following up.

## What this client protects, and how

Two properties are load-bearing. If you find a way around either, that is a vulnerability
even if nothing visibly breaks:

**Transport trust.** Certificate verification runs against the system certificate store.
When it fails, the client shows the certificate's fingerprint and asks for a one-time
confirmation during pairing, then pins the public key. A later mismatch is a **hard
failure** with no re-prompt — a "continue anyway?" question during normal operation would
wave through any machine-in-the-middle. There is deliberately no global "allow insecure
connections" switch. Plain HTTP is accepted only for `localhost`.

**Credential storage.** Access and refresh tokens live in the Windows Credential Manager,
stored under separate keys, never in a configuration file and never in a log. What lands on
disk is the server address, the username and the confirmed certificate pin — none of which
is a secret.

## Out of scope

- An attacker who already has code execution as the same user: they can read the credential
  store by design, and no client-side measure changes that
- The strength of the server's own authentication — that belongs to BaluHost
- Findings that require a modified build of this client
