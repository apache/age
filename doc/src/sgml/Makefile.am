# suggested by libtoolize
ACLOCAL_AMFLAGS = -I m4
DEFS = @DEFS@ \
	-DDEFAULT_CONFIGDIR=\"$(sysconfdir)\"

all: html man1 man8

GENERATED_SGML = bookindex.sgml version.sgml

ALLSGML := $(wildcard $(srcdir)/*.sgml $(srcdir)/ref/*.sgml) $(GENERATED_SGML)

# Sometimes we don't want this one.
ALMOSTALLSGML := $(filter-out %bookindex.sgml,$(ALLSGML))

SPFLAGS = -wall -wno-unused-param -wno-empty -wfully-tagged

OUTDIR = html

#if DOCBOOKSTYLE
#CATALOG = -c $(DOCBOOKSTYLE)/catalog
#endif

#override XSLTPROCFLAGS += --stringparam pgpool.version '$(VERSION)'
override XSLTPROCFLAGS += --stringparam pg.version '$(VERSION)'

#jade -G -t sgml -i html  -d stylesheet.dsl#html -o abas  pgpool-en.sgm
JADE.html.call = $(JADE) $(JADEFLAGS) $(SPFLAGS) $(SGMLINCLUDE) $(CATALOG) -d stylesheet.dsl -t sgml -i output-html
if STYLE_ENV_SET
JADE.html.call += -V website-stylesheet
XSLTPROC_HTML_FLAGS += --param website.stylesheet 1
endif

JADE.tex.call = $(JADE) $(JADEFLAGS) $(SGMLINCLUDE) $(CATALOG) -d stylesheet.dsl -t tex -V tex-backend -i output-print -i include-index
JADE.rtf.call = $(JADE) $(JADEFLAGS) $(SGMLINCLUDE) $(CATALOG) -d stylesheet.dsl -t rtf -V rtf-backend -i output-print  -i include-index

JADE_html = $(JADE) $(SPFLAGS) -d stylesheet.dsl\#html -t sgml -i html -V website-stylesheet


EXTRA_DIST = $(ALLSGML) html man1 man8 stylesheet.dsl stylesheet.css stylesheet-man.xsl stylesheet-common.xsl

##
## Man pages
##

man1 man8 distprep-man: man-stamp

man-stamp: stylesheet-man.xsl pgpool.xml
	$(XMLLINT) --noout --valid pgpool.xml
	$(XSLTPROC) $(XSLTPROCFLAGS) $(XSLTPROC_MAN_FLAGS) $^
	touch $@

# The draft target creates HTML output in draft mode, without index (for faster build).
draft: pgpool.sgml $(ALMOSTALLSGML) stylesheet.dsl
	$(MKDIR_P) html
	$(JADE.html.call) -V draft-mode $<
	cp $(srcdir)/stylesheet.css html/

html: html-stamp

html-stamp: pgpool.sgml $(ALLSGML) stylesheet.dsl
	$(MKDIR_P) html
	$(JADE.html.call) -i include-index $<
	cp $(srcdir)/stylesheet.css html/
	cp ../figures/*.gif html

# single-page HTML
pgpool.html: pgpool.sgml $(ALLSGML) stylesheet.dsl
	$(JADE.html.call) -V nochunks -V rootchunk -V '(define %root-filename% #f)' -V '(define use-output-dir #f)' -i include-index $<

# single-page text
pgpool.txt: pgpool.html
	$(LYNX) -force_html -dump -nolist $< > $@

HTML.index: pgpool.sgml $(ALMOSTALLSGML) stylesheet.dsl
	 $(MKDIR_P) html
	 $(JADE.html.call) -V html-index $<

bookindex.sgml: HTML.index
	LC_ALL=C $(PERL) $(COLLATEINDEX) -f -g -i 'bookindex' -o $@ $<

# single-page text
#pgpool-en.txt: pgpool-en.html
#	$(LYNX) -force_html -dump -nolist $< > $@

# single-page HTML
#pgpool-en.html: pgpool-en.sgml $(ALLSGML) stylesheet.dsl
#	$(JADE.html.call) -V nochunks -V rootchunk -i include-index $< >$@

%.rtf: %.sgml $(ALLSGML)
	$(JADE.rtf.call) $<

%-A4.tex-ps: %.sgml $(ALLSGML)
	    $(JADE.tex.call) -V texdvi-output -V '%paper-type%'=A4 -o $@ $<

%-US.tex-ps: %.sgml $(ALLSGML)
	    $(JADE.tex.call) -V texdvi-output -V '%paper-type%'=USletter -o $@ $<

%-A4.tex-pdf: %.sgml $(ALLSGML)
	    $(JADE.tex.call) -V texpdf-output -V '%paper-type%'=A4 -o $@ $<

%-US.tex-pdf: %.sgml $(ALLSGML)
	    $(JADE.tex.call) -V texpdf-output -V '%paper-type%'=USletter -o $@ $<


%.pdf: %.tex-pdf
	@rm -f $*.aux $*.log $*.out
# multiple runs are necessary to create proper intra-document links
	pdfjadetex $<
	pdfjadetex $<
	pdfjadetex $<

#pgpool-en.pdf: pgpool-en-A4.tex-pdf pgpool-en-US.tex-pdf

# This generates an XML version of the flow-object tree.  It's useful
# # for debugging DSSSL code, and possibly to interface to some other
# # tools that can make use of this.
%.fot: %.sgml $(ALLSGML)
	$(JADE) $(JADEFLAGS) $(SGMLINCLUDE) $(CATALOG) -d stylesheet.dsl -t fot -i output-print -i include-index -o $@ $<

%.ps: %.dvi
	dvips -o $@ $<

##
## XSLT processing
##

pgpool-en.xml: $(ALLSGML)
	$(OSX) -D. -x lower -i include-xslt-index $< >pgpool-en.xmltmp
	$(PERL) -p -e 's/\[(aacute|acirc|aelig|agrave|amp|aring|atilde|auml|bull|copy|eacute|egrave|gt|iacute|lt|mdash|nbsp|ntilde|oacute|ocirc|oslash|ouml|pi|quot|scaron|uuml) *\]/\&\1;/gi;' \
		-e '$$_ .= qq{<!DOCTYPE book PUBLIC "-//OASIS//DTD DocBook XML V4.2//EN" "http://www.oasis-open.org/docbook/xml/4.2/docbookx.dtd">\n} if $$. == 1;' \
		<pgpool-en.xmltmp > $@
	rm pgpool-en.xmltmp


pgpool.xml: $(srcdir)/pgpool.sgml $(ALMOSTALLSGML)
	$(OSX) -D. -x lower -i include-xslt-index $< >pgpool.xmltmp
	$(PERL) -p -e 's/\[(aacute|acirc|aelig|agrave|amp|aring|atilde|auml|bull|copy|eacute|egrave|gt|iacute|lt|mdash|nbsp|ntilde|oacute|ocirc|oslash|ouml|pi|quot|scaron|uuml) *\]/\&\1;/gi;' \
		-e '$$_ .= qq{<!DOCTYPE book PUBLIC "-//OASIS//DTD DocBook XML V4.2//EN" "http://www.oasis-open.org/docbook/xml/4.2/docbookx.dtd">\n} if $$. == 1;' \
		<pgpool.xmltmp > $@
	rm pgpool.xmltmp

.PHONY: xslthtml-stamp

xslthtml: xslthtml-stamp

xslthtml: xslthtml-stamp

xslthtml-stamp: stylesheet.xsl pgpool.xml
	$(XMLLINT) --noout --valid pgpool.xml
	$(XSLTPROC) $(XSLTPROCFLAGS) $(XSLTPROC_HTML_FLAGS) $^
	cp $(srcdir)/stylesheet.css html/

htmlhelp: stylesheet-hh.xsl pgpool.xml
	$(XMLLINT) --noout --valid pgpool.xml
	$(XSLTPROC) $(XSLTPROCFLAGS) $^

%-A4.fo.tmp: stylesheet-fo.xsl %.xml
	$(XMLLINT) --noout --valid $*.xml
	$(XSLTPROC) $(XSLTPROCFLAGS) --stringparam paper.type A4 -o $@ $^

%-US.fo.tmp: stylesheet-fo.xsl %.xml
	$(XMLLINT) --noout --valid $*.xml
	$(XSLTPROC) $(XSLTPROCFLAGS) --stringparam paper.type USletter -o $@ $^

FOP = fop

#xslthtml-stamp: stylesheet.xsl pgpool-en.xml
#	$(XSLTPROC) $(XSLTPROCFLAGS) $(XSLTPROC_HTML_FLAGS) $^
#	cp $(srcdir)/stylesheet.css html/

# reformat FO output so that locations of errors are easier to find
%.fo: %.fo.tmp
	$(XMLLINT) --format --output $@ $^

epub: pgpool.epub
	pgpool.epub: pgpool.xml
	$(XMLLINT) --noout --valid $<
	$(DBTOEPUB) $<

version.sgml: $(top_srcdir)/configure
	{ \
		echo "<!ENTITY version \"$(PACKAGE_VERSION)\">"; \
	} > $@

# tabs are harmless, but it is best to avoid them in SGML files
check-tabs:
	@( ! grep ' ' $(wildcard $(srcdir)/*.sgml $(srcdir)/ref/*.sgml $(srcdir)/*.dsl $(srcdir)/*.xsl) ) || (echo "Tabs appear in SGML/XML files" 1>&2;  exit 1)

CLEANFILES = *.html *.tex-pdf pgpool-en.rtf pgpool-en.txt html/* HTML.index bookindex.sgml man-stamp man1/* man3/* pgpool.xml
