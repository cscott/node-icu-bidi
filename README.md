# icu-bidi
[![NPM][NPM1]][NPM2]

[![Build Status][1]][2] [![dependency status][3]][4] [![dev dependency status][5]][6]

The node `icu-bidi` package binds to the [ICU][] (52.1) library in order to
provide an implementation of the Unicode [BiDi][] algorithm.

# USAGE

The JavaScript API follows the
[API of `icu4c`](http://icu-project.org/apiref/icu4c/ubidi_8h.html)
fairly closely.

```js
var ubidi = require('icu-bidi');

var e = 'English';
var h = '‭עִבְרִית‬';
var input = e + ' ' + h;

console.log( input );
var p = ubidi.Paragraph(input, {
  // this hash is optional; these are the default values:
  paraLevel:          ubidi.DEFAULT_LTR,
  reorderingMode:     ubidi.ReorderingMode.DEFAULT,
  reorderingOptions:  0, // uses ubidi.ReorderingOptions.*
  orderParagraphsLTR: false,
  inverse:            false,
  prologue:           '',
  epilogue:           '',
  embeddingLevels:    null /* Unimplemented */
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

console.log( p.writeReordered(ubidi.Reordered.KEEP_BASE_COMBINING) );
```

This example prints the following when run:
```
English ‭עִבְרִית‬
number of paragraphs 1
paragraph level 0
direction mixed
0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1
run ltr from 0 len 8
run rtl from 8 len 8
English ‭תירִבְעִ‬
```

# API

## new ubidi.Paragraph(text, [options])

Returns a new `Paragraph` object with the results of the bidi algorithm.
*   `text`: UTF-16 encoded text as a standard JavaScript string; you know
    the drill.
*   `options` *(optional)*: a hash containing various settings which can
    affect the bidi algorithm.  All are optional.
    - [`paraLevel`][ubidi_setPara]:
        Specifies the default [level][UBiDiLevel] for the text; it is
        typically 0 (LTR) or 1 (RTL). If the function shall determine
        the paragraph level from the text, then paraLevel can be set
        to either `ubidi.DEFAULT_LTR` or `ubidi.DEFAULT_RTL`.
    - [`reorderingMode`][ubidi_setReorderingMode]:
        Modify the operation of the Bidi algorithm such that it implements
        some variant to the basic Bidi algorithm or approximates an
        "inverse Bidi" algorithm, depending on different values of the
        "reordering mode".
        - `ubidi.ReorderingMode.DEFAULT`:
            The standard Bidi Logical to Visual algorithm is applied.
        - `ubidi.ReorderingMode.REORDER_NUMBERS_SPECIAL`:
            Approximate the algorithm used in Microsoft Windows XP rather
            than strictly conform to the Unicode Bidi algorithm.
        - `ubidi.ReorderingMode.GROUP_NUMBERS_WITH_R`:
            Numbers located between LTR text and RTL text are
            associated with the RTL text.  This makes the algorithm
            reversible and makes it useful when round trip must be
            achieved without adding LRM characters. However, this is a
            variation from the standard Unicode Bidi algorithm.
        - `ubidi.ReorderingMode.RUNS_ONLY`:
            Logical-to-Logical transformation. If the default text
            level of the source text (`paraLevel`) is even, the source
            text will be handled as LTR logical text and will be
            transformed to the RTL logical text which has the same LTR
            visual display.  If the default level of the source text
            is odd, the source text will be handled as RTL logical
            text and will be transformed to the LTR logical text which
            has the same LTR visual display.
        - `ubidi.ReorderingMode.INVERSE_NUMBERS_AS_L`:
            An "inverse Bidi" algorithm is applied. This mode is
            equivalent to setting the `inverse` option to `true`.
        - `ubidi.ReorderingMode.INVERSE_LIKE_DIRECT`:
            The "direct" Logical to Visual Bidi algorithm is used as
            an approximation of an "inverse Bidi" algorithm.
        - `ubidi.ReorderingMode.INVERSE_FOR_NUMBERS_SPECIAL`:
            The Logical to Visual Bidi algorithm used in Windows XP is
            used as an approximation of an "inverse Bidi" algorithm.
    - [`reorderingOptions`][ubidi_setReorderingOptions]:
        Specify which of the reordering options should be applied
        during Bidi transformations.  The value is a combination using
        bitwise OR of zero or more of the following:
        - [`ubidi.ReorderingOptions.DEFAULT`][UBiDiReorderingOption]
            Disable all the options which can be set with this function.
        - [`ubidi.ReorderingOptions.INSERT_MARKS`][UBiDiReorderingOption]
            Insert Bidi marks (LRM or RLM) when needed to ensure correct
            result of a reordering to a Logical order.
        - [`ubidi.ReorderingOptions.REMOVE_CONTROLS`][UBiDiReorderingOption]
            Remove Bidi control characters.
        - [`ubidi.ReorderingOptions.STREAMING`][UBiDiReorderingOption]
            Process the output as part of a stream to be continued.
    - [`orderParagraphsLTR`][ubidi_orderParagraphsLTR]: *(boolean)*
        Specify whether block separators must be allocated level zero, so
        that successive paragraphs will progress from left to right.
    - [`inverse`][ubidi_setInverse]: *(boolean)*
        Modify the operation of the Bidi algorithm such that it approximates
        an "inverse Bidi" algorithm.
    - [`prologue`][ubidi_setContext]: *(string)*
        A preceding context for the given text.
    - [`epilogue`][ubidi_setContext]: *(string)*
        A trailing context for the given text.
    - [`embeddingLevels`][ubidi_setPara]: *(array)*
        CURRENTLY UNIMPLEMENTED.

## Paragraph#setLine(start, limit)

Returns a subsidiary `Paragraph` object containing the reordering
information, especially the resolved levels, for all the characters in
a line of text.

This line of text is specified by referring to a `Paragraph` object
representing this information for a piece of text containing one or
more paragraphs, and by specifying a range of indexes in this text.

In the returned line object, the indexes will range from `0` to `limit-start-1`.

This is used after creating a `Paragraph` for a piece of text, and
after line-breaking on that text. It is not necessary if each
paragraph is treated as a single line.

After line-breaking, rules (L1) and (L2) for the treatment of trailing
WS and for reordering are performed on the returned `Paragraph` object
representing a line.

*   `start`:
    The line's first index into the text
*   `limit`:
    The index just behind the line's last index into the text (its last
    index +1).  It must be `0<=start<limit<=containing paragraph limit`.
    If the specified line crosses a paragraph boundary, the function will
    throw an Exception.

