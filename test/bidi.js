// Check results of bidi algorithm!
require('should');

describe('Bidi algorithm', function() {
    var ubidi = require('../');
    it('should handle unidirectional LTR text', function() {
        var p = ubidi.Paragraph('The quick brown fox jumped');
        p.should.have.property('getParaLevel');
        p.getParaLevel().should.equal(0);
        p.getLevelAt(0).should.equal(0);
        p.getDirection().should.equal('ltr');
        p.getLength().should.equal(26);
        p.getProcessedLength().should.equal(26);
        p.getResultLength().should.equal(26);
        p.countParagraphs().should.equal(1);
        p.getParagraphByIndex(0).should.eql({
            index: 0,
            start: 0,
            limit: 26,
            level: 0,
            dir: 'ltr'
        });
        p.getParagraph(1).should.eql({
            index: 0,
            start: 0,
            limit: 26,
            level: 0,
            dir: 'ltr'
        });
        p.countRuns().should.equal(1);
        var run = p.getVisualRun(0);
        run.should.have.property('dir');
        run.should.have.property('logicalStart');
        run.should.have.property('length');
        run.dir.should.equal('ltr');
        run.logicalStart.should.equal(0);
        run.length.should.equal(26);
        var i;
        for (i = 0; i < p.getLength(); i++) {
            p.getLogicalIndex(p.getVisualIndex(i)).should.equal(i);
            p.getVisualIndex(p.getLogicalIndex(i)).should.equal(i);
        }
        p.getVisualIndex(0).should.equal(0);
        p.getLogicalIndex(0).should.equal(0);
    });
    it('should handle unidirectional RTL text', function() {
        var p = ubidi.Paragraph('עִבְרִית', {paraLevel: ubidi.DEFAULT_RTL});
        p.should.have.property('getParaLevel');
        p.getParaLevel().should.equal(1);
        p.getLevelAt(0).should.equal(1);
        p.getDirection().should.equal('rtl');
        p.getLength().should.equal(8);
        p.getProcessedLength().should.equal(8);
        p.getResultLength().should.equal(8);
        p.countParagraphs().should.equal(1);
        p.getParagraphByIndex(0).should.eql({
            index: 0,
            start: 0,
            limit: 8,
            level: 1,
            dir: 'rtl'
        });
        p.getParagraph(1).should.eql({
            index: 0,
            start: 0,
            limit: 8,
            level: 1,
            dir: 'rtl'
        });
        p.countRuns().should.equal(1);
        var run = p.getVisualRun(0);
        run.should.have.property('dir');
        run.should.have.property('logicalStart');
        run.should.have.property('length');
        run.dir.should.equal('rtl');
        run.logicalStart.should.equal(0);
        run.length.should.equal(8);
        var i;
        for (i = 0; i < p.getLength(); i++) {
            p.getLogicalIndex(p.getVisualIndex(i)).should.equal(i);
            p.getVisualIndex(p.getLogicalIndex(i)).should.equal(i);
        }
        p.getVisualIndex(0).should.equal(7);
        p.getLogicalIndex(0).should.equal(7);
    });
    it('should handle neutral text in LTR context', function() {
        var p = ubidi.Paragraph(' ');
        p.getParaLevel().should.equal(0);
        p.getLevelAt(0).should.equal(0);
        p.getDirection().should.equal('ltr');
        p.countRuns().should.equal(1);
        p.getVisualRun(0).should.eql({
            dir: 'ltr',
            logicalStart: 0,
            length: 1
        });
    });
    it('should handle neutral text in RTL context', function() {
        var p = ubidi.Paragraph(' ', {paraLevel: ubidi.DEFAULT_RTL});
        p.getParaLevel().should.equal(1);
        p.getLevelAt(0).should.equal(1);
        p.getDirection().should.equal('rtl');
        p.countRuns().should.equal(1);
        p.getVisualRun(0).should.eql({
            dir: 'rtl',
            logicalStart: 0,
            length: 1
        });
    });
    it('should handle mixed LTR + RTL text in LTR context', function() {
        var e = 'English';
        var h = 'עִבְרִית';
        var p = ubidi.Paragraph('(' + e + ' ' + h + ')'); // ltr context
        p.should.have.property('getParaLevel');
        p.getParaLevel().should.equal(0);
        p.getDirection().should.equal('mixed');
        p.countRuns().should.equal(3);
        var run0 = p.getVisualRun(0);
        run0.should.eql({
            dir: 'ltr',
            logicalStart: 0,
            length: 9
        });
        var run1 = p.getVisualRun(1);
        run1.should.eql({
            dir: 'rtl',
            logicalStart: 9,
            length: 8
        });
        var run2 = p.getVisualRun(2);
        run2.should.eql({
            dir: 'ltr',
            logicalStart: 17,
            length: 1
        });
        var run3 = p.getLogicalRun(1);
        run3.should.eql({
            logicalLimit: 9,
            level: 0,
            dir: 'ltr'
        });
        var run4 = p.getLogicalRun(e.length + h.length);
        run4.should.eql({
            logicalLimit: 17,
            level: 1,
            dir: 'rtl'
        });
    });
    it('should handle mixed LTR + RTL text in RTL context', function() {
        var e = 'English';
        var h = 'עִבְרִית';
        var p = ubidi.Paragraph('(' + e + ' ' + h + ')',
                               {paraLevel: ubidi.DEFAULT_RTL}); // rtl context
        p.should.have.property('getParaLevel');
        p.getParaLevel().should.equal(0);
        p.getDirection().should.equal('mixed');
        p.countRuns().should.equal(3);
        var run0 = p.getVisualRun(0);
        run0.should.eql({
            dir: 'ltr',
            logicalStart: 0,
            length: 9
        });
        var run1 = p.getVisualRun(1);
        run1.should.eql({
            dir: 'rtl',
            logicalStart: 9,
            length: 8
        });
        var run2 = p.getVisualRun(2);
        run2.should.eql({
            dir: 'ltr',
            logicalStart: 17,
            length: 1
        });
        var run3 = p.getLogicalRun(1);
        run3.should.eql({
            logicalLimit: 9,
            level: 0,
            dir: 'ltr'
        });
        var run4 = p.getLogicalRun(e.length + h.length);
        run4.should.eql({
            logicalLimit: 17,
            level: 1,
            dir: 'rtl'
        });
    });
    it('should handle separate line contexts', function() {
        var e = 'English';
        var h = 'עִבְרִית';
        var p = ubidi.Paragraph('(' + e + ' ' + h + ')');
        p.getDirection().should.equal('mixed');
        var l1 = p.setLine(0, e.length+2);
        var l2 = p.setLine(e.length+2, p.getLength());
        l1.getLength().should.equal(9);
        l2.getLength().should.equal(9);
        l1.countRuns().should.equal(1);
        l1.getVisualRun(0).should.eql({
            dir: 'ltr',
            logicalStart: 0,
            length: 9
        });
        l2.countRuns().should.equal(2);
        l2.getVisualRun(0).should.eql({
            dir: 'rtl',
            logicalStart: 0,
            length: 8
        });
        l2.getVisualRun(1).should.eql({
            dir: 'ltr',
            logicalStart: 8,
            length: 1
        });
    });
    it('should handle prologue contexts', function() {
        var h = 'עִבְרִית';
        var p1 = new ubidi.Paragraph('.-=', {
            paraLevel: ubidi.DEFAULT_LTR
        });
        p1.getDirection().should.equal('ltr');
        var p2 = new ubidi.Paragraph('.-=', {
            paraLevel: ubidi.DEFAULT_LTR,
            prologue: h
        });
        p2.getDirection().should.equal('rtl');
    });
    it('should handle epilogue contexts', function() {
        var h    = 'עִבְרִית';
        var hrev = 'תיִרְבִע';
        var p1 = new ubidi.Paragraph('.-='+h+'-+*', {
            paraLevel: ubidi.LTR
        });
        p1.getDirection().should.equal('mixed');
        p1.writeReordered(ubidi.Reordered.DO_MIRRORING).
            should.equal('.-='+hrev+'-+*');
        var p2 = new ubidi.Paragraph('.-='+h+'-+*', {
            paraLevel: ubidi.LTR,
            epilogue: ' \u0634'
        });
        p2.getDirection().should.equal('mixed');
        p2.writeReordered(ubidi.Reordered.DO_MIRRORING).
            should.equal('.-=*+-'+hrev);
    });
    it('should handle multiple paragraphs', function() {
        var e = 'English';
        var h = 'עִבְרִית';
        var p = new ubidi.Paragraph(e + '\n' + h);
        p.countParagraphs().should.equal(2);
        var para1 = p.getParagraphByIndex(0);
        var para2 = p.getParagraphByIndex(1);
        para1.should.eql({
            index: 0,
            start: 0,
            limit: 8,
            level: 0,
            dir: 'ltr'
        });
        para2.should.eql({
            index: 1,
            start: 8,
            limit: 16,
            level: 1,
            dir: 'rtl'
        });
        p.getParagraph(3).should.eql(para1);
        p.getParagraph(e.length + 4).should.eql(para2);
    });
});
