IDF_PY := idf.py
GET_IDF := . $(HOME)/esp/esp-idf/export.sh

.PHONY: all build flash monitor clean menuconfig defconfig set-target size get-idf setup
all: build

setup:
	@echo "Setting up ESP-IDF environment..."
	@echo "Please ensure you have ESP-IDF installed and configured."
	$(GET_IDF)

build:
	$(IDF_PY) build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

flash:
	$(IDF_PY) flash

monitor:
	$(IDF_PY) monitor

menuconfig:
	$(IDF_PY) menuconfig

defconfig:
	$(IDF_PY) defconfig

clean:
	$(IDF_PY) fullclean

set-target:
	$(IDF_PY) set-target esp32s3

size:
	$(IDF_PY) size
