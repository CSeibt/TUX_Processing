#include "CreateProcessedFile.h"

/**
 * Decode one binary input file into a ROOT tree and optionally derive the
 * time-difference tree before closing the output file.
 */

double ApplyCalibration(UShort_t energy_ch, TF1* calib_function) {
    if (calib_function) {
        // Apply calibration function to the raw ADC value
        return calib_function->Eval(static_cast<double>(energy_ch));
    } else {
        // If calibration_function is nullptr, return the raw energy value and print a warning
        cerr << "Warning: No calibration function provided. Returning raw energy." << endl;
        return static_cast<double>(energy_ch);
    }
    // Just the end
    return 0;
}

int CreateProcessedFile(const string& run_path, int bin_index, ProcSetup* proc_setup, const run_setup* my_run_setup, bool is_first_file) {
    int verbose = proc_setup->verbose;
    cout << "Processing file: " << run_path << endl;
    cout << "Is first file: " << (is_first_file ? "Yes" : "No") << endl;
    int debug = 0;

    string base_tree_name = "Data";
    string output_file_name = proc_setup->GetOutputDir() + proc_setup->GetCurrentRun() + "_processed_"+ to_string(bin_index) + ".root";
    string timediff_tree_name = "Data_Timediff";
    
    // Each input file receives its own output file, while the run name keeps
    // the files grouped by acquisition.
    cout << "Output file name: " << output_file_name << endl;
    cout << "Base tree name: " << base_tree_name << endl;
    
    FILE* binfile = fopen(run_path.c_str(), "r");
    if (!binfile) {
        cerr << "Error: could not open binary file: " << run_path << endl;
        return 1; // Return an error code if the file cannot be opened
    }

    // Only the first file carries the run-level header.
    UShort_t header;
    if (is_first_file) {
        fread(&header, sizeof(header), 1, binfile);
        cout << "Header read from first file: " << header << endl;
    }
    
    UShort_t board;
    UShort_t channel;
    ULong64_t time;
    UShort_t energy_ch;
    Double_t energy_cal;
    Short_t energy_short;
    UInt_t flags;
    Bool_t pileup = false;
    Bool_t saturation = false;
    Char_t wf_code;
    Int_t num_wf_samples;
    vector<int> *wfs = new vector<int>();
    map<pair<int, int>, TF1*> calibration_map;

    // Reserve waveform storage once so repeated events reuse the same buffer.
    if (my_run_setup->has_waveforms) {
        wfs->reserve(num_wf_samples);
    }

    // Create calibration functions for main detectors if calibration is enabled.
    if (proc_setup->calibrate_detectors) {
        for (const auto& detector : proc_setup->GetMainDetectors()) {
            if (detector.calib_function) {
                calibration_map[{detector.board, detector.channel}] = detector.calib_function;
            }
        }
    }

    TFile* outfile = new TFile(output_file_name.c_str(), "RECREATE");
    TTree* data_tree = new TTree(base_tree_name.c_str(), base_tree_name.c_str());

    // Branches are created conditionally to match the schema reported by the
    // binary header and avoid reading fields that are not present.
    data_tree->Branch("Board", &board, "Board/s");
    data_tree->Branch("Channel", &channel, "Channel/s");
    data_tree->Branch("Time", &time, "Time/l");
    if (my_run_setup->has_energy_channel) {
        data_tree->Branch("Energy_Ch", &energy_ch, "Energy_Ch/s");
    }
    if (my_run_setup->has_energy_cal || proc_setup->calibrate_detectors) {
        data_tree->Branch("Energy_Cal", &energy_cal, "Energy_Cal/D");
    }
    if (my_run_setup->has_energy_short) {
        data_tree->Branch("Energy_Short", &energy_short, "Energy_Short/S");
    }
    data_tree->Branch("Flags", &flags, "Flags/i");
    data_tree->Branch("Pileup", &pileup, "Pileup/O");
    data_tree->Branch("Saturation", &saturation, "Saturation/O");
    if (my_run_setup->has_waveforms) {
        data_tree->Branch("Wf_Code", &wf_code, "Wf_Code/C");
        data_tree->Branch("Waveforms", &wfs);
    }
    if (verbose > 0) {
        cout << "Branches created for data tree." << endl;
        if (verbose > 1) {
            cout << "Branch details:" << endl;
            data_tree->Print();
        }
    }
    int num_events = 0;
    // Decode records until the file has no complete board value left.
    while (fread(&board, sizeof(board), 1, binfile)) {
        fread(&channel, sizeof(channel), 1, binfile);
        fread(&time, sizeof(time), 1, binfile);
        if (my_run_setup->has_energy_channel) {
            fread(&energy_ch, sizeof(energy_ch), 1, binfile);
        }
        if (my_run_setup->has_energy_cal) {
            fread(&energy_cal, sizeof(energy_cal), 1, binfile);
        }
        if (my_run_setup->has_energy_short) {
            fread(&energy_short, sizeof(energy_short), 1, binfile);
        }
        fread(&flags, sizeof(flags), 1, binfile);
        pileup = bool(flags & 0x8000);
        saturation = bool(flags & 0x80) || bool(flags & 0x400);
        if (my_run_setup->has_waveforms) {
            fread(&wf_code, sizeof(wf_code), 1, binfile);
            fread(&num_wf_samples, sizeof(num_wf_samples), 1, binfile);
            wfs->resize(num_wf_samples);
            for (int i = 0; i < num_wf_samples; i++) {
                Short_t wf_sample;
                fread(&wf_sample, sizeof(wf_sample), 1, binfile);
                wfs->at(i) = static_cast<int>(wf_sample);
            }
        }
        // Filtering happens before Fill so rejected events never enter the
        // processed tree.
        if (!(proc_setup->keep_saturation) && saturation) {
            continue; // Skip this event if saturation is not to be kept
        }
        if (!(proc_setup->keep_pileup) && pileup) {
            continue; // Skip this event if pileup is not to be kept
        }

        if (proc_setup->calibrate_detectors) {
            TF1* calib_func = calibration_map[{board, channel}];
            energy_cal = ApplyCalibration(energy_ch, calib_func);
        }

        num_events++;
        if (verbose > 0 && num_events % 100000 == 0) {
            cout << "Processed " << num_events << " events." << endl;
        }
        if (verbose > 2) {
            cout << "Event " << num_events << ": Board=" << board
                 << ", Channel=" << channel
                 << ", Time=" << time
                 << ", Energy_Ch=" << energy_ch
                 << ", Energy_Cal=" << energy_cal
                 << ", Energy_Short=" << energy_short
                 << ", Flags=" << flags
                 << ", Pileup=" << pileup
                 << ", Saturation=" << saturation;
            if (my_run_setup->has_waveforms) {
                cout << ", Wf_Code=" << wf_code
                     << ", Num_Wf_Samples=" << num_wf_samples;
            }
            cout << endl;
        }
        data_tree->Fill();
        wfs->clear();
        pileup = false;
        saturation = false;
    }

    cout << "Finished processing binary file: " << run_path << endl;
    

    data_tree->Write();
    if (proc_setup->get_timediff) {
        // The raw tree is retained because it is the source for neighbour
        // searches while the derived tree is being built.
        cout << "Creating time difference tree..." << endl;
        TTree* timediff_tree = new TTree(timediff_tree_name.c_str(), timediff_tree_name.c_str());
        // Fill the timediff_tree with appropriate data
        if (proc_setup->apply_veto) {
            cout << "Applying veto during time difference tree creation." << endl;
            debug = CreateTimeDiffVetoTree(data_tree, timediff_tree, proc_setup, my_run_setup);
        } else {
            cout << "Not applying veto during time difference tree creation." << endl;
            debug = CreateTimeDiffTree(data_tree, timediff_tree, proc_setup, my_run_setup);
        }
        
        if (debug != 0) {
            cerr << "Error creating time difference tree. Exiting." << endl;
            outfile->Close();
            delete outfile;
            delete wfs;
            return debug; // Return the error code from CreateTimeDiffTree
        }
        timediff_tree->Write();
    }
    outfile->Close();
    cout << "Output file written: " << output_file_name << endl;
    delete outfile;
    delete wfs;

    return 0; // Return 0 to indicate success
}

