#include <cmath> // for sqrtf
#include <cstring> // for memset
#include <vector>

#include "sdfer.h"




// fixme remove timing (or make it even more separated)
#define MEASURE_TIME 1

#if MEASURE_TIME
#include <iostream>
#include <chrono>

using namespace std::chrono;

struct timepoint
{
	time_point<steady_clock> p;
};

float get_diff_ms(timepoint t0, timepoint t1)
{
	auto diff = t1.p - t0.p;
	auto diff_us = duration_cast<microseconds>(diff);
	return diff_us.count() / 1000.0f;
}

timepoint get_now()
{
	timepoint t;
	t.p = steady_clock::now();
	return t;
}

#else

// fake timing
struct timepoint{};
timepoint get_now() { return timepoint(); }
float get_diff_ms(timepoint t0, timepoint t1) { return 0.f; }

#endif






// step 1
void sdfer_extract_mask_in_place(uint8_t* data, int w, int h, int channels, int extract_channel, int threshold, bool invert)
{
	int s = w * h;
	int rI = 0;
	int wI = 0;

	int ofs0 = channels * 0 + extract_channel;
	int ofs1 = channels * 1 + extract_channel;
	int ofs2 = channels * 2 + extract_channel;
	int ofs3 = channels * 3 + extract_channel;

	int channels4 = channels * 4;

	uint8_t below = 0;
	uint8_t above = 1;
	if (invert)
	{
		below = 1;
		above = 0;
	}

	// 4-wide
	// should be able to be vectorized
	while (wI < s - 4)
	{
		uint8_t v0 = data[rI + ofs0];
		uint8_t v1 = data[rI + ofs1];
		uint8_t v2 = data[rI + ofs2];
		uint8_t v3 = data[rI + ofs3];

		v0 = (v0 > threshold) ? above : below;
		v1 = (v1 > threshold) ? above : below;
		v2 = (v2 > threshold) ? above : below;
		v3 = (v3 > threshold) ? above : below;

		data[wI + 0] = v0;
		data[wI + 1] = v1;
		data[wI + 2] = v2;
		data[wI + 3] = v3;

		rI += channels4;
		wI += 4;
	}

	// last pixels
	while (wI < s)
	{
		uint8_t v = data[rI + extract_channel];
		v = (v > threshold) ? above : below;
		data[wI] = v;

		rI += channels;
		++wI;
	}
}




// only single channel mask-image
// memcpy is safe since writing to new memory
void sdfer_pad_mask(uint8_t* dst_img, const uint8_t* src_img, int w, int h, int pad)
{
	int pw = w + pad * 2;
	int ph = h + pad * 2;

	// top band
	memset(dst_img, 0, pw*pad);

	int last_h = h - 1;

	// middle
	for (int y = 0; y < h; ++y)
	{
		const uint8_t* src_row = src_img + y * w;
		uint8_t* dst_row = dst_img + (pad + y) * pw;

		if (y == 0)
		{
			// left (for other scanline this was included in right-side from last scanline)
			memset(dst_row, 0, pad);
		}

		// mid
		memcpy(dst_row + pad, src_row, w);

		// right
		if (y < last_h)
		{
			// single call includes next scanline
			memset(dst_row + pad + w, 0, pad * 2);
		}
		else
		{
			// last row only do my scanline
			memset(dst_row + pad + w, 0, pad);
		}
	}

	// bottom band
	memset(dst_img + (ph - pad) * pw, 0, pw*pad);
}



// adjust w and h
void sdfer_crop_image_in_place(uint8_t* src_img, int& w_inout, int &h_inout)
{
	int oW = w_inout;
	int oH = h_inout;

	// 1 find top-left-bottom-right (fixme can optimize)
	int x0 = oW;
	int x1 = 0;
	int y0 = oH;
	int y1 = 0;
	for (int y = 0; y < oH; ++y)
	{
		uint8_t* row = src_img + y * oW;
		for (int x = 0; x < oW; ++x)
		{
			if (row[x])
			{
				if (x < x0) x0 = x;
				if (x > x1) x1 = x;

				if (y < y0) y0 = y;
				if (y > y1) y1 = y;
			}
		}
	}

	// make range valid
	x1 += 1;
	y1 += 1;

	// new w,h
	int nW = x1 - x0;
	int nH = y1 - y0;

	if (nW == oW && nH == oH)
	{
		// nothing to crop
		return;
	}

	// 2. copy (memmove)
	for (int y = 0; y < nH; ++y)
	{
		uint8_t* dst_row = src_img + y * nW;
		uint8_t* src_row = src_img + (y0 + y) * oW + x0;
		memmove(dst_row, src_row, nW);
	}

	// 3. adjust w, h
	w_inout = nW;
	h_inout = nH;
}






const int16_t k_far = 32767;

static inline int sdfer_get_sqr_mag(int x, int y)
{
	return x * x + y * y;
}

