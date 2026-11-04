#ifndef PROCESS_RUN_H
#define PROCESS_RUN_H 1


#include "ProcSetup.h"
#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <dirent.h>
#include "TMath.h"
#include "TFile.h"
#include "TTree.h"
#include "CreateProcessedFile.h"

using namespace std;


/**
 * Find the binary files belonging to a run and return them in sorted order.
 *
 * @param run_path Directory containing the run's raw files.
 * @param run Run identifier used to select matching filenames.
 */
vector<string> GetBinFileList(const string& run_path, const string& run);

/**
 * Process every binary file belonging to one run.
 *
 * The first file is also used to determine which optional fields are present
 * in all records of the run.
 */
int ProcessRun(ProcSetup* setup, string run);

/**
 * Read the binary header and record the input schema in `my_run_setup`.
 *
 * @return Zero on success, or one when the first binary file cannot be opened.
 */
int ReadHeader(run_setup* my_run_setup);

#endif