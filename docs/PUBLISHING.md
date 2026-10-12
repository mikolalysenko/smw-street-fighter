# Publish the WASM demo on GitHub Pages

`main` contains the source and README. `gh-pages` contains the compiled static
demo, including its packed game assets, credits, and `.nojekyll`. The browser
build uses relative URLs, so it works under a GitHub project path. WebGPU uses
HTTPS; Canvas 2D is available as a fallback.

Preparing the branch is local. It does not create a remote, push commits, or
change GitHub settings.

## First push

The local `main` and `gh-pages` branches are prepared. Sign into GitHub CLI when
ready. If Homebrew's `gh` is installed but not on PATH, first run
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
