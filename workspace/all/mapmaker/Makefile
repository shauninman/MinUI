# Define the target name
TARGET = fuzzy_match_gamelist

# Define the Python script
PYTHON_SCRIPT = fuzzy_match_gamelist.py

# Define the output directory of PyInstaller
DIST_DIR = dist

# Define the output executable
OUTPUT_EXECUTABLE = $(DIST_DIR)/$(TARGET)

# Define the final location to copy the executable
FINAL_LOCATION = .

# Phony targets
.PHONY: all clean

# Default target
all: $(TARGET)

# Build target
$(TARGET):
	pyinstaller --onefile $(PYTHON_SCRIPT)
	cp $(OUTPUT_EXECUTABLE) $(FINAL_LOCATION)

# Clean target
clean:
	rm -rf build $(DIST_DIR) $(TARGET).spec $(FINAL_LOCATION)/$(TARGET)
