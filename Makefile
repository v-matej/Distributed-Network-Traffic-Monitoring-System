BUILD_DIR=build
SOURCE_DIR=.

build:
	cmake -S $(SOURCE_DIR) -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean buildpa