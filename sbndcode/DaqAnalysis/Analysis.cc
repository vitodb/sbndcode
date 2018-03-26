//some standard C++ includes
#include <iostream>
#include <stdlib.h>
#include <string>
#include <vector>
#include <numeric>
#include <getopt.h>
#include <float.h>

//some ROOT includes
#include "TInterpreter.h"
#include "TROOT.h"
#include "TH1F.h"
#include "TTree.h"
#include "TFile.h"
#include "TStyle.h"
#include "TSystem.h"
#include "TGraph.h"
#include "TFFTReal.h"

// art includes
#include "canvas/Utilities/InputTag.h"
#include "canvas/Persistency/Common/FindMany.h"
#include "canvas/Persistency/Common/FindOne.h"
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h" 
#include "art/Framework/Principal/SubRun.h" 
#include "fhiclcpp/ParameterSet.h" 
#include "messagefacility/MessageLogger/MessageLogger.h"  
#include "art/Framework/Services/Optional/TFileService.h"
#include "lardataobj/RawData/RawDigit.h"

#include "Analysis.h"

#include "ChannelData.hh"
#include "FFT.hh"
#include "Noise.hh"
#include "PeakFinder.hh"
#include "Redis.hh"

using namespace daqAnalysis;

SimpleDaqAnalysis::SimpleDaqAnalysis(fhicl::ParameterSet const & p) :
  art::EDAnalyzer{p},
  _config(p),
  _per_channel_data(_config.n_channels),
  _noise_samples(_config.n_channels)
{
  _event_ind = 0;
  art::ServiceHandle<art::TFileService> fs;

  // set up tree and the channel data branch for output
  _output = fs->make<TTree>("channel_data", "channel_data");
  _output->Branch("channel_data", &_per_channel_data);

  // subclasses to do FFT's and send stuff to Redis
  _fft_manager = (_config.static_input_size > 0) ? FFTManager(_config.static_input_size) : FFTManager();
  if (_config.redis) {
    _redis_manager = new Redis(); 
  }
}

SimpleDaqAnalysis::AnalysisConfig::AnalysisConfig(const fhicl::ParameterSet &param) {
  // set up config

  // conversion of frame number to time (currently unused)
  frame_to_dt = param.get<double>("frame_to_dt", 1.6e-3 /* units of seconds */);
  // whether to print stuff
  verbose = param.get<bool>("verbose", false);
  // number of events to take in before exiting
  // will never exit if set to negative
  // Also--currently does nothing.
  n_events = param.get<unsigned>("n_events", -1);

  // configuring analysis code:

  // thresholds for peak finding
  threshold_hi = param.get<double>("threshold_hi", 100);
  threshold_lo = param.get<double>("threshold_lo", -1);

  // determine method to get noise sample
  // 0 == use first `n_baseline_samples`
  // 1 == use peakfinding
  noise_range_sampling = param.get<unsigned>("noise_range_sampling",0);
 
  // only used if noise_range_sampling == 0
  // number of samples in noise sample
  n_noise_samples = param.get<unsigned>("n_noise_samples", 20);

  // number of samples to average in each direction for peak finding
  n_smoothing_samples = param.get<unsigned>("n_smoothing_samples", 1);

  // Number of input adc counts per waveform. Set to negative if unknown.
  // Setting to some positive number will speed up FFT's.
  static_input_size = param.get<int>("static_input_size", -1);
  // whether to send stuff to redis
  redis = param.get<bool>("redis", false);

  // number of input channels
  // TODO: how to detect this?
  n_channels = param.get<unsigned>("n_channels", 16 /* currently only the first 16 channels have data */);

  // name of producer of raw::RawDigits
  std::string producer = param.get<std::string>("producer_name");
  daq_tag = art::InputTag(producer, ""); 
}

// Calculate the mode to find a baseline of the passed in waveform.
// Mode finding algorithm from: http://erikdemaine.org/papers/NetworkStats_ESA2002/paper.pdf (Algorithm FREQUENT)
short SimpleDaqAnalysis::Mode(const std::vector<short> &adcs) {
  // 10 counters seem good
  std::array<unsigned, 10> counters {}; // zero-initialize
  std::array<short, 10> modes {};

  for (auto val: adcs) {
    int home = -1;
    // look for a home for the val
    for (int i = 0; i < (int)modes.size(); i ++) {
      if (modes[i] == val) {
        home = i; 
        break;
      }
    }
    // invade a home if you don't have one
    if (home < 0) {
      for (int i = 0; i < (int)modes.size(); i++) {
        if (counters[i] == 0) {
          home = i;
          modes[i] = val;
          break;
        }
      }
    }
    // incl if home
    if (home >= 0) counters[home] ++;
    // decl if no home
    else {
      for (int i = 0; i < (int)counters.size(); i++) {
        counters[i] = (counters[i]==0) ? 0 : counters[i] - 1;
      }
    }
  }
  // highest counters has the mode
  unsigned max_counters = 0;
  short ret = 0;
  for (int i = 0; i < (int)counters.size(); i++) {
    /*if (_config.verbose) {
      std::cout << "Counter: " << counters[i] << std::endl;
      std::cout << "Mode: " << modes[i] << std::endl;
    }*/
    if (counters[i] > max_counters) {
      max_counters = counters[i];
      ret = modes[i];
    }
  }
  return ret;
}

