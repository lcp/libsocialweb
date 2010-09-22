ACLOCAL_AMFLAGS = -I m4

SUBDIRS = interfaces libsocialweb libsocialweb-keyfob libsocialweb-keystore libsocialweb-client src services examples tests tools po docs

pkgconfig_DATA = libsocialweb-client.pc libsocialweb-module.pc libsocialweb-keyfob.pc libsocialweb-keystore.pc
pkgconfigdir   = $(libdir)/pkgconfig

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-gtk-doc \
	--enable-all-services

EXTRA_DIST = intltool-extract.in intltool-merge.in intltool-update.in
DISTCLEANFILES = intltool-extract intltool-merge intltool-update

if GCOV_ENABLED
cov-reset:
	@rm -fr coverage
	@find . -name "*.gcda" -exec rm {} \;
	@lcov --directory . --zerocounters

cov-report:
	@mkdir -p coverage
	@lcov --compat-libtool --directory . --capture --output-file coverage/app.info
	@genhtml -o coverage/ coverage/app.info

cov:
	@make cov-report

clean-local:
	@make cov-reset

check:
	@make cov
endif
