// Check argument handling.
require('should');

describe('Argument handling', function() {
    var bidi = require('../');
    it('should fail if no arguments are given', function() {
        (function() {
            var p = bidi.Paragraph();
        }).should.throw();
        (function() {
            var p = new bidi.Paragraph();
        }).should.throw();
    });
    it('should fail if the first argument can\'t be a string', function() {
        (function() {
            bidi.Paragraph(5); // okay
        }).should.not.throw();
        (function() {
            new bidi.Paragraph(5); // okay
        }).should.not.throw();
        (function() {
            bidi.Paragraph({
                toString: function() { throw new Error('boo'); }
            });
        }).should.throw();
        (function() {
            new bidi.Paragraph({
                toString: function() { throw new Error('boo'); }
            });
        }).should.throw();
    });
});
