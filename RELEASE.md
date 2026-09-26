# RELEASE.md — cutting a wgwatch release

Tagged `v*` pushes run the full CI plus the `release` job, which attaches
the built `.deb` / `.rpm` to the GitHub release. The version lives in
three places; bump all three together or the release mixes versions.

## Files carrying the version

- `CMakeLists.txt`: `project(wgwatch VERSION X.Y.Z ...)` — drives the
  binary, the test build, and the CPack package filenames.
- `package.nix`: `version = "X.Y.Z";` — drives the nix package name.
- `README.md` (`## Packages`): the exact `apt` / `dnf` filenames, e.g.
  `wgwatch_X.Y.Z_amd64.deb` and `wgwatch-X.Y.Z-1.x86_64.rpm`.

## Procedure

```sh
# 1. bump the three files above to X.Y.Z
make test                                # configure + build + 44 tests
git add -p && git commit -m "Release X.Y.Z"
git tag -a vX.Y.Z -m "wgwatch X.Y.Z"
git push origin HEAD --tags
gh run watch                             # all 5 jobs; release needs
                                         # debian + fedora green
```

## Notes

- The `release` job only runs on `refs/tags/v*`; ordinary branch pushes
  just build and test.
- README download links point at the `releases/latest` page, which needs
  no per-release update — but the copy-paste `apt`/`dnf` filenames do,
  hence step 1.
- Never commit or push without being asked; tagging is part of that rule.
