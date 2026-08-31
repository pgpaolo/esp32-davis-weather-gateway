# Security Policy

## Supported branch

Security fixes are applied first to `develop` and promoted to `main` after review and CI validation. Critical fixes may be prepared directly against `main` when necessary.

## Reporting a vulnerability

Please avoid publishing credentials, private endpoints, device identifiers, network dumps containing secrets, or exploitable details in a public issue.

For a suspected vulnerability, contact the repository maintainer privately through the contact options available on the maintainer's GitHub profile. Provide the affected version/commit, hardware target, impact, and reproducible steps where possible.

## Secrets and configuration

The public repository must not contain real Wi-Fi credentials, private certificates, tokens, passwords, or installation-specific receiver URLs. Runtime credentials belong in NVS or an ignored private configuration file.
