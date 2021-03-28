libhhdir = $(libdir)/hildon-desktop
dathhdir = $(datadir)/applications/hildon-home
libhh_LTLIBRARIES = desktop-cmd-exec.la
dathh_DATA = desktop-cmd-exec.desktop

desktop_cmd_exec_la_SOURCES = desktop-cmd-exec.c
desktop_cmd_exec_la_CFLAGS = $(GTK_CFLAGS) $(DESKTOP_CFLAGS) $(CONIC_CFLAGS)
desktop_cmd_exec_la_LIBADD = $(GTK_LIBS) $(DESKTOP_LIBS) $(CONIC_LIBS)
desktop_cmd_exec_la_LDFLAGS = -module -avoid-version

icondir = $(datadir)/icons/hicolor

install-data-local:
	i=desktop-cmd-exec; \
	$(mkinstalldirs) $(DESTDIR)$(icondir)/128x128/hildon; \
	$(INSTALL_DATA) $(srcdir)/$$i.png $(DESTDIR)$(icondir)/128x128/hildon/$$i.png;

uninstall-local:
	i=desktop-cmd-exec; \
	rm $(DESTDIR)$(icondir)/128x128/hildon/$$i.png;

EXTRA_DIST =					\
        autogen.sh				\
        make-package				\
        debian/changelog			\
        debian/compat				\
        debian/copyright			\
        debian/control				\
        debian/rules				\
	debian/postinst				\
	debian/install				\
        desktop-cmd-exec.desktop

install-data-hook:
	@(cd $(DESTDIR)$(libhhdir) && $(RM) $(libhh_LTLIBRARIES))

deb:
	./make-package $(PACKAGE) $(VERSION)

ACLOCAL_AMFLAGS = -I m4
