/**
 * @file    camerad_commands.h
 * @brief   the doit() function listens for these commands.
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

const std::string CAMERAD_BIDIRECTION  = "bidirection"; ///< bidirectional readout direction
const std::string CAMERAD_BOI          = "boi";       ///< band of interest
const std::string CAMERAD_COMPRESSION  = "compress";  ///< FITS compression type
const std::string CAMERAD_POWER        = "power";     ///< control power to detector
const std::string CAMERAD_SAVEUNP      = "saveunp";   ///< save unprocessed images

const std::vector<std::string> CAMERAD_SYNTAX = {
                                                  CAMERAD_BIDIRECTION+" [ ? | yes | no ]",
                                                  CAMERAD_BOI+" [ ? | <skip> <rows> ]",
                                                  CAMERAD_COMPRESSION+" [ ? | none | rice | gzip | plio ]",
                                                  CAMERAD_POWER+" [ ? | on | off ]",
                                                  CAMERAD_SAVEUNP+" [ ? | true | false ]"
                                                };
