#include "ProcSetup.h"

#include <fstream>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>
#include <TFile.h>

using json = nlohmann::json;

namespace {

TF1* LoadCalibrationFunction(const json& detector_json, const string& detector_name)
{
	if (!detector_json.contains("calibration") ||
		!detector_json.at("calibration").is_object()) {
		throw runtime_error(
			"ProcSetup: main detector '" + detector_name +
			"' is missing the calibration object");
	}

	const json& calibration = detector_json.at("calibration");
	if (!calibration.contains("calibration_type") ||
		!calibration.at("calibration_type").is_string()) {
		throw runtime_error(
			"ProcSetup: main detector '" + detector_name +
			"' is missing calibration.calibration_type");
	}

	const string calibration_type = calibration.at("calibration_type").get<string>();
	if (calibration_type == "file") {
		if (!calibration.contains("calibration_file") ||
			!calibration.at("calibration_file").is_string() ||
			calibration.at("calibration_file").get<string>().empty()) {
			throw runtime_error(
				"ProcSetup: main detector '" + detector_name +
				"' is missing a non-empty calibration.calibration_file");
		}
		if (!calibration.contains("calibration_tf1_name") ||
			!calibration.at("calibration_tf1_name").is_string() ||
			calibration.at("calibration_tf1_name").get<string>().empty()) {
			throw runtime_error(
				"ProcSetup: main detector '" + detector_name +
				"' is missing a non-empty calibration.calibration_tf1_name");
		}

		const string calibration_file_path =
			calibration.at("calibration_file").get<string>();
		const string calibration_tf1_name =
			calibration.at("calibration_tf1_name").get<string>();
		TFile calibration_file(calibration_file_path.c_str(), "READ");
		if (calibration_file.IsZombie()) {
			throw runtime_error(
				"ProcSetup: unable to open calibration file '" +
				calibration_file_path + "' for main detector '" + detector_name + "'");
		}

		TObject* object = calibration_file.Get(calibration_tf1_name.c_str());
		TF1* source_function = dynamic_cast<TF1*>(object);
		if (source_function == nullptr) {
			throw runtime_error(
				"ProcSetup: calibration object '" + calibration_tf1_name +
				"' in '" + calibration_file_path +
				"' is not a TF1 for main detector '" + detector_name + "'");
		}

		TF1* cloned_function = dynamic_cast<TF1*>(
			source_function->Clone((detector_name + "_calibration").c_str()));
		if (cloned_function == nullptr) {
			throw runtime_error(
				"ProcSetup: failed to clone calibration TF1 '" +
				calibration_tf1_name + "' for main detector '" + detector_name + "'");
		}
		return cloned_function;
	}

	if (calibration_type == "linear") {
		if (!calibration.contains("p0") || !calibration.at("p0").is_number() ||
			!calibration.contains("p1") || !calibration.at("p1").is_number()) {
			throw runtime_error(
				"ProcSetup: linear calibration for main detector '" + detector_name +
				"' requires numeric calibration.p0 and calibration.p1");
		}

		const double p0 = calibration.at("p0").get<double>();
		const double p1 = calibration.at("p1").get<double>();
		auto function = make_unique<TF1>(
			(detector_name + "_calibration").c_str(), "[0] + [1]*x");
		function->SetParameters(p0, p1);
		return function.release();
	}

	throw runtime_error(
		"ProcSetup: unsupported calibration type '" + calibration_type +
		"' for main detector '" + detector_name +
		"'; expected 'file' or 'linear'");
}

} // namespace

// Convert the JSON representation into a complete, validated intermediate
// configuration before moving its values into the ProcSetup members.
ProcSetup::Config ProcSetup::LoadConfig(string path_to_json)
{
	ifstream file(path_to_json);
	if (!file.is_open()) {
		throw runtime_error("ProcSetup: unable to open JSON file: " + path_to_json);
	}

	json data;
	file >> data;

	// Read the fields that control processing behavior.
	ProcSetup::Config config{};
	const json& processing_setup = data.at("processing_setup");

	config.keep_saturation = processing_setup.value("keep_saturation", false);
	config.keep_pileup = processing_setup.value("keep_pileup", false);
	config.get_timediff = processing_setup.value("get_timediff", false);
	config.apply_veto = processing_setup.value("apply_veto", false);
	config.calibrate_detectors = processing_setup.value("calibrate_detectors", false);

	config.input_dir = data.value("input_dir", string{});
	config.run_list = data.value("runs", vector<string>{});
	config.output_dir = data.value("output_dir", string{});

	if (data.contains("veto_time_window_mus")) {
		// The JSON stores the veto window in microseconds, but the class keeps it in picoseconds.
		const double veto_time_window_mus = data.at("veto_time_window_mus").get<double>();
		config.veto_time_threshold_ps = static_cast<ULong64_t>(veto_time_window_mus * 1000000.0);
	} else {
		config.veto_time_threshold_ps = 0;
	}

	if (data.contains("main_detectors")) {
		if (!data.at("main_detectors").is_array()) {
			throw runtime_error("ProcSetup: main_detectors must be an array");
		}
		for (const auto& detector_json : data.at("main_detectors")) {
			if (!detector_json.is_object()) {
				throw runtime_error("ProcSetup: each main detector must be an object");
			}
			const string name = detector_json.value("name", string{});
			const UShort_t board = detector_json.value("board", static_cast<UShort_t>(0));
			const UShort_t channel = detector_json.value("channel", static_cast<UShort_t>(0));
			if (name.empty()) {
				throw runtime_error("ProcSetup: main detector is missing a non-empty name");
			}
			config.detectors.emplace_back(name, board, channel);
			if (config.calibrate_detectors) {
				config.detectors.back().calib_function =
					LoadCalibrationFunction(detector_json, name);
			}
		}
	}
	if (config.calibrate_detectors && config.detectors.empty()) {
		throw runtime_error(
			"ProcSetup: calibrate_detectors is true but no main detectors are configured");
	}

	if (data.contains("veto_detectors")) {
		for (const auto& detector_json : data.at("veto_detectors")) {
			// Veto detectors also carry an ADC threshold that is used later during veto checks.
			const string name = detector_json.value("name", string{});
			const UShort_t board = detector_json.value("board", static_cast<UShort_t>(0));
			const UShort_t channel = detector_json.value("channel", static_cast<UShort_t>(0));
			const UShort_t threshold = detector_json.value("adc_threshold", static_cast<UShort_t>(0));
			config.veto_detectors.emplace_back(name, board, channel, threshold);
		}
	}

	return config;
}

