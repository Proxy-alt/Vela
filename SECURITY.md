# Security Policy

## Reporting a Vulnerability

If you find a security issue in Voland, please **do not open a public GitHub issue**. Public disclosure before a fix gives bad actors a window to exploit the vulnerability.

Instead open a private security advisory through GitHub: go to the repo's Security tab → Advisories → Report a vulnerability

We will acknowledge receipt within 7 days and provide an estimated timeline for a fix or response within 14 days.

## Scope

Voland is currently pre-alpha. There is no production deployment, no shipped binaries, no users running emulator code from this repo. The "security surface" right now is small — primarily the build process and any code that gets executed during compilation or testing.

Once Voland has runnable code, this policy will expand to cover:

- Memory safety issues in the JIT-compiled output
- Sandbox escapes from the emulator
- Issues in dependency handling or build tooling
- Anything that could compromise users running compiled emulator builds

## Out of Scope

- Issues in upstream dependencies (report to the relevant upstream)
- Issues in Ballistic specifically (report to [pound-emu/ballistic](https://github.com/pound-emu/ballistic))
- Theoretical issues with the project's design that haven't been implemented yet
- Issues in the Switch 2 console itself or Nintendo's software

## Supported Versions

Pre-alpha. There are no released versions to support yet. Once Voland has releases, this section will list which versions receive security updates.
