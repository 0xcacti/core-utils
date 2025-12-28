SRC_DIR=src
BIN_DIR=bin
OBJ_DIR=obj
VENDOR_DIR=vendor

CFLAGS=-Iinclude -I$(VENDOR_DIR)/raylib/include -Wall -Wextra -O2
LDFLAGS=-L$(VENDOR_DIR)/raylib/lib -lraylib 

TARGET=$(BIN_DIR)/sorting-algos

SRC=$(SRC_DIR)
