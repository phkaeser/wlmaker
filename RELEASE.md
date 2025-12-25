# Instructions for releasing wlmaker

* Update [CMakeLists.txt]:
  * Set a new `VERSION` in the `PROJECT` directive.

* Update [doc/ROADMAP.md]:
  * Updates the roadmap's section title to a link to the new release tag.

* Update [README.md]:
  * Update section with highlights for the current version.
  * Remove the `*new*` prefix from the earlier version's highlights.

* On https://github.com/phkaeser/wlmaker/releases, *Draft a new release*
  * Create a new version tag, in format `v<major>.<minor>`
  * Update the release title to `v<major>.<minor> - <description>`

Pending: Generate release notes.