See [the icu docs][ubidi_setLine] for more information.

## Paragraph#getDirection()

Get the directionality of the text.

Returns `'ltr'` or `'rtl'` if the text is unidirectional; otherwise returns
`'mixed'`.

See [the icu docs][ubidi_getDirection] for more information.

## Paragraph#getLength()

Returns the length of the text that the `Paragraph` object was created for.

See [the icu docs][ubidi_getLength] for more information.

## Paragraph#getProcessedLength()

Get the length of the source text processed by the `Paragraph` constructor.

This length may be different from the length of the source text if option
`ubidi.ReorderingOptions.STREAMING` has been set.

See [the icu docs][ubidi_getProcessedLength] for more information.

## Paragraph#getResultLength()

Get the length of the reordered text resulting from `Paragraph` constructor.

This length may be different from the length of the source text if
option `ubidi.ReorderingOptions.INSERT_MARKS` or option
`ubidi.ReorderingOptions.REMOVE_CONTROLS` has been set.

See [the icu docs][ubidi_getResultLength] for more information.

## Paragraph#getParaLevel()

Returns the paragraph [level][UBiDiLevel]. If there are multiple
paragraphs, their level may vary if the required `paraLevel` is
`ubidi.DEFAULT_LTR` or `ubidi.DEFAULT_RTL`. In that case, the level of
the first paragraph is returned.

See [the icu docs][ubidi_getParaLevel] for more information.

## Paragraph#countParagraphs()

Returns the number of paragraphs.

See [the icu docs][ubidi_countParagraphs] for more information.

## Paragraph#getParagraph(charIndex)

Get information about a paragraph, given a position within the text which
is contained by that paragraph.

Returns an object with the following properties:
*   `index`:
    The index of the paragraph containing the specified position.
*   `start`:
    The index of the first character of the paragraph in the text.
*   `limit`:
    The limit of the paragraph.
*   `level`:
    The [level][UBiDiLevel] of the paragraph.
*   `dir`:
    The directionality of the run, either `'ltr'` or `'rtl'`.  This is
    derived from bit 0 of the level; see [UBiDiLevel][].

See [the icu docs][ubidi_getParagraph] for more information.

## Paragraph#getParagraphByIndex(paraIndex)

Get information about a paragraph, given the index of the paragraph.

Returns an object with the same properties as `getParagraph`, above.

See [the icu docs][ubidi_getParagraphByIndex] for more information.

