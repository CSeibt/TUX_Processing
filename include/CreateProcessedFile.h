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


class run_setup {
    public:
        run_setup() :    has_energy_channel(false), 
                        has_energy_cal(false), 
                        has_energy_short(false), 
                        has_waveforms(false), 
                        wf_code(0), 
                        wf_n_samples(0) {}
        ~run_setup() = default;

        bool has_energy_channel;
        bool has_energy_cal;
        bool has_energy_short;
        bool has_waveforms;
        char wf_code;
        UInt_t wf_n_samples;
        vector<string> bin_files;
};


int CreateTimeDiffTree(TTree* data_tree, TTree* timediff_tree, ProcSetup* proc_setup, const run_setup* my_run_setup);

int CreateProcessedFile(const string& run_path, int bin_index, ProcSetup* proc_setup, const run_setup* my_run_setup, bool is_first_file);

#endif