/**
 * Add nearest-before and nearest-after detector information for each main
 * detector event. Entries are traversed in a timestamp index so the source
 * tree does not need to be physically sorted.
 */
int CreateTimeDiffTree(TTree* data_tree, TTree* timediff_tree, ProcSetup* proc_setup, const run_setup* my_run_setup) {
    cout << "Creating time difference tree..." << endl;

    int verbose = proc_setup->verbose;

    vector<main_detector> main_detectors = proc_setup->GetMainDetectors();
    vector<veto_detector> veto_detectors = proc_setup->GetVetoDetectors();

    // These variables are bound to the input tree and are overwritten while
    // searching neighbouring entries.
    UShort_t board, channel, energy_ch;
    Double_t energy_cal;
    Short_t energy_short;
    UInt_t flags;
    ULong64_t time;
    Bool_t pileup, saturation;
    Char_t wf_code;
    vector<int> *wfs = new vector<int>();

    UShort_t current_board, current_channel, current_energy_ch;
    Double_t current_energy_cal;
    Short_t current_energy_short;
    UInt_t current_flags;
    ULong64_t current_time;
    Bool_t current_pileup, current_saturation;
    Char_t current_wf_code;
    vector<int> *current_wfs = new vector<int>();

    // Maps are keyed by board/channel so output branches can be generated for
    // both main and veto detector definitions.
    map<pair<int, int>, UShort_t> energy_dep_before;
    map<pair<int, int>, UShort_t> energy_dep_after;
    map<pair<int, int>, ULong64_t> time_diff_before;
    map<pair<int, int>, ULong64_t> time_diff_after;
    vector<pair<int, int>> detector_channels;
    for (const auto& det : main_detectors) {
        energy_dep_before[{det.board, det.channel}] = 0;
        energy_dep_after[{det.board, det.channel}] = 0;
        time_diff_before[{det.board, det.channel}] = kMaxULong64;
        time_diff_after[{det.board, det.channel}] = kMaxULong64;
        detector_channels.push_back({det.board, det.channel});
    }
    for (const auto& det : veto_detectors) {
        energy_dep_before[{det.board, det.channel}] = 0;
        energy_dep_after[{det.board, det.channel}] = 0;
        time_diff_before[{det.board, det.channel}] = kMaxULong64;
        time_diff_after[{det.board, det.channel}] = kMaxULong64;
        detector_channels.push_back({det.board, det.channel});
    }

    // Print the initialized state when constructing the derived-tree schema.
    cout << "Testing the before/after maps" << endl;
    for (const auto& entry : energy_dep_before) {
        cout << "Board: " << entry.first.first << ", Channel: " << entry.first.second << ", Energy Dep Before: " << entry.second << endl;
    }
    for (const auto& entry : energy_dep_after) {
        cout << "Board: " << entry.first.first << ", Channel: " << entry.first.second << ", Energy Dep After: " << entry.second << endl;
    }
    for (const auto& entry : time_diff_before) {
        cout << "Board: " << entry.first.first << ", Channel: " << entry.first.second << ", Time Diff Before: " << entry.second << endl;
    }
    for (const auto& entry : time_diff_after) {
        cout << "Board: " << entry.first.first << ", Channel: " << entry.first.second << ", Time Diff After: " << entry.second << endl;
    }
    for (const auto& det : detector_channels) {
        cout << "Detector Board: " << det.first << ", Channel: " << det.second << endl;
    }

    data_tree->SetBranchAddress("Board", &board);
    data_tree->SetBranchAddress("Channel", &channel);
    if (my_run_setup->has_energy_channel) {
        data_tree->SetBranchAddress("Energy_Ch", &energy_ch);
    } else {
        cout << "Warning: Energy channel data not available in the input tree." << endl;
        return 21;
    }
    if (my_run_setup->has_energy_cal || proc_setup->calibrate_detectors) {
        data_tree->SetBranchAddress("Energy_Cal", &energy_cal);
    }
    if (my_run_setup->has_energy_short) {
        data_tree->SetBranchAddress("Energy_Short", &energy_short);
    }
    data_tree->SetBranchAddress("Flags", &flags);
    data_tree->SetBranchAddress("Time", &time);
    data_tree->SetBranchAddress("Pileup", &pileup);
    data_tree->SetBranchAddress("Saturation", &saturation);
    if (my_run_setup->has_waveforms) {
        data_tree->SetBranchAddress("Wf_Code", &wf_code);
        data_tree->SetBranchAddress("Waveforms", &wfs);
    }

    data_tree->BuildIndex("Time");
    TTreeIndex* index = (TTreeIndex*)data_tree->GetTreeIndex();
    if (!index) {
        cout << "Failed to build index on Timestamp. Exiting." << endl;
        return 22;
    }
    Long64_t* index_list = index->GetIndex();
    Long64_t n_entries = data_tree->GetEntries();

    timediff_tree->Branch("Board", &current_board, "Board/s");
    timediff_tree->Branch("Channel", &current_channel, "Channel/s");
    timediff_tree->Branch("Energy_Ch", &current_energy_ch, "Energy_Ch/s");
    if (my_run_setup->has_energy_cal || proc_setup->calibrate_detectors) {
        timediff_tree->Branch("Energy_Cal", &current_energy_cal, "Energy_Cal/D");
    }
    if (my_run_setup->has_energy_short) {
        timediff_tree->Branch("Energy_Short", &current_energy_short, "Energy_Short/S");
    }
    timediff_tree->Branch("Flags", &current_flags, "Flags/i");
    timediff_tree->Branch("Time", &current_time, "Time/l");
    timediff_tree->Branch("Pileup", &current_pileup, "Pileup/O");
    timediff_tree->Branch("Saturation", &current_saturation, "Saturation/O");
    if (my_run_setup->has_waveforms) {
        timediff_tree->Branch("Wf_Code", &current_wf_code, "Wf_Code/C");
        timediff_tree->Branch("Waveforms", &current_wfs);
    }

    for (const auto &det : main_detectors) {
        string branch_name_before = "TimeDiffBefore_" + det.name;
        timediff_tree->Branch(branch_name_before.c_str(), 
        &time_diff_before[{det.board, det.channel}], (branch_name_before + "/l").c_str());
        
        string branch_name_after = "TimeDiffAfter_" + det.name;
        timediff_tree->Branch(branch_name_after.c_str(), 
        &time_diff_after[{det.board, det.channel}], (branch_name_after + "/l").c_str());
        
        string branch_name_energy_before = "EnergyDepBefore_" + det.name;
        timediff_tree->Branch(branch_name_energy_before.c_str(), 
        &energy_dep_before[{det.board, det.channel}], (branch_name_energy_before + "/s").c_str());
        
        string branch_name_energy_after = "EnergyDepAfter_" + det.name;
        timediff_tree->Branch(branch_name_energy_after.c_str(), 
        &energy_dep_after[{det.board, det.channel}], (branch_name_energy_after + "/s").c_str());
    }

    for (const auto &det : veto_detectors) {
        string branch_name_before = "TimeDiffBefore_" + det.name;
        timediff_tree->Branch(branch_name_before.c_str(), 
        &time_diff_before[{det.board, det.channel}], (branch_name_before + "/l").c_str());
        
        string branch_name_after = "TimeDiffAfter_" + det.name;
        timediff_tree->Branch(branch_name_after.c_str(), 
        &time_diff_after[{det.board, det.channel}], (branch_name_after + "/l").c_str());
        
        string branch_name_energy_before = "EnergyDepBefore_" + det.name;
        timediff_tree->Branch(branch_name_energy_before.c_str(), 
        &energy_dep_before[{det.board, det.channel}], (branch_name_energy_before + "/s").c_str());
        
        string branch_name_energy_after = "EnergyDepAfter_" + det.name;
        timediff_tree->Branch(branch_name_energy_after.c_str(), 
        &energy_dep_after[{det.board, det.channel}], (branch_name_energy_after + "/s").c_str());
    }

    cout << "Processing entries to create time difference tree" << endl;
    cout << "Total entries in raw tree: " << n_entries << endl;
    int percent = 1;
    for (Long64_t i = 0; i < n_entries; i++) {
        if (verbose > 0) {
            if (verbose > 1 && i < 100) {
                cout << "Processing entry " << i << " of " << n_entries << endl;
            }
            if (i == percent * int(n_entries / 100)) {
                cout << "\rProgress: " << (100.0 * i / n_entries) << "%" << flush;
                percent++;
            }
        }

        data_tree->GetEntry(index_list[i]);
        bool matches_main = false;
        // The output first copies the current event, then appends detector
        // proximity and deposited-energy branches.
        for (const auto& det : main_detectors) {
            if (board == det.board && channel == det.channel) {
                matches_main = true;
                break;
            }
        }
        if (!matches_main) {continue;}
        current_board = board;
        current_channel = channel;
        current_energy_ch = energy_ch;
        if (my_run_setup->has_energy_cal || proc_setup->calibrate_detectors) {
            current_energy_cal = energy_cal;
        }
        if (my_run_setup->has_energy_short) {
            current_energy_short = energy_short;
        }
        current_flags = flags;
        current_time = time;
        current_pileup = pileup;
        current_saturation = saturation;
        if (my_run_setup->has_waveforms) {
            current_wf_code = wf_code;
            current_wfs = new vector<int>(*wfs);
        }
        // Search outward in the timestamp index for the closest event on each
        // configured detector channel.
        for (const auto& det : detector_channels) {
            time_diff_before[det] = kMaxULong64;
            energy_dep_before[det] = 0;
            for (Long64_t j = i - 1; j >= 0; j--) {
                data_tree->GetEntry(index_list[j]);
                if (board == det.first && channel == det.second) {
                    time_diff_before[det] = current_time - time;
                    energy_dep_before[det] = energy_ch;
                    break;
                }
            }
            time_diff_after[det] = kMaxULong64;
            energy_dep_after[det] = 0;
            for (Long64_t j = i + 1; j < n_entries; j++) {
                data_tree->GetEntry(index_list[j]);
                if (board == det.first && channel == det.second) {
                    time_diff_after[det] = time - current_time;
                    energy_dep_after[det] = energy_ch;
                    break;
                }
            }
        }
    timediff_tree->Fill();
    }


    cout << "Finished creating time diff tree, return to CreateProcessedFile ... "  << endl;

    return 0;
}

