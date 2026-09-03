/**
 * @file    generic.cpp
 * @brief   instrument-specific class definitions for generic instruments
 * @details This file defines functions for any generic instrument
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 * Any functions defined here must have (generic) function definitions in archon.h.
 * This file contains the specific code for those function definitions which are
 * unique to the particular instrument.
 *
 * More than likely, if a function is here then it is not supported, otherwise it
 * would be defined for a particular instrument. This file is here so that the main
 * daemon will compile.
 *
 */

#include <string>
#include <sstream>
#include <vector>

#include "archon.h"
#include "common.h"
#include "utilities.h"
#include "logentry.h"

namespace Archon {

    /**************** Archon::Interface::band_of_interest *******************/
    /**
     * @brief      define a band of interest
     * @param[in]  args
     * @param[out] retstring
     * @return     ERROR | NO_ERROR
     *
     */
    long Interface::band_of_interest(std::string args, std::string &retstring) {
      std::string function = "Archon::Interface::band_of_interest";
      std::stringstream message;
      return this->do_boi( args, retstring );
    }
    /**************** Archon::Interface::band_of_interest *******************/


    /**************** Archon::Interface::region_of_interest *****************/
    /**
     * @fn         region_of_interest
     * @brief      define a region of interest
     * @param[in]  args
     * @param[out] retstring
     * @return
     *
     */
    long Interface::region_of_interest(std::string args, std::string &retstring) {
        std::string function = "Archon::Interface::region_of_interest";
        std::stringstream message;
        this->camera.log_error(function, "ROI not supported");
        return (ERROR);
    }
    /**************** Archon::Interface::region_of_interest *****************/


    /***** Archon::Interface::power *****************************************/
    /**
     * @brief      wrapper for Archon::Interface::do_power()
     * @param[in]  state_in  requested power state
     * @param[out] restring  return string holds power state
     * @return     ERROR or NO_ERROR
     *
     */
    long Interface::power( std::string state_in, std::string &retstring ) {
        // use Archon::Interface::do_power() to set/get the power
        //
        return( this->do_power( state_in, retstring ) );
    }
    /***** Archon::Interface::power *****************************************/
}
