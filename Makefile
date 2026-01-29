PACKAGE = nullinitrd
VERSION = 1.0.0

-include .config

CXX ?= g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -DVERSION=\"$(VERSION)\"
LDFLAGS =

ifeq ($(CONFIG_STATIC),y)
LDFLAGS += -static
endif

ifeq ($(CONFIG_DEBUG),y)
CXXFLAGS += -g -O0 -DDEBUG
endif

ifeq ($(CONFIG_LTO),y)
CXXFLAGS += -flto
LDFLAGS += -flto
endif

SRCDIR = src
OBJDIR = obj
BINDIR = bin

GEN_SOURCES = $(SRCDIR)/main.cpp $(SRCDIR)/config.cpp $(SRCDIR)/generator.cpp $(SRCDIR)/hooks.cpp $(SRCDIR)/utils.cpp
GEN_OBJECTS = $(GEN_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
GEN_TARGET = $(BINDIR)/$(PACKAGE)

INIT_SOURCE = $(SRCDIR)/init.cpp
INIT_OBJECT = $(OBJDIR)/init.o
INIT_TARGET = $(BINDIR)/init

PREFIX ?= /usr
BINPREFIX = $(PREFIX)/bin
CONFDIR = /etc/$(PACKAGE)
DATADIR = $(PREFIX)/share/$(PACKAGE)
HOOKSDIR = $(DATADIR)/hooks

.PHONY: all clean install uninstall menuconfig defconfig help

all: $(GEN_TARGET) $(INIT_TARGET)

$(GEN_TARGET): $(GEN_OBJECTS) | $(BINDIR)
	$(CXX) $(GEN_OBJECTS) -o $@ $(LDFLAGS)
ifneq ($(CONFIG_DEBUG),y)
	strip $@
endif

$(INIT_TARGET): $(INIT_OBJECT) | $(BINDIR)
	$(CXX) $(INIT_OBJECT) -o $@ -static
	strip $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(OBJDIR) $(BINDIR)

install: all
	install -D -m 755 $(GEN_TARGET) $(DESTDIR)$(BINPREFIX)/$(PACKAGE)
	install -D -m 755 $(INIT_TARGET) $(DESTDIR)$(DATADIR)/init
	install -D -m 644 config.default $(DESTDIR)$(CONFDIR)/config
	install -d $(DESTDIR)$(HOOKSDIR)
	@if [ -d hooks ] && [ "$$(ls -A hooks 2>/dev/null)" ]; then install -m 755 hooks/* $(DESTDIR)$(HOOKSDIR)/; fi

uninstall:
	rm -f $(DESTDIR)$(BINPREFIX)/$(PACKAGE)
	rm -rf $(DESTDIR)$(CONFDIR)
	rm -rf $(DESTDIR)$(DATADIR)

menuconfig:
	@if command -v dialog >/dev/null 2>&1; then \
		./scripts/menuconfig.sh; \
	else \
		echo "Error: dialog not found"; \
		exit 1; \
	fi

defconfig:
	@cp config.defconfig .config
	@echo "Default configuration written to .config"

help:
	@echo "nullinitrd $(VERSION)"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build generator and init binary"
	@echo "  clean       - Remove build artifacts"
	@echo "  install     - Install to system"
	@echo "  uninstall   - Remove from system"
	@echo "  menuconfig  - Interactive configuration"
	@echo "  defconfig   - Load default configuration"
	@echo ""
	@echo "Build options (in .config):"
	@echo "  CONFIG_STATIC=y  - Static linking"
	@echo "  CONFIG_DEBUG=y   - Debug build"
	@echo "  CONFIG_LTO=y     - Link-time optimization"