void SimpleDaqAnalysis::analyze(art::Event const & event) {
  //if (_config.n_events >= 0 && _event_ind >= (unsigned)_config.n_events) return false;

  _event_ind ++;

  // clear out containers from last iter
  for (unsigned i = 0; i < _config.n_channels; i++) {
    _per_channel_data[i].waveform.clear();
    _per_channel_data[i].fft_real.clear();
    _per_channel_data[i].fft_imag.clear();
    _per_channel_data[i].peaks.clear();
  }
  _noise_samples.clear();

  auto const& raw_digits_handle = event.getValidHandle<std::vector<raw::RawDigit>>(_config.daq_tag);
  
  // calculate per channel stuff
  for (auto const& digits: *raw_digits_handle) {
    ProcessChannel(digits);
  }

  // now calculate stuff that depends on stuff between channels
  for (unsigned i = 0; i < _config.n_channels; i++) {
    unsigned last_channel_ind = i == 0 ? _config.n_channels - 1 : i - 1;
    unsigned next_channel_ind = i == _config.n_channels - 1 ? 0 : i + 1;

    // cross channel correlations
    _per_channel_data[i].last_channel_correlation = _noise_samples[i].Correlation(
        _per_channel_data[i].waveform, _noise_samples[last_channel_ind],  _per_channel_data[last_channel_ind].waveform);
    _per_channel_data[i].next_channel_correlation = _noise_samples[i].Correlation(
        _per_channel_data[i].waveform, _noise_samples[next_channel_ind],  _per_channel_data[next_channel_ind].waveform);

    // cross channel correlations
    _per_channel_data[i].last_channel_sum_rms = _noise_samples[i].SumRMS(
        _per_channel_data[i].waveform, _noise_samples[last_channel_ind],  _per_channel_data[last_channel_ind].waveform);
    _per_channel_data[i].next_channel_sum_rms = _noise_samples[i].SumRMS(
        _per_channel_data[i].waveform, _noise_samples[next_channel_ind],  _per_channel_data[next_channel_ind].waveform);
  }

  ReportEvent(event);
}

void SimpleDaqAnalysis::ReportEvent(art::Event const &art_event) {
  // don't need this for now
  (void) art_event;
  // Fill the output
  _output->Fill();

  // print stuff out
  if (_config.verbose) {
    std::cout << "EVENT NUMBER: " << _event_ind << std::endl;
    for (auto &channel_data: _per_channel_data) {
      std::cout << channel_data.JsonifyPretty();
    }
  }

  // Send stuff to Redis
  if (_config.redis) {
    Redis::EventDef event;
    event.per_channel_data = &_per_channel_data;
    _redis_manager->Send(event);
  }

}

void SimpleDaqAnalysis::ProcessChannel(const raw::RawDigit &digits) {

  /*
  auto fragment_header = fragment.header(); 
  
  //_output_file.cd();
  
  if (_config.verbose) {
    std::cout << "EVENT NUMBER: "  << _header_data.event_number << std::endl;
    std::cout << "FRAME NUMBER: "  << _header_data.frame_number << std::endl;
    std::cout << "CHECKSUM: "  << _header_data.checksum << std::endl;
    std::cout << "ADC WORD COUNT: "  << _header_data.adc_word_count << std::endl;
    std::cout << "TRIG FRAME NUMBER: "  << _header_data.trig_frame_number << std::endl;
  }*/

  auto channel = digits.Channel();
  if (channel < _config.n_channels) {
    // re-allocate FFT if necessary
    if (_fft_manager.InputSize() != digits.NADC()) {
      _fft_manager.Set(digits.NADC());
    }
   
    _per_channel_data[channel].channel_no = channel;

    double max = 0;
    double min = DBL_MAX;
    for (unsigned i = 0; i < digits.NADC(); i ++) {
      double adc = (double) digits.ADCs()[i];
      if (adc > max) max = adc;
      if (adc < min) min = adc;
    
      // fill up waveform
      _per_channel_data[channel].waveform.push_back(adc);
      // fill up fftw array
      double *input = _fft_manager.InputAt(i);
      *input = adc;
    }
    // use mode to calculate baseline
    _per_channel_data[channel].baseline = (double) Mode(digits.ADCs());

    _per_channel_data[channel].max = max;
    _per_channel_data[channel].min = min;
      
    // calculate FFTs
    _fft_manager.Execute();
    int adc_fft_size = _fft_manager.OutputSize();
    for (int i = 0; i < adc_fft_size; i++) {
      _per_channel_data[channel].fft_real.push_back(_fft_manager.ReOutputAt(i));
      _per_channel_data[channel].fft_imag.push_back(_fft_manager.ImOutputAt(i));
    } 

    // get Peaks
    PeakFinder peaks(_per_channel_data[channel].waveform, _per_channel_data[channel].baseline, 
         _config.n_smoothing_samples, _config.threshold_hi, _config.threshold_lo);
    _per_channel_data[channel].peaks.assign(peaks.Peaks()->begin(), peaks.Peaks()->end());

    // get noise samples
    if (_config.noise_range_sampling == 0) {
      // use first n_noise_samples
      _noise_samples[channel] = NoiseSample( { { 0, _config.n_noise_samples -1 } }, _per_channel_data[channel].baseline);
    }
    else {
      // or use peak finding
      _noise_samples[channel] = NoiseSample(_per_channel_data[channel].peaks, _per_channel_data[channel].baseline, digits.NADC()); 
    }

    _per_channel_data[channel].rms = _noise_samples[channel].RMS(_per_channel_data[channel].waveform);
    _per_channel_data[channel].noise_ranges = *_noise_samples[channel].Ranges();
  }

}