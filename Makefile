CC := cc
CFLAGS := -O2 -Wall -Wextra -Wpedantic -Werror -Werror=implicit-function-declaration -Wstrict-prototypes -Wmissing-prototypes
LDFLAGS :=
LDLIBS :=

BINDIR := bin
TOOLDIRS := cat head pwd echo yes touch tee rm tail du mv cp

.PHONY: all clean
all: $(addprefix $(BINDIR)/,$(TOOLDIRS))

$(BINDIR):
	mkdir -p $@

$(BINDIR)/%: %/*.c | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -rf $(BINDIR)