struct sdfer_cell
{
	int16_t x; // distance from the closest edge
	int16_t y;

	void set( uint16_t v )
	{
		x = v;
		y = v;
	}

	int get_sqr_mag() const
	{
		return ::sdfer_get_sqr_mag(x, y);
	}
};


// set cells to mark where the edges are
static void sdfer_img_to_cells(sdfer_cell* cells, const uint8_t* img, int w, int h)
{
	// set all to very far
	int s = w * h;
	for (int i = 0; i < s; ++i)
	{
		cells[i].set(k_far);
	}

	// top row
	int left = img[0];
	for (int x = 1; x < w; ++x)
	{
		int center = img[x];

		if (left != center)
		{
			cells[x].set(0);
			cells[x-1].set(0);
		}

		left = center;
	}

	// rest of rows
	for (int y = 1; y < h; ++y)
	{
		// first col only check up
		int ind = y * w;
		int ind_up = ind-w;
		int center = img[ind];
		if (img[ind_up] != center)
		{
			cells[ind_up].set(0);
			cells[ind].set(0);
		}

		left = center;

		// rest check both
		for (int x = 1; x < w; ++x)
		{
			++ind;
			++ind_up;

			int center = img[ind];
			int up = img[ind_up];

			if (left != center)
			{
				cells[ind].set(0);
				cells[ind-1].set(0);
			}

			if (up != center)
			{
				cells[ind].set(0);
				cells[ind_up].set(0);
			}

			left = center;
		}
	}
}


// returns the new square mag
static int merge_cell(sdfer_cell& dest, int dst_sqr_mag, const sdfer_cell& src, int dx, int dy)
{
	int x = src.x + dx;
	int y = src.y + dy;
	int sqr_mag = sdfer_get_sqr_mag(x,y);
	if (sqr_mag >= dst_sqr_mag)
		return dst_sqr_mag;

	dest.x = x;
	dest.y = y;
	return sqr_mag;

}

static void sdfer_simple_scanline_l2r(sdfer_cell* row, int w)
{
	for (int x = 1; x < w; ++x)
	{
		merge_cell(row[x], row[x].get_sqr_mag(), row[x-1], 1, 0);
	}
}

static void sdfer_simple_scanline_r2l(sdfer_cell* row, int w)
{
	for (int x = w-2; x >= 0; --x)
	{
		merge_cell(row[x], row[x].get_sqr_mag(), row[x + 1], -1, 0);
	}
}

static void sdfer_fancy_scanline(sdfer_cell* row, const sdfer_cell* prev_row, int w, int dy)
{
	// special case for single-pixel-row
	// the code below looks left and right, can't have that
	if (w < 2)
	{
		merge_cell(row[0], row[0].get_sqr_mag(), prev_row[0], 0, dy);
		return;
	}

	// 1. check up and up-right for the left-most piece
	int sqr = merge_cell(row[0], row[0].get_sqr_mag(), prev_row[0],0,dy);
	merge_cell(row[0], sqr, prev_row[1],-1,dy);

	// 2. loop for most of scan-line
	for (int x = 1; x < w - 1; ++x)
	{
		sdfer_cell& c = row[x];
		int sqr = c.get_sqr_mag();

		const sdfer_cell& c0 = prev_row[x-1];
		const sdfer_cell& c1 = prev_row[x];
		const sdfer_cell& c2 = prev_row[x+1];
		const sdfer_cell& c3 = row[x - 1]; // fixme could handle this simpler

		sqr = merge_cell(c, sqr, c0, 1, dy);
		sqr = merge_cell(c, sqr, c1, 0, dy);
		sqr = merge_cell(c, sqr, c2, -1, dy);
		merge_cell(c, sqr, c3, 1, 0);
	}

	// 3. check up, left and up-left for the right-most piece
	sqr = merge_cell(row[w - 1], row[w - 1].get_sqr_mag(), prev_row[w - 1], 0, dy);
	sqr = merge_cell(row[w - 1], sqr, row[w-2], 1, 0);
	merge_cell(row[w - 1], sqr, prev_row[w - 2], 1, dy);


	// simple loop right to left
	sdfer_simple_scanline_r2l(row, w);
}



// handles top-down
static void sdfer_cell_passes(sdfer_cell* cells, int w, int h)
{
	sdfer_cell* row = cells;

	// top-loop
	sdfer_simple_scanline_l2r(row, w);
	sdfer_simple_scanline_r2l(row, w);

	// move downs each scanline
	for (int y = 1; y < h; ++y)
	{
		sdfer_cell* prev_row = row;

		// next row
		row += w;

		// fancy loop left to right
		sdfer_fancy_scanline(row, prev_row, w, 1);
	}

	// back-up each scanline
	for (int y = h - 2; y >= 0; --y)
	{
		sdfer_cell* prev_row = row;

		// next row
		row -= w;

		// fancy loop left to right
		sdfer_fancy_scanline(row, prev_row, w, -1);
	}

}


