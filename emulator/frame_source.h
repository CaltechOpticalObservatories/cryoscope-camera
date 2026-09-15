/**
 * @file    frame_source.h
 * @brief   defines source of emulated data
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <fitsio.h>

#include "utilities.h"

namespace Emulator {

  // Abstract interface for filling frame buffers with pixel data
  //
  class FrameSource {
    public:
      virtual ~FrameSource() = default;

      // Number of frames the source can supply before it must repeat, or 0 if
      // it is unlimited. Sources that cycle or generate data are unlimited.
      //
      virtual size_t available_frames() const { return 0; }

      /**
       * @brief         Fill buffer with one frame of pixel data (uint16 pixels)
       * @param buffer  destination buffer, must be at least width*height*2 bytes
       * @param width   frame width in pixels
       * @param height  frame height in pixels
       * @return true on success
       *
       */
      virtual bool fill_frame(char* buffer, int width, int height) = 0;
  };


  // Generate synthetic frames with bias + Gaussian noise
  // Mode-aware: RXR generates signal/reset halves with correlated noise
  //
  class SyntheticSource : public FrameSource {
    private:
      std::mt19937 rng{std::random_device{}()};
      double noise_stddev = 50.0;
      std::string* active_mode;  // non-owning pointer to emulator's active mode
      int taplines = 0;

    public:
      /**
       * @param mode  pointer to the active mode string (owned by Interface)
       * @param taps  number of taplines for RXR half-width calculation
       */
      SyntheticSource(std::string* mode = nullptr, int taps = 0)
        : active_mode(mode), taplines(taps) {}

      void set_taplines(int taps) { taplines = taps; }

      bool fill_frame(char* buffer, int width, int height) override {
        auto* pixels = reinterpret_cast<uint16_t*>(buffer);
        std::normal_distribution<double> noise(0.0, noise_stddev);
        size_t npixels = static_cast<size_t>(width) * height;

        // Check if active mode contains "RXR" (covers VideoRXR, RXRV, etc)
        bool rxr_mode = active_mode &&
                         active_mode->find("RXR") != std::string::npos;

        if (rxr_mode && taplines > 0) {
          // RXR: each tap has signal pixels then reset pixels
          // Signal half: higher bias (10000), reset half: lower bias (5000)
          // Correlated noise: same noise added to both halves so subtraction cancels it
          int pixels_per_tap = width / taplines;
          int half = pixels_per_tap / 2;

          for (int row = 0; row < height; row++) {
            for (int tap = 0; tap < taplines; tap++) {
              int tap_offset = row * width + tap * pixels_per_tap;
              for (int col = 0; col < pixels_per_tap; col++) {
                double common_noise = noise(rng);
                double readout_noise = noise(rng) * 0.3;  // uncorrelated readout noise
                bool is_signal = (col < half);
                double base = is_signal ? 10000.0 : 5000.0;
                double val = base + common_noise + readout_noise;
                pixels[tap_offset + col] = static_cast<uint16_t>(std::clamp(val, 0.0, 65535.0));
              }
            }
          }
        }
        else {
          // Default: flat bias + noise + horizontal gradient
          for (size_t i = 0; i < npixels; i++) {
            int col = i % width;
            double val = 5000.0 + noise(rng) + static_cast<double>(col) * 100.0 / width;
            pixels[i] = static_cast<uint16_t>(std::clamp(val, 0.0, 65535.0));
          }
        }
        return true;
      }
  };


  // Read FITS files from folder, serve them sequentially, cycling
  // Not mode-aware: FITS data is already in the correct format for its mode
  //
  class FitsFileSource : public FrameSource {
    private:
      std::vector<std::string> files;
      size_t current_index = 0;
      bool limited = false;

      public:
      explicit FitsFileSource(const std::string &datadir) {
        std::string function = "(Emulator::FitsFileSource) ";
        for (const auto &entry : std::filesystem::directory_iterator(datadir)) {
          auto path = entry.path().string();
          if (ends_with(path, ".fits") || ends_with(path, ".fits.gz") ||
              ends_with(path, ".fit")  || ends_with(path, ".fit.gz")) {
            files.push_back(path);
          }
        }
        std::sort(files.begin(), files.end());
        std::cout << get_timestamp() << function << files.size()
                  << " FITS files found in " << datadir << "\n";
      }

      // Serve a fixed set of files, taken as the complete set of frames
      // available. The directory constructor above instead cycles indefinitely.
      //
      explicit FitsFileSource( std::vector<std::string> paths )
          : files( std::move( paths ) ), limited( true ) {
        std::string function = "(Emulator::FitsFileSource) ";
        std::cout << get_timestamp() << function << files.size()
                  << " FITS files\n";
      }

      size_t available_frames() const override { return limited ? files.size() : 0; }

      bool fill_frame(char* buffer, int width, int height) override {
        std::string function = "(Emulator::FitsFileSource::fill_frame) ";
        if (files.empty()) {
          std::cerr << get_timestamp() << function << "ERROR: no FITS files\n";
          return false;
        }

        const auto &path = files[current_index % files.size()];
        current_index++;

        fitsfile* fptr = nullptr;
        int status = 0;

        fits_open_file(&fptr, path.c_str(), READONLY, &status);
        if (status) {
          std::cerr << get_timestamp() << function << "ERROR opening " << path << "\n";
          return false;
        }

        int naxis = 0;
        long naxes[2] = {0, 0};
        fits_get_img_dim(fptr, &naxis, &status);
        fits_get_img_size(fptr, 2, naxes, &status);

        if (status || naxis != 2) {
          std::cerr << get_timestamp() << function << "ERROR: " << path
                    << " is not a 2D image\n";
          fits_close_file(fptr, &status);
          return false;
        }

        if (naxes[0] != width || naxes[1] != height) {
          std::cerr << get_timestamp() << function << "ERROR: " << path
                    << " dimensions " << naxes[0] << "x" << naxes[1]
                    << " don't match expected " << width << "x" << height << "\n";
          fits_close_file(fptr, &status);
          return false;
        }

        long fpixel[2] = {1, 1};
        long nelements = static_cast<long>(width) * height;
        fits_read_pix(fptr, TUSHORT, fpixel, nelements, nullptr, buffer, nullptr, &status);
        fits_close_file(fptr, &status);

        if (status) {
          std::cerr << get_timestamp() << function << "ERROR reading pixels from " << path << "\n";
          return false;
        }

        std::cout << get_timestamp() << function << "loaded " << path << "\n";
        return true;
      }
  };

  // Read headerless frame buffers from disk, serve them sequentially.
  // A raw file holds width*height 16 bit pixels and nothing else, so the frame
  // geometry comes from the caller rather than from the file.
  //
  class RawFileSource : public FrameSource {
    private:
    std::vector<std::string> files;
    size_t current_index = 0;

    public:
    explicit RawFileSource( std::vector<std::string> paths )
        : files( std::move( paths ) ) {
      std::string function = "(Emulator::RawFileSource) ";
      std::cout << get_timestamp() << function << files.size()
                << " raw files\n";
    }

    size_t available_frames() const override { return files.size(); }

    bool fill_frame( char *buffer, int width, int height ) override {
      std::string function = "(Emulator::RawFileSource::fill_frame) ";
      if ( files.empty() ) {
        std::cerr << get_timestamp() << function << "ERROR: no raw files\n";
        return false;
      }

      const auto &path = files[current_index % files.size()];
      current_index++;

      const std::streamsize expected =
          static_cast<std::streamsize>( width ) * height * sizeof( uint16_t );

      std::ifstream file( path, std::ios::binary | std::ios::ate );
      if ( !file ) {
        std::cerr << get_timestamp() << function << "ERROR opening " << path << "\n";
        return false;
      }

      const std::streamsize filesize = file.tellg();
      if ( filesize != expected ) {
        std::cerr << get_timestamp() << function << "ERROR: " << path << " is "
                  << filesize << " bytes, expected " << expected
                  << " for " << width << "x" << height << "\n";
        return false;
      }

      file.seekg( 0 );
      if ( !file.read( buffer, expected ) ) {
        std::cerr << get_timestamp() << function << "ERROR reading pixels from "
                  << path << "\n";
        return false;
      }

      std::cout << get_timestamp() << function << "loaded " << path << "\n";
      return true;
    }
  };

  // Expand a frame source specification into a list of files.
  // Accepts a directory, a single file, or a comma separated list of files.
  // Returns an empty list if nothing matches.
  //
  inline std::vector<std::string> expand_source_spec( const std::string &spec,
                                                      bool ( *accept )( const std::string & ) ) {
    std::string function = "(Emulator::expand_source_spec) ";
    std::vector<std::string> paths;

    if ( std::filesystem::is_directory( spec ) ) {
      for ( const auto &entry : std::filesystem::directory_iterator( spec ) ) {
        auto path = entry.path().string();
        if ( accept( path ) ) paths.push_back( path );
      }
      std::sort( paths.begin(), paths.end() );
      return paths;
    }

    std::stringstream ss( spec );
    std::string item;
    while ( std::getline( ss, item, ',' ) ) {
      if ( item.empty() || !accept( item ) ) continue;
      if ( !std::filesystem::is_regular_file( item ) ) {
        std::cerr << get_timestamp() << function << "ERROR: " << item << " not found\n";
        return {};
      }
      paths.push_back( item );
    }
    return paths;
  }

  inline bool is_fits_name( const std::string &path ) {
    return ends_with( path, ".fits" ) || ends_with( path, ".fits.gz" ) ||
           ends_with( path, ".fit" ) || ends_with( path, ".fit.gz" );
  }

  inline bool is_raw_name( const std::string &path ) {
    return ends_with( path, ".raw" );
  }

  // Create the appropriate FrameSource based on config
  //
  inline std::unique_ptr<FrameSource> make_frame_source(
      const std::string &datadir,
      std::string* active_mode = nullptr,
      int taplines = 0) {
    if (!datadir.empty() && std::filesystem::is_directory(datadir)) {
      return std::make_unique<FitsFileSource>(datadir);
    }
    return std::make_unique<SyntheticSource>(active_mode, taplines);
  }

  // Create a FrameSource from a runtime specification. A spec naming raw files
  // yields a raw source, one naming FITS files yields a FITS source, and an
  // empty spec or "none" yields generated data. Returns nullptr if the spec
  // names files but none of them match a recognised extension.
  //
  inline std::unique_ptr<FrameSource> make_frame_source_from_spec(
      const std::string &spec,
      std::string *active_mode = nullptr,
      int taplines = 0 ) {
    if ( spec.empty() || caseCompareString( spec, "none" ) ) {
      return std::make_unique<SyntheticSource>( active_mode, taplines );
    }

    auto raws = expand_source_spec( spec, is_raw_name );
    if ( !raws.empty() ) {
      return std::make_unique<RawFileSource>( std::move( raws ) );
    }

    auto fits = expand_source_spec( spec, is_fits_name );
    if ( !fits.empty() ) {
      return std::make_unique<FitsFileSource>( std::move( fits ) );
    }

    return nullptr;
  }
}