/**
 * Create a time-difference tree and omit main events that fall inside a
 * configured veto detector's time and energy window.
 */
int CreateTimeDiffVetoTree(TTree* data_tree, TTree* timediff_tree, ProcSetup* proc_setup, const run_setup* my_run_setup) {
    int verbose = proc_setup->verbose;
    vector<main_detector> main_detectors = proc_setup->GetMainDetectors();
    vector<veto_detector> veto_detectors = proc_setup->GetVetoDetectors();
    ULong64_t veto_time_threshold = proc_setup->GetVetoTimeThreshold();

    if (verbose > 0) {
        cout << "Veto time threshold: " << veto_time_threshold << " ps" << endl;
    }

    UShort_t board, channel, energy_ch;
    Double_t energy_cal;
    Short_t energy_short;
    UInt_t flags;
    ULong64_t time;
    Bool_t pileup, saturation;
    Char_t wf_code;
    vector<int>* wfs = new vector<int>();

    UShort_t current_board, current_channel, current_energy_ch;
    Double_t current_energy_cal;
    Short_t current_energy_short;
    UInt_t current_flags;
    ULong64_t current_time;
    Bool_t current_pileup, current_saturation, veto;
    Char_t current_wf_code;
    vector<int>* current_wfs = new vector<int>();

    map<pair<int, int>, UShort_t> energy_dep_before, energy_dep_after;
    map<pair<int, int>, ULong64_t> time_diff_before, time_diff_after;
    map<pair<int, int>, UShort_t> veto_thresholds;
    vector<pair<int, int>> detector_channels;

    for (const auto& det : main_detectors) {
        pair<int, int> key = {det.board, det.channel};
        energy_dep_before[key] = energy_dep_after[key] = 0;
        time_diff_before[key] = time_diff_after[key] = kMaxULong64;
        detector_channels.push_back(key);
    }
    for (const auto& det : veto_detectors) {
        pair<int, int> key = {det.board, det.channel};
        energy_dep_before[key] = energy_dep_after[key] = 0;
        time_diff_before[key] = time_diff_after[key] = kMaxULong64;
        veto_thresholds[key] = det.veto_threshold;
        detector_channels.push_back(key);
    }

    data_tree->SetBranchAddress("Board", &board);
    data_tree->SetBranchAddress("Channel", &channel);
    if (!my_run_setup->has_energy_channel) {
        cerr << "Warning: Energy channel data not available in the input tree." << endl;
        delete wfs;
        delete current_wfs;
        return 21;
    }
    data_tree->SetBranchAddress("Energy_Ch", &energy_ch);
    if (my_run_setup->has_energy_cal || proc_setup->calibrate_detectors)
        data_tree->SetBranchAddress("Energy_Cal", &energy_cal);
    if (my_run_setup->has_energy_short)
        data_tree->SetBranchAddress("Energy_Short", &energy_short);
    data_tree->SetBranchAddress("Flags", &flags);
    data_tree->SetBranchAddress("Time", &time);
    data_tree->SetBranchAddress("Pileup", &pileup);
    data_tree->SetBranchAddress("Saturation", &saturation);
    if (my_run_setup->has_waveforms) {
        data_tree->SetBranchAddress("Wf_Code", &wf_code);
        data_tree->SetBranchAddress("Waveforms", &wfs);
    }

    data_tree->BuildIndex("Time");
    TTreeIndex* index = (TTreeIndex*)data_tree->GetTreeIndex();
    if (!index) {
        delete wfs;
        delete current_wfs;
        return 22;
    }
    Long64_t* index_list = index->GetIndex();
    Long64_t n_entries = data_tree->GetEntries();

    timediff_tree->Branch("Board", &current_board, "Board/s");
    timediff_tree->Branch("Channel", &current_channel, "Channel/s");
    timediff_tree->Branch("Energy_Ch", &current_energy_ch, "Energy_Ch/s");
    if (my_run_setup->has_energy_cal || proc_setup->calibrate_detectors)
        timediff_tree->Branch("Energy_Cal", &current_energy_cal, "Energy_Cal/D");
    if (my_run_setup->has_energy_short)
        timediff_tree->Branch("Energy_Short", &current_energy_short, "Energy_Short/S");
    timediff_tree->Branch("Flags", &current_flags, "Flags/i");
    timediff_tree->Branch("Time", &current_time, "Time/l");
    timediff_tree->Branch("Pileup", &current_pileup, "Pileup/O");
    timediff_tree->Branch("Saturation", &current_saturation, "Saturation/O");
    timediff_tree->Branch("Veto", &veto, "Veto/O");
    if (my_run_setup->has_waveforms) {
        timediff_tree->Branch("Wf_Code", &current_wf_code, "Wf_Code/C");
        timediff_tree->Branch("Waveforms", &current_wfs);
    }

    // Build output branches using numeric board/channel names so they remain
    // unique even when detector labels are reused.
    for (const auto& det : detector_channels) {
        string suffix = to_string(det.first) + "_" + to_string(det.second);
        string name = "TimeDiffBefore_" + suffix;
        timediff_tree->Branch(name.c_str(), &time_diff_before[det], (name + "/l").c_str());
        name = "TimeDiffAfter_" + suffix;
        timediff_tree->Branch(name.c_str(), &time_diff_after[det], (name + "/l").c_str());
        name = "EnergyDepBefore_" + suffix;
        timediff_tree->Branch(name.c_str(), &energy_dep_before[det], (name + "/s").c_str());
        name = "EnergyDepAfter_" + suffix;
        timediff_tree->Branch(name.c_str(), &energy_dep_after[det], (name + "/s").c_str());
    }
    cout << "Processing " << n_entries << " entries to create time difference tree with veto" << endl;
    int percent = 1;
    for (Long64_t i = 0; i < n_entries; ++i) {
        if (verbose > 0) {
            if (verbose > 1 && i < 100) {
                cout << "Processing entry " << i << " of " << n_entries << endl;
            }
            if (i == percent * int(n_entries / 100)) {
                cout << "\rProgress: " << (100.0 * i / n_entries) << "%" << flush;
                percent++;
            }
        }
        data_tree->GetEntry(index_list[i]);

        bool matches_main = false;
        for (const auto& det : main_detectors) {
            if (board == det.board && channel == det.channel) {
                matches_main = true;
                break;
            }
        }
        if (!matches_main)
            continue;

        current_board = board;
        current_channel = channel;
        current_energy_ch = energy_ch;
        if (my_run_setup->has_energy_cal || proc_setup->calibrate_detectors)
            current_energy_cal = energy_cal;
        if (my_run_setup->has_energy_short)
            current_energy_short = energy_short;
        current_flags = flags;
        current_time = time;
        current_pileup = pileup;
        current_saturation = saturation;
        if (my_run_setup->has_waveforms) {
            current_wf_code = wf_code;
            *current_wfs = *wfs;
        }

        // A main event is rejected when a neighbouring veto event satisfies
        // both its ADC threshold and configured time window.
        veto = false;
        for (const auto& det : detector_channels) {
            time_diff_before[det] = kMaxULong64;
            energy_dep_before[det] = 0;
            for (Long64_t j = i - 1; j >= 0; --j) {
                data_tree->GetEntry(index_list[j]);
                if (board == det.first && channel == det.second && energy_ch >= veto_thresholds[det]) {
                    time_diff_before[det] = current_time - time;
                    energy_dep_before[det] = energy_ch;
                    break;
                }
            }
            time_diff_after[det] = kMaxULong64;
            energy_dep_after[det] = 0;
            for (Long64_t j = i + 1; j < n_entries; ++j) {
                data_tree->GetEntry(index_list[j]);
                if (board == det.first && channel == det.second && energy_ch >= veto_thresholds[det]) {
                    time_diff_after[det] = time - current_time;
                    energy_dep_after[det] = energy_ch;
                    break;
                }
            }

            auto threshold = veto_thresholds.find(det);
            if (threshold != veto_thresholds.end() &&
                ((energy_dep_before[det] >= threshold->second &&
                  time_diff_before[det] <= veto_time_threshold) ||
                 (energy_dep_after[det] >= threshold->second &&
                  time_diff_after[det] <= veto_time_threshold))) {
                veto = true;
            }
        }
        timediff_tree->Fill();
        
    }

    delete wfs;
    delete current_wfs;
    return 0;
}