static inline uint8_t sdfer_norm(bool inv, float sqr_mag, float spread_mul)
{
	float v = sqr_mag;

	v = sqrtf(v);
	v += 0.5f; // +1 bias to add half-pixel

	if (inv)
		v = -v;

	// mul-add
	v *= spread_mul;
	v += 128.0f;

	int i = (int)v;

	// clamp to 0..255
	i = i > 255 ? 255 : i < 0 ? 0 : i;
	return i;
}

static inline uint8_t sdfer_norm4(
	bool inv0,
	bool inv1,
	bool inv2,
	bool inv3,
	float sqr_mag0,
	float sqr_mag1,
	float sqr_mag2,
	float sqr_mag3,
	float spread_mul_div4
)
{
	// add a quarter to each
	float v0 = sqrtf(sqr_mag0) + 0.5f;
	float v1 = sqrtf(sqr_mag1) + 0.5f;
	float v2 = sqrtf(sqr_mag2) + 0.5f;
	float v3 = sqrtf(sqr_mag3) + 0.5f;

	if (inv0) v0 = -v0;
	if (inv1) v1 = -v1;
	if (inv2) v2 = -v2;
	if (inv3) v3 = -v3;

	// mul-add
	float v = v0 + v1 + v2 + v3;
	v *= spread_mul_div4;
	v += 128.0f;

	int i = (int)v;

	// clamp to 0..255
	i = i > 255 ? 255 : i < 0 ? 0 : i;
	return i;
}


static void sdfer_final_pass(uint8_t* img, const sdfer_cell* cells, int stride, int oW, int oH, int spread, int downsample)
{
	float spread_mul = 128.0f / spread;
	float spread_mul_div4 = spread_mul / 4.f;

	uint8_t* write_pix = img;

	if (downsample == 1)
	{
		// no downsample, sligtly simpler?
		int s = oW * oH;
		for (int i = 0; i < s; ++i)
		{
			img[i] = sdfer_norm(img[i] == 0, (float)cells[i].get_sqr_mag(), spread_mul);
		}
		return;
	}

	if ((downsample & 1) != 0)
	{
		int half_downsample = downsample / 2;

		// odd scale, can pick center-sample
		for (int y = 0; y < oH; ++y)
		{
			int read_y = half_downsample + y * downsample;
			int read_ind = (read_y*stride) + half_downsample;

			const uint8_t* read_pix = img + read_ind;
			const sdfer_cell* read_cell = cells + read_ind;

			for (int x = 0; x < oW; ++x)
			{
				*write_pix = sdfer_norm(*read_pix == 0, (float)read_cell->get_sqr_mag(), spread_mul);
				++write_pix;

				read_pix += downsample;
				read_cell += downsample;
			}
		}
		return;
	}

	int half_downsample = (downsample / 2) - 1;

	// even scale, need to average 4 values
	for (int y = 0; y < oH ; ++y)
	{
		int read_y = half_downsample + y * downsample;
		int read_ind = (read_y*stride) + half_downsample;

		const uint8_t* read_pix = img + read_ind;
		const sdfer_cell* read_cell = cells + read_ind;

		for (int x = 0; x < oW; ++x)
		{
			*write_pix = sdfer_norm4(
				read_pix[0] == 0,
				read_pix[1] == 0,
				read_pix[stride] == 0,
				read_pix[stride+1] == 0,
				(float)read_cell[0].get_sqr_mag(),
				(float)read_cell[1].get_sqr_mag(),
				(float)read_cell[stride].get_sqr_mag(),
				(float)read_cell[stride+1].get_sqr_mag(),
				spread_mul_div4
			);
			++write_pix;

			read_pix += downsample;
			read_cell += downsample;
		}
	}
}






void sdfer_process_in_place(uint8_t* data_inout, int w, int h, int downsample, int spread, int& w_out, int& h_out, bool verbose)
{
	// fixme fail on an image that's too large
	// since we use int16_t for coordinates

	w_out = w / downsample;
	h_out = h / downsample;

	std::vector<sdfer_cell> cells_vector(w*h);
	sdfer_cell* cells = &cells_vector[0];

	auto t0 = get_now();

	sdfer_img_to_cells(cells, data_inout, w, h );

	auto t1 = get_now();

	sdfer_cell_passes(cells, w, h);

	auto t2 = get_now();

	sdfer_final_pass(data_inout, cells, w, w_out, h_out, spread, downsample);

	auto t3 = get_now();

	if (verbose)
	{
#if MEASURE_TIME
		printf("img -> cells : %.2f\n", get_diff_ms(t0, t1));
		printf("process      : %.2f\n", get_diff_ms(t1, t2));
		printf("final pass   : %.2f\n", get_diff_ms(t2, t3));
		printf("------------ : -----\n");
		printf("TOTAL        : %.2f\n", get_diff_ms(t0, t3));
#endif
	}

}




// License

// like fuzzy?