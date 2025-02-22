#include "services/cam.hpp"
#include "ipc.hpp"
#include "kernel.hpp"

namespace CAMCommands {
	enum : u32 {
		GetBufferErrorInterruptEvent = 0x00060040,
		DriverInitialize = 0x00390000,
		GetMaxLines = 0x000A0080,
	};
}

void CAMService::reset() { bufferErrorInterruptEvents.fill(std::nullopt); }

void CAMService::handleSyncRequest(u32 messagePointer) {
	const u32 command = mem.read32(messagePointer);
	switch (command) {
		case CAMCommands::DriverInitialize: driverInitialize(messagePointer); break;
		case CAMCommands::GetBufferErrorInterruptEvent: getBufferErrorInterruptEvent(messagePointer); break;
		case CAMCommands::GetMaxLines: getMaxLines(messagePointer); break;
		default: Helpers::panic("CAM service requested. Command: %08X\n", command);
	}
}

void CAMService::driverInitialize(u32 messagePointer) {
	log("CAM::DriverInitialize\n");
	mem.write32(messagePointer, IPC::responseHeader(0x39, 1, 0));
	mem.write32(messagePointer + 4, Result::Success);
}

// Algorithm taken from Citra
// https://github.com/citra-emu/citra/blob/master/src/core/hle/service/cam/cam.cpp#L465
void CAMService::getMaxLines(u32 messagePointer) {
	const u16 width = mem.read16(messagePointer + 4);
	const u16 height = mem.read16(messagePointer + 8);
	log("CAM::GetMaxLines (width = %d, height = %d)\n", width, height);

	constexpr u32 MIN_TRANSFER_UNIT = 256;
	constexpr u32 MAX_BUFFER_SIZE = 2560;
	if (width * height * 2 % MIN_TRANSFER_UNIT != 0) {
		Helpers::panic("CAM::GetMaxLines out of range");
	} else {
		u32 lines = MAX_BUFFER_SIZE / width;
		if (lines > height) {
			lines = height;
		}

		u32 result = Result::Success;
		while (height % lines != 0 || (lines * width * 2 % MIN_TRANSFER_UNIT != 0)) {
			--lines;
			if (lines == 0) {
				Helpers::panic("CAM::GetMaxLines out of range");
				break;
			}
		}

		mem.write32(messagePointer, IPC::responseHeader(0xA, 2, 0));
		mem.write32(messagePointer + 4, result);
		mem.write16(messagePointer + 8, lines);
	}
}

void CAMService::getBufferErrorInterruptEvent(u32 messagePointer) {
	const u32 port = mem.read32(messagePointer + 4);
	log("CAM::GetBufferErrorInterruptEvent (port = %d)\n", port);

	mem.write32(messagePointer, IPC::responseHeader(0x6, 1, 2));

	if (port >= portCount) {
		Helpers::panic("CAM::GetBufferErrorInterruptEvent: Invalid port");
	} else {
		auto& event = bufferErrorInterruptEvents[port];
		if (!event.has_value()) {
			event = kernel.makeEvent(ResetType::OneShot);
		}

		mem.write32(messagePointer + 4, Result::Success);
		mem.write32(messagePointer + 8, 0);
		mem.write32(messagePointer + 12, event.value());
	}
}