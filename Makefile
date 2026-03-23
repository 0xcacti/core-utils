CC := cc
CFLAGS := -O2 -Wall -Wextra -Wpedantic -Werror -Werror=implicit-function-declaration -Wstrict-prototypes -Wmissing-prototypes
LDFLAGS :=
LDLIBS :=

BINDIR := bin
TOOLDIRS := cat head pwd echo yes touch tee rm tail du mv ln chmod

.PHONY: all clean
all: $(addprefix $(BINDIR)/,$(TOOLDIRS))

$(BINDIR):
	mkdir -p $@

$(BINDIR)/%: %/*.c | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

compile_commands.json: clean
	bear -- make

clean:
	rm -rf $(BINDIR)
