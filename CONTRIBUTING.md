# Contributing

Thanks for helping build Cortex and CoChat!

- Code of Conduct: see CODE_OF_CONDUCT.md
- License:
    - Cortex IDE core + libs: Apache-2.0 (or MPL‑2.0 if chosen)
    - CoChat server: AGPL-3.0
    - By contributing, you agree your contributions are licensed under the appropriate project license.
- How to contribute:
    - Small PRs preferred; add tests if feasible.
    - Write clear commit messages (Conventional Commits encouraged).
    - For larger changes, open a discussion/issue first.

Development quick start:
- Desktop (Windows): `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`
- CLI capture example: `.\build\cortex_em_cli.exe --capture 1 --live --seconds 10 --fps 30 --resize 1680x720 --record capture\frame`

Sign-off:
- We use DCO. Sign your commits with `git commit -s`.