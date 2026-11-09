#ifndef CREATE_PROCESSED_FILE_H
#define CREATE_PROCESSED_FILE_H 1

#include <string>
#include <vector>
#include <map>
#include <utility>
#include <TMath.h>
#include <TF1.h>
#include <TFile.h>
#include <TTree.h> 
#include <TTreeIndex.h>
#include "ProcSetup.h"


using namespace std;

/**
 * Describes optional fields present in the binary files of one run.
 *
 * The flags are read from the first file and reused while processing every
 * file belonging to the run, so all files are interpreted with one schema.
 */
class run_setup {
    public:
        run_setup() :    has_energy_channel(false), 
                        has_energy_cal(false), 
                        has_energy_short(false), 
                        has_waveforms(false), 
                        wf_code(0), 
                        wf_n_samples(0) {}
        ~run_setup() = default;

        bool has_energy_channel; ///< Whether the raw record contains ADC energy.
        bool has_energy_cal; ///< Whether the raw record contains calibrated energy.
        bool has_energy_short; ///< Whether the raw record contains short energy.
        bool has_waveforms; ///< Whether the raw record contains waveform samples.
        char wf_code; ///< Waveform encoding reported by the binary-file header.
        UInt_t wf_n_samples; ///< Number of waveform samples reported by the header.
        vector<string> bin_files; ///< Binary files that make up the current run.
};

/**
 * Build a tree containing the nearest detector events around each main event.
 *
 * @param data_tree Input tree containing events ordered by their raw timestamps.
 * @param timediff_tree Output tree to populate with neighbouring-event metadata.
 * @param proc_setup Processing flags and detector configuration.
 * @param my_run_setup Optional-field information discovered in the raw header.
 * @return Zero on success, or a non-zero error code when required data is absent.
 */
int CreateTimeDiffTree(TTree* data_tree, TTree* timediff_tree, ProcSetup* proc_setup, const run_setup* my_run_setup);

/**
 * Read one binary file and write its events to a processed ROOT file.
 *
 * @param run_path Path to the binary input file.
 * @param bin_index Position of the file within the run.
 * @param proc_setup Processing flags, output location, and detector settings.
 * @param my_run_setup Schema information read from the first binary file.
 * @param is_first_file Whether this file still contains its binary header.
 * @return Zero on success, or a non-zero error code on I/O/tree failure.
 */

 int CreateTimeDiffVetoTree(TTree* data_tree, TTree* timediff_tree, ProcSetup* proc_setup, const run_setup* my_run_setup);
 

int CreateProcessedFile(const string& run_path, int bin_index, ProcSetup* proc_setup, const run_setup* my_run_setup, bool is_first_file);

/**
 * Apply an energy calibration function to a raw ADC value.
 *
 * @param energy_ch Raw ADC energy channel.
 * @param calib_func Calibration function to evaluate.
 * @return Calibrated energy value.
 */
double ApplyCalibration(UShort_t energy_ch, TF1* calib_func);
#endif