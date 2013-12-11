var Binary = require('./binary_name.js').Binary;
var binary = new Binary({name:'icu_bidi'});
var bindings;
try {
  bindings = require(binary.getRequirePath('Debug'));
} catch (err) {
  bindings = require(binary.getRequirePath('Release'));
}

exports.foo = 'bar';
exports.hello = bindings.hello;
