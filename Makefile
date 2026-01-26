CC := cc
CFLAGS := -O2 -Wall -Wextra -Wpedantic
LDFLAGS :=
LDLIBS :=

BINDIR := bin
TOOLDIRS := cat head pwd echo touch yes

.PHONY: all clean
all: $(addprefix $(BINDIR)/,$(TOOLDIRS))

$(BINDIR):
	mkdir -p $@

$(BINDIR)/%: %/*.c | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -rf $(BINDIR)
