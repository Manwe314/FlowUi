TARGET := flowui_font_baker
BUILD_DIR := .flowui_font_baker_build
OUTPUT_BIN := ./$(TARGET)
CMAKE := cmake

.PHONY: all clean fclean re

all: $(OUTPUT_BIN)

$(BUILD_DIR)/CMakeCache.txt:
	$(CMAKE) -S . -B $(BUILD_DIR) \
		-DFLOWUI_BUILD_FONT_BAKER=ON \
		-DFLOWUI_ENABLE_RUNTIME_FONT_BAKING=OFF \
		-DFLOWUI_INSTALL=OFF

$(BUILD_DIR)/$(TARGET): $(BUILD_DIR)/CMakeCache.txt
	$(CMAKE) --build $(BUILD_DIR) --target $(TARGET) --parallel

$(OUTPUT_BIN): $(BUILD_DIR)/$(TARGET)
	$(CMAKE) -E copy_if_different $(BUILD_DIR)/$(TARGET) $(OUTPUT_BIN)
	chmod +x $(OUTPUT_BIN)

clean:
	rm -rf $(BUILD_DIR)

fclean: clean
	rm -f $(OUTPUT_BIN)

re: fclean all
