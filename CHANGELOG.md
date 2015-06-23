# node-icu-bidi x.x.x (not yet released)
* Use `nan` to support node versions > 0.10

# node-icu-bidi 0.1.3 (2014-09-16)
* Update to icu-bidi 53.1
* Fix build issues on OS X
* Use `node-pre-gyp` for build automation

# node-icu-bidi 0.1.2 (2014-01-10)
* Add `dir` property to return values with `level` property.
  The directionality is derived from bit 0 of the level, as the docs for
  `UBiDiLevel` specify.  Make this an explicit field in the result for the
  convenience of users.

# node-icu-bidi 0.1.1 (2013-12-13)
* API additions: added lots of missing methods.

# node-icu-bidi 0.1.0 (2013-12-13)
* Initial release
