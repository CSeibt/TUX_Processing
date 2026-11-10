#include <stdio.h>
#include <dirent.h>
#include <algorithm>
#include <ctime>
#include <cmath>
#include <vector>
#include <string>
#include <iostream>
#include "TFile.h"
#include "TF1.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TTree.h"
#include "TRandom3.h"
#include "TCanvas.h"
#include "TLegend.h"

using namespace std;

void get_hists_veto(){
    TFile* file1 = new TFile("../output_files/TU2_results/run390_processed_0.root", "READ");
    TTree* tree1 = (TTree*)file1->Get("Data_Timediff");

    int channel = 0;

    TH1D* hist_all = new TH1D("energy_all", "; Energy [adc]; Counts", 16000, 0, 16000);
    tree1->Draw("Energy_Ch>>energy_all", Form("Channel==%d", channel), "goff");
    hist_all->SetStats(0);
    hist_all->SetLineColor(kBlack);

    TH1D* hist_veto = new TH1D("energy_veto", "; Energy [adc]; Counts", 16000, 0, 16000);
    tree1->Draw("Energy_Ch>>energy_veto", Form("Channel==%d && Veto==1", channel), "goff");
    hist_veto->SetStats(0);
    hist_veto->SetLineColor(kRed);

    TH1D* hist_anti_veto = new TH1D("energy_anti_veto", "; Energy [adc]; Counts", 16000, 0, 16000);
    tree1->Draw("Energy_Ch>>energy_anti_veto", Form("Channel==%d && Veto==0", channel), "goff");
    hist_anti_veto->SetStats(0);
    hist_anti_veto->SetLineColor(kBlue);
    

    TCanvas* canvas = new TCanvas("canvas", "canvas", 1200, 600);
    hist_all->Draw("hist");
    hist_veto->Draw("hist same");
    hist_anti_veto->Draw("hist same");

    TLegend* legend = new TLegend(0.7, 0.7, 0.9, 0.9);
    legend->AddEntry(hist_all, "All Events", "l");
    legend->AddEntry(hist_veto, "Veto Events", "l");
    legend->AddEntry(hist_anti_veto, "Anti-Veto Events", "l");
    legend->Draw();
}

