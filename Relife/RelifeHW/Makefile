.PHONY: all hw-test hw-build mock contract-test clean help

PYTHON ?= python

help:
	@echo "ReLife Hardware Rig Build & Test Targets:"
	@echo "  make hw-test        Run native Unity tests via PlatformIO"
	@echo "  make hw-build       Compile firmware for esp32dev environment"
	@echo "  make mock           Launch FastAPI mock backend server (port 8000)"
	@echo "  make contract-test  Execute contracts/v1 compliance test suite"
	@echo "  make clean          Clean build artifacts and temporary cache"

hw-test:
	cd firmware && $(PYTHON) -m platformio test -e native

hw-build:
	cd firmware && $(PYTHON) -m platformio run -e esp32dev

mock:
	$(PYTHON) -m uvicorn tools.hw.mock_server.main:app --host 0.0.0.0 --port 8000 --reload

contract-test:
	$(PYTHON) -m pytest tools/hw/test_contracts.py -v

clean:
	cd firmware && $(PYTHON) -m platformio run --target clean
