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
    it('should not crash if the `this` pointer is bogus', function() {
        (function() {
            var p = new bidi.Paragraph('xyz');
            var o = { f: p.countRuns };
            o.f();
        }).should.throw();
    });
    it('should allow subtyping', function() {
        (function() {
            var p = new bidi.Paragraph('xyz');
            var pp = Object.create(p);
            pp.countRuns();
        }).should.not.throw();
    });
});
