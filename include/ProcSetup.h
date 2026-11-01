#ifndef PROC_SETUP_H
#define PROC_SETUP_H 1

#include <string>
#include <vector>
#include <utility>
#include <TMath.h>
#include <TF1.h>

using namespace std;

struct main_detector{
    string name;
    UShort_t board;
    UShort_t channel;
    TF1* calib_function;
    // The calibration function is optional; it is filled later if detector calibration is enabled.
    main_detector(string n, UShort_t b, UShort_t c) : name(move(n)), board(b), channel(c), calib_function(nullptr) {}
};

struct veto_detector{
    string name;
    UShort_t board;
    UShort_t channel;
    UShort_t veto_threshold;
    // Threshold is the ADC cut used to decide whether this veto detector fires.
    veto_detector(string n, UShort_t b, UShort_t c, UShort_t v) : name(n), board(b), channel(c), veto_threshold(v) {}
};

class ProcSetup {
    public:
        ProcSetup(string path_to_json);
        ~ProcSetup();
        string GetRawPath();
        string GetOutputDir();
        string GetNextRun();
        string GetCurrentRun();
        void Print();
        vector<UShort_t> GetAllDetectorBoards();
        vector<UShort_t> GetAllDetectorChannels();

        vector<main_detector> GetMainDetectors() const { return detectors; }
        vector<veto_detector> GetVetoDetectors() const { return veto_detectors; }
        ULong64_t GetVetoTimeThreshold() const { return veto_time_threshold_ps; }

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