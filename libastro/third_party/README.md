# Vendored third-party code

Header-only dependencies are checked in here so the build is offline and
reproducible (the same policy as the `mdspan` fallback in `cmake/`).

| Dir        | Project                                                    | Version | License | Used by |
|------------|------------------------------------------------------------|---------|---------|---------|
| `argparse` | [p-ranav/argparse](https://github.com/p-ranav/argparse)    | v3.1    | MIT     | `cli/astro` |

Each header carries its own upstream copyright and license text. To update,
replace the single header with the corresponding tagged release and bump the
version above.
