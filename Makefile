FONT_BAKER_TARGET := flowui_font_baker
FONT_BAKER_BUILD_DIR := .flowui_font_baker_build
FONT_BAKER_BIN := ./$(FONT_BAKER_TARGET)

DEV_CHANGE_UPDATER_TARGET := flowui_devChange_updater
DEV_CHANGE_UPDATER_BIN := ./$(DEV_CHANGE_UPDATER_TARGET)
DEV_CHANGE_UPDATER_SRC := tools/flowui_devChange_updater/flowui_devChange_updater.cpp

CMAKE := cmake
CXX ?= c++
CXXFLAGS ?= -O2 -Wall -Wextra
CXXSTD ?= -std=c++20

.PHONY: all font_baker devChange_updater clean fclean re

all: font_baker devChange_updater

font_baker: $(FONT_BAKER_BIN)

devChange_updater: $(DEV_CHANGE_UPDATER_BIN)

$(FONT_BAKER_BUILD_DIR)/CMakeCache.txt:
	$(CMAKE) -S . -B $(FONT_BAKER_BUILD_DIR) \
		-DFLOWUI_BUILD_FONT_BAKER=ON \
		-DFLOWUI_ENABLE_RUNTIME_FONT_BAKING=OFF \
		-DFLOWUI_INSTALL=OFF

$(FONT_BAKER_BUILD_DIR)/$(FONT_BAKER_TARGET): $(FONT_BAKER_BUILD_DIR)/CMakeCache.txt
	$(CMAKE) --build $(FONT_BAKER_BUILD_DIR) --target $(FONT_BAKER_TARGET) --parallel

$(FONT_BAKER_BIN): $(FONT_BAKER_BUILD_DIR)/$(FONT_BAKER_TARGET)
	$(CMAKE) -E copy_if_different $(FONT_BAKER_BUILD_DIR)/$(FONT_BAKER_TARGET) $(FONT_BAKER_BIN)
	chmod +x $(FONT_BAKER_BIN)

$(DEV_CHANGE_UPDATER_BIN): $(DEV_CHANGE_UPDATER_SRC)
	$(CXX) $(CXXSTD) $(CXXFLAGS) $(CPPFLAGS) $(DEV_CHANGE_UPDATER_SRC) -o $(DEV_CHANGE_UPDATER_BIN)
	chmod +x $(DEV_CHANGE_UPDATER_BIN)

clean:
	rm -rf $(FONT_BAKER_BUILD_DIR)

fclean: clean
	rm -f $(FONT_BAKER_BIN) $(DEV_CHANGE_UPDATER_BIN)

re: fclean all
