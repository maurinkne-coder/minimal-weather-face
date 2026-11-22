`pebble-clay.js` is the prebuilt JavaScript of Clay 1.0.4 (https://github.com/pebble/clay, MIT licence, see `pebble-clay.LICENSE`).

It is copied here instead of installed from npm because the npm package also
declares a C library that only lists the old platforms (aplite, basalt, chalk,
diorite, emery), so the SDK refuses to build for Round 2 (gabbro). This face only
uses Clay's phone-side JavaScript.
