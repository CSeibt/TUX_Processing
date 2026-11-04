#ifndef PROC_SETUP_H
#define PROC_SETUP_H 1

#include <string>
#include <vector>
#include <utility>
#include <TMath.h>
#include <TF1.h>

using namespace std;

/** Configuration for one main (non-veto) detector channel. */
struct main_detector{
    string name; ///< Name used for generated output branches.
    UShort_t board; ///< Digitizer board number.
    UShort_t channel; ///< Digitizer channel number.
    TF1* calib_function; ///< Optional calibration function, owned elsewhere.
    // The calibration function is optional; it is filled later if detector calibration is enabled.
    main_detector(string n, UShort_t b, UShort_t c) : name(move(n)), board(b), channel(c), calib_function(nullptr) {}
};

/** Configuration for a detector channel used to veto nearby events. */
struct veto_detector{
    string name; ///< Name used for generated output branches.
    UShort_t board; ///< Digitizer board number.
    UShort_t channel; ///< Digitizer channel number.
    UShort_t veto_threshold; ///< Minimum ADC energy that triggers the veto.
    // Threshold is the ADC cut used to decide whether this veto detector fires.
    veto_detector(string n, UShort_t b, UShort_t c, UShort_t v) : name(n), board(b), channel(c), veto_threshold(v) {}
};

/**
 * Loads processing options and detector definitions from a JSON configuration.
 *
 * A ProcSetup instance also owns the run-list cursor used by ProcessRun.
 */
class ProcSetup {
    public:
        /** Parse `path_to_json` and initialize the processing configuration. */
        ProcSetup(string path_to_json);
        ~ProcSetup();
        string GetRawPath(); ///< Return the directory containing raw runs.
        string GetOutputDir(); ///< Return the directory for processed ROOT files.
        string GetNextRun(); ///< Return and advance to the next configured run.
        string GetCurrentRun(); ///< Return the most recently returned run.
        void Print(); ///< Print the loaded configuration to standard output.
        vector<UShort_t> GetAllDetectorBoards(); ///< Return unique configured boards.
        vector<UShort_t> GetAllDetectorChannels(); ///< Return unique configured channels.

        vector<main_detector> GetMainDetectors() const { return detectors; } ///< Return main detectors.
        vector<veto_detector> GetVetoDetectors() const { return veto_detectors; } ///< Return veto detectors.
        ULong64_t GetVetoTimeThreshold() const { return veto_time_threshold_ps; } ///< Return the veto window in ps.

        // Constant variables that are set with the constructor, can be accessed from outside without getters:
        const bool keep_saturation;
        const bool keep_pileup;
        const bool get_timediff;
        const bool apply_veto;
        const bool calibrate_detectors;
        int verbose;

    private:
        // Parsed values from the JSON file are copied into this temporary structure first.
        // This keeps file reading separate from member initialization and makes the flow easier to follow.
        struct Config {
            bool keep_saturation;
            bool keep_pileup;
            bool get_timediff;
            bool apply_veto;
            bool calibrate_detectors;
            string input_dir;
            vector<string> run_list;
            string output_dir;
            vector<main_detector> detectors;
            vector<veto_detector> veto_detectors;
            ULong64_t veto_time_threshold_ps;
        };

        // Load all values from the JSON file into Config.
        ProcSetup(Config config, string path_to_json);
        // Helper that reads and interprets the JSON file on disk.
        static Config LoadConfig(string path_to_json);

        string path_to_json;
        string raw_file_path;
        vector<string> run_list;
        string output_dir;
        
        // Detector settings:
        vector<main_detector> detectors;
        vector<veto_detector> veto_detectors;
        ULong64_t veto_time_threshold_ps;
        // Helper variables
        size_t run_index;

};

#endif // PROC_SETUP_H