## Paragraph#getLevelAt(charIndex)

Return the [level][UBiDiLevel] for the character at `charIndex`, or `0` if
`charIndex` is not in the valid range.

See [the icu docs][ubidi_getLevelAt] for more information.

## Paragraph#countRuns()

Returns the number of runs in this text.

See [the icu docs][ubidi_countRuns] for more information.

## Paragraph#getVisualRun(runIndex)

Get one run's logical start, length, and directionality (`'ltr'` or `'rtl'`).

In an RTL run, the character at the logical start is visually on the
right of the displayed run. The length is the number of characters in
the run. The `runIndex` parameter is the number of the run in visual
order, in the range `[0..Paragraph#countRuns()-1]`.

Returns an object with the following properties:
*   `dir`:
    The directionality of the run, either `'ltr'` or `'rtl'`.
*   `logicalStart`:
    The first logical character index in the text.
*   `length`:
    The number of characters (at least one) in the run.

See [the icu docs][ubidi_getVisualRun] for more information.

## Paragraph#getLogicalRun(logicalPosition)

This function returns information about a run and is used to retrieve
runs in logical order.  This is especially useful for line-breaking on
a paragraph.

Returns an object with the following properties:
*   `logicalLimit`:
    The limit of the corresponding run.
*   `level`:
    The [level][UBiDiLevel] of the corresponding run.
*   `dir`:
    The directionality of the run, either `'ltr'` or `'rtl'`.  This is
    derived from bit 0 of the level; see [UBiDiLevel][].

See [the icu docs][ubidi_getLogicalRun] for more information.

## Paragraph#getVisualIndex(logicalIndex)

Get the visual position from a logical text position.

The value returned may be `ubidi.MAP_NOWHERE` if there is no visual
position because the corresponding text character is a Bidi control
removed from output by the option
`ubidi.ReorderingOptions.REMOVE_CONTROLS`.

See [the icu docs][ubidi_getVisualIndex] for more information.

## Paragraph#getLogicalIndex(visualIndex)

Get the logical text position from a visual position.

The value returned may be `ubidi.MAP_NOWHERE` if there is no logical
position because the corresponding text character is a Bidi mark
inserted in the output by option
`ubidi.ReorderingOptions.INSERT_MARKS`.

See [the icu docs][ubidi_getLogicalIndex] for more information.

## Paragraph#writeReordered([options])

Take a `Paragraph` object containing the reordering information for a
piece of text (one or more paragraphs) set by `new Paragraph` or for a
line of text set by `Paragraph#setLine()` and returns a reordered string.

This function preserves the integrity of characters with multiple code
units and (optionally) combining characters. Characters in RTL runs
can be replaced by mirror-image characters in the returned
string. There are also options to insert or remove Bidi control
characters.

The `options` argument is optional.  If it is present, it is a bit set
of options for the reordering that control how the reordered text is
written. The available options are:
- [`ubidi.Reordered.KEEP_BASE_COMBINING`][UBIDI_KEEP_BASE_COMBINING]
    Keep combining characters after their base characters in RTL runs.
- [`ubidi.Reordered.DO_MIRRORING`][UBIDI_DO_MIRRORING]
    Replace characters with the "mirrored" property in RTL runs by their
    mirror-image mappings.
- [`ubidi.Reordered.INSERT_LRM_FOR_NUMERIC`][UBIDI_INSERT_LRM_FOR_NUMERIC]
    Surround the run with LRMs if necessary; this is part of the approximate
    "inverse Bidi" algorithm.
- [`ubidi.Reordered.REMOVE_BIDI_CONTROLS`][UBIDI_REMOVE_BIDI_CONTROLS]
    Remove Bidi control characters (this does not affect
    `ubidi.Reordered.INSERT_LRM_FOR_NUMERIC`).
- [`ubidi.Reordered.OUTPUT_REVERSE`][UBIDI_OUTPUT_REVERSE]
    Write the output in reverse order.

See [the icu docs][ubidi_writeReordered] for more information.

