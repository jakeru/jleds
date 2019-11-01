default: build

build:
	pio run

upload:
	pio run --target upload