CLEANFILES = \
	ZhConversion.* \
	$(NULL)

ZhConversion.php:
	$(AM_V_GEN) \
	curl https://raw.githubusercontent.com/wikimedia/mediawiki/master/includes/languages/data/ZhConversion.php > $@ || \
		( $(RM) $@; exit 1 )

ZhConversion.py: ZhConversion.php
	$(AM_V_GEN) \
	php $(srcdir)/php_2_py.php > $@ || \
		( $(RM) $@; exit 1 )