[ubidi_setPara]:              http://icu-project.org/apiref/icu4c/ubidi_8h.html#abdfe9e113a19dd8521d3b7ac8220fe11
[ubidi_setReorderingMode]:    http://icu-project.org/apiref/icu4c/ubidi_8h.html#afe123acc1196c4d7363f968ca6af6faa
[ubidi_setReorderingOptions]: http://icu-project.org/apiref/icu4c/ubidi_8h.html#a25dd2aba9db100133217b9fe76de01de
[ubidi_orderParagraphsLTR]:   http://icu-project.org/apiref/icu4c/ubidi_8h.html#ab7b9785b85169b3830034029729c672e
[ubidi_setInverse]:           http://icu-project.org/apiref/icu4c/ubidi_8h.html#a836b2eaf83ca712cf28e69cd4ba934f4
[ubidi_setContext]:           http://icu-project.org/apiref/icu4c/ubidi_8h.html#a1e38e9d7036f4aa7cc5aea5a435b3e63
[UBiDiReorderingOption]:      http://icu-project.org/apiref/icu4c/ubidi_8h.html#a4505e4adc8da792501414b770f49f386
[ubidi_setLine]:              http://icu-project.org/apiref/icu4c/ubidi_8h.html#ac7d96b281cd6ab2d56900bfdc37c808a
[ubidi_getDirection]:         http://icu-project.org/apiref/icu4c/ubidi_8h.html#af31ec52194764c663c224f5171e95ea3
[ubidi_getLength]:            http://icu-project.org/apiref/icu4c/ubidi_8h.html#a8f51ac46083e7ce52b6bea4bc7ac14a8
[ubidi_getProcessedLength]:   http://icu-project.org/apiref/icu4c/ubidi_8h.html#abf3d2acd9d73fb4a3a25deb0ebca28d5
[ubidi_getResultLength]:      http://icu-project.org/apiref/icu4c/ubidi_8h.html#a3247782277731ee82cfb3ba700f598a8
[ubidi_getParaLevel]:         http://icu-project.org/apiref/icu4c/ubidi_8h.html#a6724e673e9ff8f0ee47bd24e47ceb95a
[ubidi_countParagraphs]:      http://icu-project.org/apiref/icu4c/ubidi_8h.html#a8f4b5bb9a8e37d8065490af4e6825563
[ubidi_getParagraph]:         http://icu-project.org/apiref/icu4c/ubidi_8h.html#a5cd3d78464b8e3b71886a643f70f25ab
[ubidi_getParagraphByIndex]:  http://icu-project.org/apiref/icu4c/ubidi_8h.html#a62377f811a750130246dfb49c1cc6dc0
[ubidi_getLevelAt]:           http://icu-project.org/apiref/icu4c/ubidi_8h.html#ad363767eacb66359de7c639a722338c8
[ubidi_countRuns]:            http://icu-project.org/apiref/icu4c/ubidi_8h.html#a18c2f5cfaf8c8717759d6e0feaa58c99
[ubidi_getVisualRun]:         http://icu-project.org/apiref/icu4c/ubidi_8h.html#ae923ec697e2eb77652fca9f1fcddc894
[ubidi_getLogicalRun]:        http://icu-project.org/apiref/icu4c/ubidi_8h.html#aaa99079b617dcc6c15910558306b7145
[ubidi_getVisualIndex]:       http://icu-project.org/apiref/icu4c/ubidi_8h.html#a17696c56f06e1a48270f0ff3b69edd79
[ubidi_getLogicalIndex]:      http://icu-project.org/apiref/icu4c/ubidi_8h.html#a95ad84e638be70e73b23809fc132582f
[ubidi_writeReordered]:       http://icu-project.org/apiref/icu4c/ubidi_8h.html#a26790ff71c59f223ded4047da5626725
[UBIDI_KEEP_BASE_COMBINING]:  http://icu-project.org/apiref/icu4c/ubidi_8h.html#a2e022ccd0d2c55a21c2aa233c30ecd88
[UBIDI_DO_MIRRORING]:         http://icu-project.org/apiref/icu4c/ubidi_8h.html#a0b1370dda1e3ad8ef9c94fd28320153d
[UBIDI_INSERT_LRM_FOR_NUMERIC]: http://icu-project.org/apiref/icu4c/ubidi_8h.html#adad66f9132bc4e4621427091acfc0f40
[UBIDI_REMOVE_BIDI_CONTROLS]: http://icu-project.org/apiref/icu4c/ubidi_8h.html#a039000c1e298cbad5909d07a55ca5312
[UBIDI_OUTPUT_REVERSE]:       http://icu-project.org/apiref/icu4c/ubidi_8h.html#a4a10c3aac68ceca1569bac717156cef3
[UBiDiLevel]:                 http://icu-project.org/apiref/icu4c/ubidi_8h.html#ab2460a19f323ab9787a79a95db91a606

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

* [ICU][]
* [GNU FriBiDi](http://fribidi.org/)

# LICENSE
Copyright (c) 2013 C. Scott Ananian.

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
