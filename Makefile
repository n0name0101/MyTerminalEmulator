# Compiler dan flag
CC     = gcc
CFLAGS = -Wall -I/usr/include/freetype2 -Iinclude/ $(shell pkg-config --cflags gtk+-3.0)
LDFLAGS = -lX11 -lXft -lfreetype $(shell pkg-config --libs gtk+-3.0) -lutil

# Folder untuk object files
OBJDIR = obj

# Daftar source dan target
SRCS   = main.c GTKConsole.c MyPTY.c
OBJS   = $(OBJDIR)/main.o $(OBJDIR)/GTKConsole.o $(OBJDIR)/MyPTY.o
TARGET = MyTerminalEmulator

# Target utama
all: $(TARGET)

# Rule untuk membuat executable
$(TARGET): $(OBJS)
	@echo "Linking $@ ..."
	@$(CC) -o $@ $^ $(LDFLAGS)

# Rule untuk membangun object file di folder obj
$(OBJDIR)/%.o: %.c
	@mkdir -p $(OBJDIR)
	@echo "Compiling $< ..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Membersihkan file objek dan executable
clean:
	@rm -f $(OBJS) $(TARGET)
	@echo "Clean complete."
