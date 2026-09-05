# Contributing

Thank you for contributing to ESP32 Davis Weather Gateway.

## Branch model

- `main`: reviewed and CI-validated project state.
- `develop`: active integration branch.
- Feature/fix branches: branch from `develop` and open a pull request back to `develop`.
- Promotion to `main`: use a dedicated `develop -> main` pull request after CI and review.

Direct pushes to `main` should remain disabled through repository branch protection/rulesets.

## Pull requests

A pull request should:

1. explain the problem and proposed change;
2. identify affected hardware targets;
3. avoid installation-specific URLs, Wi-Fi credentials, private IP profiles or secrets except documented generic defaults;
4. keep public configuration generic and user-configurable;
5. update documentation when protocol behaviour, diagnostics, storage, configuration or security changes;
6. pass PlatformIO CI for all supported targets;
7. identify any source copied, adapted or materially derived from third-party work and document the applicable license/attribution before merge.

For RF changes, include serial traces, `davis-diagnostic.txt`, captures or other reproducible evidence when possible. Do not change the validated Davis RF engine without evidence that the change is required.

## Commit style

Use short imperative subjects, for example:

- `Fix Davis hop resynchronization`
- `Add microSD write retry`
- `Refine dashboard pressure card`
- `Document remote relay security model`

Keep unrelated changes in separate commits where practical.

## Licensing and provenance

By contributing, you agree that your original contribution may be distributed under **GNU LGPL v3.0 only (`LGPL-3.0-only`)**, unless explicitly agreed otherwise.

Do not copy third-party code unless its license is compatible with the intended distribution and all required notices are documented. Protocol facts and publicly observed interoperability behaviour may be implemented independently, but relevant technical sources should be cited when they materially inform implementation.

New project-original source files should use, where practical:

```text
SPDX-License-Identifier: LGPL-3.0-only
```

Do not apply the project SPDX identifier to third-party files carrying a different license.

See `NOTICE.md`, `THIRD_PARTY_NOTICES.md`, `docs/LICENSING_IT.md` and `docs/LICENSING_EN.md`.
