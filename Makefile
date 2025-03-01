all: application bootloader

application: FORCE
	@echo "Building application..."
	$(MAKE) -C application -j16

application-metadata: FORCE
	@echo "Generating metadata from ELF file..."
	$(MAKE) -C application metadata

bootloader: FORCE
	@echo "Building bootloader..."
	$(MAKE) -C bootloader -j16

flash-application:
	@echo "Flashing application..."
	$(MAKE) -C application flash -j16

flash-metadata:
	@echo "Flashing application metadata..."
	$(MAKE) -C application flash-metadata -j16

flash-bootloader:
	@echo "Flashing bootloader..."
	$(MAKE) -C bootloader flash -j16

clean:
	@echo "Cleaning application..."
	$(MAKE) -C application clean
	@echo "Cleaning bootloader..."
	$(MAKE) -C bootloader clean

FORCE:
