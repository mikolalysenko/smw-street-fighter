# Publish a release and the browser demo

`main` contains the source and README. `gh-pages` contains the compiled static
demo, including its packed game assets, credits, and `.nojekyll`. The browser
build uses relative URLs, so it works under a GitHub project path. WebGPU uses
HTTPS; Canvas 2D is available as a fallback.

Preparing the branch is local. It does not create a remote, push commits, or
change GitHub settings.

## Repository

Source: https://github.com/mikolalysenko/smw-street-fighter

Demo: https://mikolalysenko.github.io/smw-street-fighter/

The demo deploys from **gh-pages → /(root)**. Downloadable browser builds are
attached to GitHub releases.

## Desktop release

The [Native builds workflow](../.github/workflows/build.yml) produces complete
packages on Linux x86-64, Windows x86-64, macOS ARM64 and macOS Intel runners.
Each archive includes data, SDL libraries, configuration, a player guide and
license notices. Each platform boots its relocated package twice and verifies
save creation/reload before uploading an artifact. Macs use ad-hoc signatures,
not Apple notarization; Windows builds are not publisher-signed.

CI retrieves `runtime-data.zip` from the published `demo-2026-10-12` release. That
archive contains the same packed game data as the browser demo, plus a SHA-256
manifest. It contains no ROM images or saves. To prepare a changed asset bundle:

```sh
python3 dev/package_native.py --data-only
```

Publish that bundle on a release accessible to the workflow's read-only token,
then update the workflow's source tag. GitHub's read-only workflow token cannot
retrieve assets from a draft release. Bump `--version` in the workflow and local
packager together. Finish gameplay/media/docs changes and commit before building
the final release; `build.json` in every archive identifies its source revision.

```sh
git push origin main
gh run list --workflow build.yml
gh run download RUN_ID --dir build/release-artifacts
make pages-branch
```

Collect the four native archives and `dist/super-mario-world-ii-web.zip`, generate
`SHA256SUMS`, and attach them to a draft release with notes and OS requirements.
Verify the artifacts came from the intended commit and that every platform job
passed. Publish the release as Latest only once all download links are ready.
Keep raw capture masters in `build/media/`; the optimized teaser and screenshots
under `media/` are included in source and Pages. See the [media guide](media/README.md).

Desktop saves live outside the install directory. Never copy a developer's
`saves/` folder into a package. The packager checks for saves and raw ROM suffixes.

## First push to a new repository

For a fresh fork, sign into GitHub CLI when ready. If Homebrew's `gh` is installed but not on PATH, first run
`export PATH="$(brew --prefix gh)/bin:$PATH"`.

Run these commands from the repository root:

```sh
gh auth login --hostname github.com --git-protocol https --web
gh repo create smw-street-fighter --public --source=. --remote=origin \
  --description "Super Mario World II — The World Warrior: a twelve-fighter crossover in C and WebAssembly"
git push -u origin main gh-pages
gh api --method POST 'repos/{owner}/{repo}/pages' \
  -f build_type=legacy -f 'source[branch]=gh-pages' -f 'source[path]=/'
gh api 'repos/{owner}/{repo}/pages' --jq '.html_url'
```

The repository is created under the signed-in account; change the repo name or
use `OWNER/NAME` in `gh repo create` if desired. If `origin` already exists, skip
repository creation and inspect `git remote -v` before pushing.

Pages should use **Deploy from a branch → gh-pages → /(root)**. The `.nojekyll`
file serves the build as static files. The first deployment can take a few
minutes. Inspect it with:

```sh
gh api 'repos/{owner}/{repo}/pages/builds/latest' --jq '{status, error}'
gh run list --limit 5
```

Once hosting is live, put the returned demo URL near the top of README.md and
set it as the repository's website with `gh repo edit --homepage URL`.
Open the site, click Play, confirm the build stamp, and test character select,
one level and sound. Saves at localhost and GitHub Pages use separate browser
storage; Export Save preserves a copy of an existing local save.

## Update the demo

Commit source changes first, then run:

```sh
make pages-branch
git push origin main gh-pages
```

`make pages-branch` builds WASM, runs the browser-engine tests in Node, and copies
an explicit list of runtime files into a temporary Git worktree. It updates
`gh-pages` with a normal commit, preserving deployment history. It leaves the
active checkout on its original branch and does not push. `build.json` records
the exact source revision and timestamp; dirty or stale builds are rejected.

The deployment contains the compiled runtime and packed assets, not local
saves, source downloads, raw ROM files, or native application bundles. Credits
and licenses travel with the demo. See [ASSETS.md](ASSETS.md) for provenance.

GitHub publishes later pushes from `gh-pages` automatically. The native build
workflow runs on `main` and pull requests independently of Pages.

GitHub's [branch publishing documentation](https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site)
and [Pages API reference](https://docs.github.com/en/rest/pages/pages) describe
the hosting configuration used here.
