# icu-bidi
[![NPM][NPM1]][NPM2]

[![Build Status][1]][2] [![dependency status][3]][4] [![dev dependency status][5]][6]

The node `icu-bidi` package binds to the [ICU] (52.1) library in order to
provide an implementation of the Unicode [BiDi] algorithm.

# USAGE

The JavaScript API follows the
[API of `icu4c`](http://icu-project.org/apiref/icu4c/ubidi_8h.html)
fairly closely.

```js
var ubidi = require('icu-bidi');

var e = 'English';
var h = 'עִבְרִית';
var input = e + ' ' + h;

console.log( input );
var p = ubidi.Paragraph(input, {
  // this hash is optional; these are the default values:
  paraLevel:         ubidi.DEFAULT_LTR,
  reorderingMode:    ubidi.ReorderingMode.DEFAULT,
  reorderingOptions: 0,
  inverse:           false,
  prologue:          '',
  epilogue:          '',
  embeddingLevels:   null /* Unimplemented */
});
console.log( 'number of paragraphs', p.countParagraphs() );
console.log( 'paragraph level',      p.getParaLevel()    );
// direction is 'ltr', 'rtl', or 'mixed'
console.log( 'direction',            p.getDirection()    );

var i, levels = [];
for (i=0; i < p.getProcessedLength(); i++) {
  levels.push( p.getLevelAt(i) );
}
console.log( levels.join(' ') );

for (i=0; i < p.countRuns(); i++) {
  var run = p.getVisualRun(i);
  console.log( 'run', run.dir, 'from', run.logicalStart, 'len', run.length );
}

console.log( p.writeReordered(ubidi.Reordered.DO_MIRRORING) );
```

This example prints the following when run:
```
English עִבְרִית
number of paragraphs 1
paragraph level 0
direction mixed
0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1
run ltr from 0 len 8
run rtl from 8 len 8
English תיִרְבִע
```

# INSTALLING

You can use [`npm`](https://github.com/isaacs/npm) to download and install:

* The latest `icu-bidi` package: `npm install icu-bidi`

* GitHub's `master` branch: `npm install https://github.com/cscott/node-icu-bidi/tarball/master`

In both cases the module is automatically built with npm's internal version of `node-gyp`,
and thus your system must meet [node-gyp's requirements](https://github.com/TooTallNate/node-gyp#installation).

It is also possible to make your own build of `icu-bidi` from its source instead of its npm package ([see below](#building-from-the-source)).

# BUILDING FROM THE SOURCE

Unless building via `npm install` (which uses its own `node-gyp`) you will need `node-gyp` installed globally:

    npm install node-gyp -g

The `icu-bidi` module depends only on `libicu`. However, by default, an internal/bundled copy of `libicu` will be built and statically linked, so an externally installed `libicu` is not required.

If you wish to install against an external `libicu` then you need to
pass the `--libicu` argument to `node-gyp`, `npm install` or the
`configure` wrapper.

    ./configure --libicu=external
    make

Or, using the node-gyp directly:

     node-gyp --libicu=external rebuild

Or, using npm:

     npm install --libicu=external

If building against an external `libicu` make sure to have the
development headers available. Mac OS X ships with these by
default. If you don't have them installed, install the `-dev` package
with your package manager, e.g. `apt-get install libicu-dev` for
Debian/Ubuntu. Make sure that you have at least `libicu` >= 52.1

# TESTING

[mocha](https://github.com/visionmedia/mocha) is required to run unit tests.

    npm install mocha
    npm test


# CONTRIBUTORS

* [C. Scott Ananian](https://github.com/cscott)

# RELATED PROJECTS

* [GNU FriBiDi](http://fribidi.org/)

# LICENSE

`icu-bidi` is licensed using the same [ICU license] as the libicu library
itself.  It is an MIT/X-style license.

[ICU]:         http://icu-project.org/
[ICU license]: http://source.icu-project.org/repos/icu/icu/trunk/license.html
[BiDi]:        http://www.unicode.org/unicode/reports/tr9/

[NPM1]: https://nodei.co/npm/icu-bidi.png
[NPM2]: https://nodei.co/npm/icu-bidi/

[1]: https://travis-ci.org/cscott/node-icu-bidi.png
[2]: https://travis-ci.org/cscott/node-icu-bidi
[3]: https://david-dm.org/cscott/node-icu-bidi.png
[4]: https://david-dm.org/cscott/node-icu-bidi
[5]: https://david-dm.org/cscott/node-icu-bidi/dev-status.png
[6]: https://david-dm.org/cscott/node-icu-bidi#info=devDependencies
