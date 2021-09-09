///////////////////////////////////////////////
// README

// Jonas Norberg

// Inspired by the following document: (1980) Euclidean Distance Mapping, PER-ERIK DANIELSSON, Department of Electrical Engineering, Linkoping University

///////////////////////////////////////////////
// INTERFACE

#ifndef SDFER_H
#define SDFER_H

#include <cstdint> // for uint8_t

// takes a multi-channel image and changes it to a 1-channel mask with 0/1 per pixel
void sdfer_extract_mask_in_place(uint8_t* data_inout, int w, int h, int channels, int extractChannel, int threshold, bool invert);

// copies the src to the destination (dest has to be pre-allocated to fit the padded image)
void sdfer_pad_mask(uint8_t* dst_img, const uint8_t* src_img, int w, int h, int pad);

// takes a 1-channel mask and replaces it with a sdf-image
void sdfer_process_in_place(uint8_t* data_inout, int w, int h, int scale, int spread, int& w_out, int& h_out, bool verbose = true);

// takes a image and shrinks to fit (in-place)
void sdfer_crop_image_in_place(uint8_t* src_img, int& w_inout, int &h_inout);

#endif // SDFER_H
