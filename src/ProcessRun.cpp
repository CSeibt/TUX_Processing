#include "ProcessRun.h"

// Raw files are named with the run identifier and use the .BIN extension.
vector<string> GetBinFileList(const string& run_path, const string& run) {
    vector<string> bin_file_list;

    cout << "Searching for bin files in: " << run_path << endl;
    DIR* dir = opendir(run_path.c_str());
    if (!dir) {
        cout << "Could not open directory: " << run_path << endl;
        return bin_file_list;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        string file_name = ent->d_name;
        if (file_name.find(run) != string::npos && file_name.find(".BIN") != string::npos) {
            bin_file_list.push_back(run_path + file_name);
        }
    }
    closedir(dir);
    sort(bin_file_list.begin(), bin_file_list.end());
    cout << "Found the following bin files: " << endl;
    for (const auto& file : bin_file_list) {
        cout << file << endl;
    }
    return bin_file_list;
}

// The first two bytes describe which optional fields follow each event.
int ReadHeader(run_setup* my_run_setup) {
    UShort_t header;
    UShort_t board, channel, energy_ch;
    ULong64_t time;
    UInt_t flags;
    Double_t energy_cal;
    Short_t energy_short;
    Char_t wf_code;
    Int_t num_wf_samples;
    FILE* file_h = fopen((my_run_setup->bin_files[0]).c_str(), "r");
    if (!file_h) { 
        cout << "Problem while opening first file" << endl;
        return 1;
    }
    fread(&header, sizeof(header), 1, file_h);
    cout << "Header: " << header << endl;
    my_run_setup->has_energy_channel = bool(header & 0x1);
    my_run_setup->has_energy_cal = bool(header & 0x2);
    my_run_setup->has_energy_short = bool(header & 0x4);
    my_run_setup->has_waveforms = bool(header & 0x8);

    if (my_run_setup->has_waveforms) {
        fread(&board, sizeof(board), 1, file_h);
        fread(&channel, sizeof(channel), 1, file_h);
        fread(&time, sizeof(time), 1, file_h);
        if (my_run_setup->has_energy_channel) {
            fread(&energy_ch, sizeof(energy_ch), 1, file_h);
        }
        if (my_run_setup->has_energy_cal) {
            fread(&energy_cal, sizeof(energy_cal), 1, file_h);
        }
        if (my_run_setup->has_energy_short) {
            fread(&energy_short, sizeof(energy_short), 1, file_h);
        }
        fread(&flags, sizeof(flags), 1, file_h);
        fread(&wf_code, sizeof(wf_code), 1, file_h);
        fread(&num_wf_samples, sizeof(num_wf_samples), 1, file_h);
        my_run_setup->wf_code = wf_code;
        my_run_setup->wf_n_samples = num_wf_samples;
    }

    fclose(file_h);
    cout << "Finished reading header!" << endl;
    return 0;
}

int ProcessRun(ProcSetup* setup, string run) {
    cout << "Processing run: " << run << endl;
    string run_path = setup->GetRawPath() + run + "/RAW/";
    run_setup my_run_setup;
    // The header determines how every record in this run must be decoded.
    my_run_setup.bin_files = GetBinFileList(run_path, run);
    int debug = 0;
    if (my_run_setup.bin_files.size() == 0) {
        debug = 1;
        cout << "No files could be found in " << run_path << "!" << endl;
        return debug;
    }
    debug = ReadHeader(&my_run_setup);
    if (debug != 0) {
        cerr << "Warning! ReadHeader exited with error code " << debug << endl;
        return 11;
    }

    // The first file includes the shared binary header; subsequent files start
    // directly with event records.
    for (int i = 0; i < my_run_setup.bin_files.size(); i++) {
        if (i == 0) {
            debug = CreateProcessedFile(my_run_setup.bin_files[i], i, setup, &my_run_setup, true);
        } else {
            debug = CreateProcessedFile(my_run_setup.bin_files[i], i, setup, &my_run_setup, false);
        }
        if (debug != 0) {
            cerr << "Warning! CreateProcessedFile exited with error code " << debug << endl;
            return 12;
        }
    }

    return 0;
    

}