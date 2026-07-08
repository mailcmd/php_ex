# Makefile
OTP_ROOT ?= $(shell erl -noshell -eval 'io:format("~s~n", [code:root_dir()]), halt().')
PRIV_DIR ?= priv
SRC_DIR ?= c_src
FILE_NAME = php_node
INCLUDES ?= $(shell /usr/bin/php-config --includes)
LIBS ?= -lphp7 $(shell /usr/bin/php-config --libs)
CFLAGS ?= -fPIC -Wno-incompatible-pointer-types

LIB_CHECK := $(shell ldconfig -p | grep libphp7)

ifeq ($(LIB_CHECK),)
    $(error Error: libphp7 is not installed or not in system library paths.)
endif

.PHONY: all clean

all: real_all
	@:

real_all: $(PRIV_DIR)/$(FILE_NAME)

$(PRIV_DIR)/$(FILE_NAME): $(SRC_DIR)/$(FILE_NAME).c $(SRC_DIR)/php.h 
	@mkdir -p $(PRIV_DIR) 2>/dev/null 
	@gcc $(SRC_DIR)/$(FILE_NAME).c -o $(PRIV_DIR)/$(FILE_NAME) \
        $(INCLUDES) \
        $(LIBS) \
        $(CFLAGS) \
        -I$(OTP_ROOT)/usr/include \
        -L$(OTP_ROOT)/usr/lib \
        -lei_st \
        -Wno-incompatible-pointer-types        
	@cp "$(PRIV_DIR)/$(FILE_NAME)" "$(PWD)/priv/" 2>/dev/null || echo -n 

clean:
	@rm -f $(PRIV_DIR)/$(FILE_NAME)
