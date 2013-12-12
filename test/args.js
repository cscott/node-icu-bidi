// Check argument handling.
require('should');

describe('Argument handling', function() {
    var bidi = require('../');
    it('should fail if no arguments are given', function() {
        (function() {
            bidi.analyze();
        }).should.throw();
    });
    it('should fail if the first argument can\'t be a string', function() {
        (function() {
            bidi.analyze(5); // okay
        }).should.not.throw();
        (function() {
            bidi.analyze({
                toString: function() { throw new Error('boo'); }
            });
        }).should.throw();
    });
});
