////////////////////////////////////////////////////////////////////////
// File:        sbndPDMapAlg.h
// Authors: Laura Paulucci, Franciole Marinho, and Iker de Icaza
//
// Updates: 2020-03, v08_45_00. Iker de Icaza icaza@fnal.gov
//          Added properties to PDS MAP and functions to access these.
//
// This class stores the SBND PDS Map and channel's properties;
// also implements functions to access these.
//
// As of version v08_45_00 the PDS Map has:
// channel: 0 to 503
// pd_type: pmt, barepmt, xarapuca, xarapucaT1, xarapucaT2, arapucaT1, arapucaT2
// pds_box: -12, to 12; skipping 0
// sensible_to: VUV or VIS
// tpc: 0, 1
// xarapuca_pos: top, bottom, null
////////////////////////////////////////////////////////////////////////

#ifndef SBND_OPDETSIM_SBNDPDMAPALG_H
#define SBND_OPDETSIM_SBNDPDMAPALG_H

#include <algorithm>
#include <fstream>
#include <map>
#include <string>

#include "art_root_io/TFileService.h"

#include "json.hpp"

namespace opdet {

  class sbndPDMapAlg {

  public:
    //Default constructor
    sbndPDMapAlg();
    //Default destructor
    ~sbndPDMapAlg();

    nlohmann::json getCollectionWithProperty(std::string property, std::string property_value);
    nlohmann::json getCollectionWithProperty(std::string property, int property_value);
    // template<typename T> nlohmann::json getCollectionWithProperty(std::string property, T property_value);

    // struct Config {};

    //  sbndPDMapAlg(Config const&) {}

    // void setup() {}

    bool isPDType(size_t ch, std::string pdname) const;
    std::string pdType(size_t ch) const;
    size_t size() const;
    auto getChannelEntry(size_t ch) const;

  private:
    nlohmann::json PDmap;
    nlohmann::json subSetPDmap;

  }; // class sbndPDMapAlg

} // namespace

#endif // SBND_OPDETSIM_SBNDPDMAPALG_H
