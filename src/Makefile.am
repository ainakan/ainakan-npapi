extra_ldflags =

if OS_LINUX
extra_ldflags += -Wl,--version-script -Wl,$(srcdir)/npainakan.version
else
extra_ldflags += -export-symbols-regex "^(NP_GetMIMEDescription|NP_GetValue|NP_GetEntryPoints|NP_Initialize|NP_Shutdown)$$"
endif

plugindir = $(libdir)/browser/plugins
plugin_LTLIBRARIES = libnpainakan.la

noinst_LTLIBRARIES = \
	libnpainakan-codegen.la \
	libnpainakan-handwritten.la \
	libnpainakan-generated.la

libnpainakan_la_SOURCES = \
	$(NULL)
libnpainakan_la_LDFLAGS = \
	-avoid-version -shared \
	$(NPAINAKAN_LDFLAGS) \
	$(extra_ldflags)
libnpainakan_la_LIBADD = \
	libnpainakan-handwritten.la \
	libnpainakan-generated.la \
	$(NPAINAKAN_LIBS)
libnpainakan_la_LIBTOOLFLAGS = \
	--tag=disable-static

libnpainakan_handwritten_la_SOURCES = \
	npainakan-plugin.h \
	npainakan-plugin.cpp \
	npainakan-object.h \
	npainakan-object-priv.h \
	npainakan-object.cpp \
	npainakan-promise.h \
	npainakan-promise.cpp \
	npainakan-byte-array.h \
	npainakan-byte-array.cpp \
	npainakan-api-glue.c

libnpainakan_generated_la_SOURCES = \
	npainakan-root.c
libnpainakan_generated_la_CFLAGS = \
	-w

libnpainakan_codegen_la_SOURCES = \
	npainakan-api.vala \
	npainakan-root.vala \
	npainakan-codegen.c
libnpainakan_codegen_la_CFLAGS = \
	-w
libnpainakan_codegen_la_LDFLAGS = \
	-export-symbols-regex "^__hack_to_force_build_order__$$" \
	$(NPAINAKAN_LDFLAGS) \
libnpainakan_codegen_la_LIBADD = \
	$(NPAINAKAN_LIBS)
libnpainakan_codegen_la_VALAFLAGS = \
	--library=npainakan \
	--header=npainakan.h \
	--vapidir=$(abs_sourcedir) \
	--pkg=npainakan-object \
	@NPAINAKAN_PACKAGES@ \
	@NPAINAKAN_VALAFLAGS@

AM_CPPFLAGS = \
	-include config.h \
	$(NPAINAKAN_CFLAGS)