ProcSetup::ProcSetup(string path_to_json)
	: ProcSetup(LoadConfig(path_to_json), path_to_json)
{
	cout << "ProcSetup initialized with JSON file: " << this->path_to_json << "\n";
}

ProcSetup::ProcSetup(Config config, string path_to_json)
	: keep_saturation(config.keep_saturation),
	  keep_pileup(config.keep_pileup),
	  get_timediff(config.get_timediff),
	  apply_veto(config.apply_veto),
	  calibrate_detectors(config.calibrate_detectors),
	  path_to_json(std::move(path_to_json)),
	  raw_file_path(std::move(config.input_dir)),
	  run_list(std::move(config.run_list)),
	  output_dir(std::move(config.output_dir)),
	  detectors(std::move(config.detectors)),
	  veto_detectors(std::move(config.veto_detectors)),
	  veto_time_threshold_ps(config.veto_time_threshold_ps),
	  run_index(0)
{
	// All data members are initialized in the initializer list; no extra work is needed here.
	cout << "ProcSetup constructed with " << detectors.size() << " main detectors and "
		 << veto_detectors.size() << " veto detectors." << endl;
	cout << "Choose Verbosity level (0-3) for output messages: ";
	int verbosity_level;
	cin >> verbosity_level;
	if (verbosity_level < 0 || verbosity_level > 3) {
		cout << "Invalid verbosity level. Defaulting to 0." << endl;
		verbosity_level = 0;
	}
	verbose = verbosity_level;
	cout << "Verbosity level set to " << verbose << endl;

}

ProcSetup::~ProcSetup()
{
	for (const auto& detector : detectors) {
		if(detector.calib_function) {
			delete detector.calib_function;
		}
	}
}

// Accessors intentionally return the configured paths without adding
// separators; the JSON configuration is expected to provide them as needed.
string ProcSetup::GetRawPath()
{
	return raw_file_path;
}

string ProcSetup::GetOutputDir()
{
	return output_dir;
}

string ProcSetup::GetNextRun()
{
	string run = "";
	if (run_index >= run_list.size()) {
		return run;
	}
	run = run_list[run_index];
	run_index++;
	return run;
}

string ProcSetup::GetCurrentRun()
{
	if (run_index == 0 || run_index > run_list.size()) {
		return "";
	}
	return run_list[run_index - 1];
}

vector<UShort_t> ProcSetup::GetAllDetectorBoards()
{
	set<UShort_t> boards;

	// A set removes duplicates when several detector definitions share a board.
	for (const auto& detector : detectors) {
		boards.insert(detector.board);
	}

	for (const auto& detector : veto_detectors) {
		boards.insert(detector.board);
	}

	return vector<UShort_t>(boards.begin(), boards.end());
}

vector<UShort_t> ProcSetup::GetAllDetectorChannels()
{
	set<UShort_t> channels;

	for (const auto& detector : detectors) {
		channels.insert(detector.channel);
	}

	for (const auto& detector : veto_detectors) {
		channels.insert(detector.channel);
	}

	return vector<UShort_t>(channels.begin(), channels.end());
}

void ProcSetup::Print()
{
    // Keep this output human-readable: it is used to confirm the JSON setup
    // before processing begins.
	cout << "ProcSetup configuration:\n";
	cout << "  keep_saturation: " << keep_saturation << "\n";
	cout << "  keep_pileup: " << keep_pileup << "\n";
	cout << "  get_timediff: " << get_timediff << "\n";
	cout << "  apply_veto: " << apply_veto << "\n";
	cout << "  calibrate_detectors: " << calibrate_detectors << "\n";
	cout << "  input_dir: " << raw_file_path << "\n";
	cout << "  output_dir: " << output_dir << "\n";
	cout << "  veto_time_threshold_ps: " << veto_time_threshold_ps << "\n";

	cout << "  Main detectors:\n";
	for (const auto& detector : detectors) {
		cout << "    Name: " << detector.name
				<< ", Board: " << detector.board
				<< ", Channel: " << detector.channel
				<< "\n";
	}

	cout << "  Veto detectors:\n";
	for (const auto& detector : veto_detectors) {
		cout << "    Name: " << detector.name
				<< ", Board: " << detector.board
				<< ", Channel: " << detector.channel
				<< ", ADC Threshold: " << detector.veto_threshold
				<< "\n";
	}
}