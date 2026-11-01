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



vector<string> GetBinFileList(const string& run_path);

int ProcessRun(ProcSetup* setup, string run);

int ReadHeader(run_setup* my_run_setup);

#endif