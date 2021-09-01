///////////////////////////////////////////////
// README

// Jonas Norberg

// Inspired by the following document: (1980) Euclidean Distance Mapping, PER-ERIK DANIELSSON, Department of Electrical Engineering, Linkoping University

// how to use?

///////////////////////////////////////////////
// INTERFACE

#ifndef SDFER_H
#define SDFER_H

#include <cstdint> // for uint8_t

// takes a multi-channel image and changes it to a 1-channel mask with 0/1 per pixel
void sdfer_masker_extract_mask_in_place(uint8_t* data_inout, int w, int h, int channels, int extractChannel, int threshold, bool invert);

// gets the size needed for scratchpad 
int sdfer_need_scratchpad_bytes(int w, int h);

// takes a 1-channel mask and replaces it with a sdf-image
void sdfer_process_in_place(uint8_t* data_inout, int w, int h, int scale, int spread, int& outW, int& outH, uint8_t* scratchpad, bool verbose = true);

#endif // SDFER_H
