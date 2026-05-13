CFLAGS = -fPIC -shared -nostdlib -nodefaultlibs -Iinclude
# Errors
CFLAGS += -Wall -Wextra -Wcomment
CC = gcc-14

# Logger flags
ERROR_LOGS ?= 0
WARN_LOGS ?= 0
INFO_LOGS ?= 0
DEBUG_LOGS ?= 0
IO_LOGS ?= 0
MEM_LOGS ?= 0
LOGGING_LOGS ?= 0
SPECIAL_LOGS ?= 0

# Build mode flags
NO_HEAP ?= 0
NIFAT32_RO ?= 0
NO_DEFAULT_MM_MANAGER ?= 0
ALLOC_BUFFER_SIZE ?=

########
# Logger flagså
ifeq ($(ERROR_LOGS), 1)
    CFLAGS += -DERROR_LOGS
endif

ifeq ($(WARN_LOGS), 1)
    CFLAGS += -DWARNING_LOGS
endif

ifeq ($(INFO_LOGS), 1)
    CFLAGS += -DINFO_LOGS
endif

ifeq ($(DEBUG_LOGS), 1)
    CFLAGS += -DDEBUG_LOGS
endif

ifeq ($(IO_LOGS), 1)
    CFLAGS += -DIO_OPERATION_LOGS
endif

ifeq ($(MEM_LOGS), 1)
    CFLAGS += -DMEM_OPERATION_LOGS
endif

ifeq ($(LOGGING_LOGS), 1)
    CFLAGS += -DLOGGING_LOGS
endif

ifeq ($(SPECIAL_LOGS), 1)
    CFLAGS += -DSPECIAL_LOGS
endif

ifeq ($(NIFAT32_RO), 1)
    CFLAGS += -DNIFAT32_RO
endif

ifeq ($(NO_DEFAULT_MM_MANAGER), 1)
    CFLAGS += -DNO_DEFAULT_MM_MANAGER
endif

ifeq ($(NO_HEAP), 1)
    CFLAGS += -DNO_HEAP
endif

ifneq ($(ALLOC_BUFFER_SIZE),)
    CFLAGS += -DALLOC_BUFFER_SIZE=$(ALLOC_BUFFER_SIZE)
endif

OUTPUT = builds/nifat32.so
SOURCES = nifat32.c src/*.c std/*.c

all: force_build $(OUTPUT)

force_build:
	@if [ -e $(OUTPUT) ]; then rm -f $(OUTPUT); fi

$(OUTPUT): $(SOURCES)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SOURCES) -g

clean:
	rm -f $(OUTPUT)

.PHONY: all clean force_build
