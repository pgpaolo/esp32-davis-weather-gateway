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
5. update documentation when protocol behaviour or configuration changes;
6. pass the PlatformIO CI builds for all supported targets.

For RF changes, include serial traces or other reproducible evidence when possible. Clearly distinguish verified behaviour from assumptions or reverse-engineered details that still require hardware validation.

## Commit style

Use short imperative commit subjects, for example:

- `Fix Davis hop resynchronization`
- `Document captive portal recovery`
- `Add BME280 altitude validation`

Keep unrelated changes in separate commits where practical.

## Licensing

By contributing, you agree that your original contribution may be distributed under the repository license, `LGPL-3.0-only`, unless explicitly agreed otherwise.

Do not copy code from third-party projects unless its license is compatible and the required attribution/license obligations are documented. Protocol facts and public interoperability information should be cited in the technical documentation when relevant.
