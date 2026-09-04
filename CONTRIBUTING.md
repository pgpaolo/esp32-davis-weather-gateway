# Contributing

Thank you for contributing to ESP32 Davis Weather Gateway.

## Branch model

- `main`: reviewed and CI-validated project state.
- `develop`: active integration branch.
- Feature/fix branches: branch from `develop` and open a pull request back to `develop`.
- Promotion to `main`: use a dedicated `develop -> main` pull request after CI and review.

Direct pushes to `main` should be disabled through repository branch protection/rulesets.

## Pull requests

A pull request should:

1. explain the problem and the proposed change;
2. identify affected hardware targets;
3. avoid installation-specific URLs, Wi-Fi credentials, IP addresses or secrets except documented generic defaults;
4. keep public configuration generic and user-configurable;
5. update documentation when protocol behaviour, diagnostics, configuration or licensing/provenance changes;
6. pass the PlatformIO CI builds for all supported targets;
7. identify any source copied, adapted or materially derived from a third-party project and document the applicable license/attribution before merge.

For RF changes, include serial traces, `davis-diagnostic.txt`, captures or other reproducible evidence when possible. Clearly distinguish verified behaviour from assumptions or reverse-engineered details that still require hardware validation.

## Commit style

Use short imperative commit subjects, for example:

- `Fix Davis hop resynchronization`
- `Document captive portal recovery`
- `Add BME280 altitude validation`
- `Extend RF diagnostic report`

Keep unrelated changes in separate commits where practical.

## Licensing and provenance

By contributing, you agree that your original contribution may be distributed under the repository license, **GNU LGPL v3.0 only (`LGPL-3.0-only`)**, unless explicitly agreed otherwise.

Project-specific attribution and provenance are documented in:

- `NOTICE.md`
- `THIRD_PARTY_NOTICES.md`
- `docs/LICENSING_IT.md`
- `docs/LICENSING_EN.md`

Do not copy code from third-party projects unless its license is compatible with the intended distribution and all required notices/obligations are documented.

Protocol facts and publicly observed interoperability behaviour may be implemented independently, but relevant technical sources should be cited in documentation when they materially inform the implementation.

### Oregon / Technoline boundary

The Davis repository follows the same explicit-attribution practice used by the maintainer's Oregon/Technoline project, but it does not import that project's third-party GPL decoder material.

In particular, do not add Oregon Scientific, Technoline/LaCrosse, `rtl_433`-derived or PracticalArduino-derived decoder code to the Davis weather RF engine without a deliberate project-level licensing decision.

### SPDX

New project-original source files should use, where practical:

```text
SPDX-License-Identifier: LGPL-3.0-only
```

Do not apply the project SPDX identifier to third-party files that retain a different license.
