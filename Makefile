CC = gcc
TARGET = now
SRC = now.c
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $@ $^

clean:
	rm -rf $(TARGET)

install: $(TARGET)
	@echo "Installing $(TARGET) to $(BINDIR)..."
	sudo cp $(TARGET) $(BINDIR)
	sudo chmod 755 $(BINDIR)/$(TARGET)

uninstall:
	@echo "Removing $(TARGET) from $(BINDIR)..."
	sudo rm -rf $(BINDIR)/$(TARGET)
