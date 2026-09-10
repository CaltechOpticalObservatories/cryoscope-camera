/**
 * @file    emulator-archon.h
 * @brief   header for interface to Archon emulator
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#pragma once

#include <atomic>
#include <chrono>
#include <numeric>
#include <functional>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <fenv.h>

#include "utilities.h"
#include "common.h"
#include "config.h"
#include "logentry.h"
#include "network.h"

#include "generic.h"
#include "nirc2.h"
#include "frame_source.h"

namespace Archon {

  // Archon hardware-based constants.
  // These shouldn't change unless there is a significant hardware change.
  //
  constexpr const int NBUFS = 3;        //!< total number of frame buffers  //TODO rename to maxnbufs?
  constexpr const int NMODS = 12;       //!< number of modules per controller
  constexpr const int NADCHAN = 4;      //!< number of channels per ADC module
  constexpr const int BLOCKLEN = 1024;  //!< Archon block size in bytes

  class Interface {
    private:
      std::string instr;
      std::atomic<bool> abort{false};
      std::atomic<bool> exposing{false};

      // Declare a map to contain image types for each recognized instrument.
      //
      static std::unordered_map<std::string, std::function<std::unique_ptr<ImageInfoBase>()>> ImageInfoMap;

      static void initialize_image_info_map();

    public:

      std::unique_ptr<ImageInfoBase> image;   ///!< smart pointer to the base class
      std::unique_ptr<Emulator::FrameSource> frame_source;

      Interface( const std::string &instr );

      // Class Objects
      //
      Config config;

      std::string systemfile;

      unsigned long long init_time;
      bool poweron;                //!< is the power on?
      bool bigbuf;                 //!< is BIGBUF==1 in ACF file?
      std::string exposeparam;     //!< param name to trigger exposure when set =1

      std::string active_mode;    //!< currently active mode detected from ACF parameters

      /**
       * @var     struct frame_data_t frame
       * @details structure to contain Archon results from "FRAME" command
       */
      struct frame_data_t {
        int      index;                       // index of newest buffer data
        int      frame;                       // index of newest buffer data
        std::string timer;                    // current hex 64 bit internal timer
        int      rbuf;                        // current buffer locked for reading
        int      wbuf;                        // current buffer locked for writing
        std::vector<int>      bufsample;      // sample mode 0=16 bit, 1=32 bit
        std::vector<int>      bufcomplete;    // buffer complete, 1=ready to read
        std::vector<int>      bufmode;        // buffer mode: 0=top 1=bottom 2=split
        std::vector<uint64_t> bufbase;        // buffer base address for fetching
        std::vector<int>      bufframen;      // buffer frame number
        std::vector<int>      bufwidth;       // buffer width
        std::vector<int>      bufheight;      // buffer height
        std::vector<int>      bufpixels;      // buffer pixel progress
        std::vector<int>      buflines;       // buffer line progress
        std::vector<int>      bufrawblocks;   // buffer raw blocks per line
        std::vector<int>      bufrawlines;    // buffer raw lines
        std::vector<int>      bufrawoffset;   // buffer raw offset
        std::vector<uint64_t> buftimestamp;   // buffer hex 64 bit timestamp
        std::vector<uint64_t> bufretimestamp; // buf trigger rising edge time stamp
        std::vector<uint64_t> buffetimestamp; // buf trigger falling edge time stamp
        std::vector<std::vector<char>> bufdata;  // pixel data per buffer
      } frame;

      // Functions
      //
      long configure_controller();           //!< get configuration parameters from .cfg file
      long system_report(std::string buf, std::string &retstring);         
      long status_report(std::string &retstring);
      long timer_report(std::string &retstring);
      unsigned long  get_timer();
      long frame_report(std::string &retstring);
      long fetch_data( const std::string &ref, const std::string &cmd, Network::TcpSocket &sock );      //!< 
      long wconfig(std::string buf);         //!< 
      long rconfig(std::string buf, std::string &retstring);         
      long write_parameter(std::string buf);
      static void dothread_expose( Archon::Interface &iface, int numexpose );

      /** @var      vector modtype
       *  @details  stores the type of each module from the SYSTEM command
       */
      std::vector<int> modtype;

      /** @var      vector modversion
       *  @details  stores the version of each module from the SYSTEM command
       */
      std::vector<std::string> modversion;

      std::string backplaneversion;

      /** @var      int lastframe
       *  @details  the last (I.E. previous) frame number acquired
       */
      int lastframe;

      /**
       * rawinfo_t is a struct which contains variables specific to raw data functions
       */
      struct rawinfo_t {
        int adchan;          // selected A/D channels
        int rawsamples;      // number of raw samples per line
        int rawlines;        // number of raw lines
        int iteration;       // iteration number
        int iterations;      // number of iterations
      } rawinfo;

      /**
       * config_line_t is a struct for the configfile key=value map, used to
       * store the configuration line and its associated line number.
       */
      typedef struct {
        std::string line;      // the line number, used for updating Archon
        std::string key;       // the part before the '='
        std::string value;     // the part after the '='
      } config_line_t;

      /**
       * param_line_t is a struct for the PARAMETER name key=value map, used to
       * store parameters where the format is PARAMETERn=parametername=value
       */
      typedef struct {
        std::string key;       // the PARAMETERn part
        std::string name;      // the parametername part
        std::string value;     // the value part
        std::string line;      // the line number
      } param_line_t;

      typedef std::map<std::string, config_line_t>  cfg_map_t;
      typedef std::map<std::string, param_line_t>   param_map_t;

      cfg_map_t   configmap;
      param_map_t parammap;

      /**
       * generic key=value STL map for Archon commands
       */
      typedef std::map<std::string, std::string> map_t;

      /**
       * \var     map_t systemmap
       * \details key=value map for Archon SYSTEM command
       */
      map_t systemmap;

      /**
       * \var     map_t statusmap
       * \details key=value map for Archon STATUS command
       */
      map_t statusmap;

  };

}
