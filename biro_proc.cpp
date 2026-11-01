// CLI entry point for a JSON-based processor.

#include <filesystem>
#include <iostream>
#include <string>

#include "ProcSetup.h"
#include "ProcessRun.h"

namespace {

void printUsage(const char* programName) {
	std::cout << "Usage: " << programName << " <input.json>\n"
			  << "\n"
			  << "Provide a JSON file to start processing.\n"
			  << "Use --help or -h to display this message.\n";
}

}  // namespace

int main(int argc, char* argv[]) {
	const std::string programName = (argc > 0 && argv[0] != nullptr) ? argv[0] : "biro_proc";

	if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
		printUsage(programName.c_str());
		return 0;
	}

	const std::filesystem::path inputPath(argv[1]);
	if (!std::filesystem::exists(inputPath)) {
		std::cerr << "Error: file not found: " << inputPath.string() << "\n";
		return 1;
	}

	if (inputPath.extension() != ".json") {
		std::cerr << "Error: expected a JSON file: " << inputPath.string() << "\n";
		return 1;
	}

	std::cout << "Processing JSON file: " << inputPath.string() << "\n";

	ProcSetup* procSetup = new ProcSetup(inputPath.string());
	procSetup->Print();

	int debug = 0;
	string run = procSetup->GetNextRun();
	while (!run.empty()) {
		debug = ProcessRun(procSetup, run);
		if (debug != 0) {
			cerr << "Warning! ProcessRun exited with error code " << debug << endl;
		}
		run = procSetup->GetNextRun();
	}

	delete procSetup;

	return 0;
}
