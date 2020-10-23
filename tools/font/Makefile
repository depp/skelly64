CFLAGS := -O1 -g -Wall -Wextra -Wpointer-arith -Wwrite-strings -Wmissing-prototypes -Wdouble-promotion -Werror=implicit-function-declaration -Winit-self -Wstrict-prototypes

WERROR := 1
ifeq ($(WERROR),1)
CFLAGS += -Werror
endif

dep_cflags := $(shell pkg-config --cflags freetype2)
dep_libs := $(shell pkg-config --libs freetype2)

raster: raster.c
	$(CC) -fsanitize=address $(CFLAGS) $(dep_cflags) -o $@ $^ $(dep_libs)
clean:
	rm -f raster
.PHONY: clean
