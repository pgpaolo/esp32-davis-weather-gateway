# Branch and release policy

## Branches

- `main` - reviewed, CI-green integration baseline intended for users and releases.
- `develop` - active integration branch for the next project state.
- feature/fix branches - created from `develop` and merged back through pull requests.

## Promotion to `main`

A `develop -> main` pull request should be merged only when:

1. firmware CI is green for every supported PlatformIO target;
2. documentation CI is green;
3. protocol/configuration changes are documented in both Italian and English where applicable;
4. no credentials, installation-specific endpoints or private values are committed;
5. reverse-engineered assumptions are clearly separated from field-verified behaviour;
6. required license and attribution files are present;
7. unresolved review threads are closed.

## Recommended `main` protection

Configure a GitHub ruleset or branch protection rule for `main` with:

- require a pull request before merging;
- require at least 1 approving review;
- dismiss stale approvals when new commits are pushed;
- require review from Code Owners;
- require conversation resolution;
- require status checks to pass before merging;
- require the firmware build checks for both supported LILYGO targets;
- require the documentation build check when applicable;
- require the branch to be up to date before merging;
- block force pushes;
- block branch deletion;
- apply rules to administrators unless an emergency bypass is deliberately configured.

Direct pushes to `main` are not part of the normal workflow.

## Commit style

Use short imperative subjects and keep unrelated changes separate. Examples:

- `Fix Davis hop resynchronization`
- `Document Wi-Fi recovery portal`
- `Update RF protocol validation notes`

Release or promotion pull requests should summarize hardware impact, protocol impact, configuration migration and validation status.
