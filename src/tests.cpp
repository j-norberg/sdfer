

#include <stdint.h>
#include <vector>
#include <string>

#include "../../ok_sdf/src/ok_sdf.h"

// returns the largest error
int diff_img(const uint8_t* img_a, const uint8_t* img_b, int w, int h)
{
	int s = w * h;
	int m = 0;
	for (int i = 0; i < s; ++i)
	{
		int d = img_a[i] - img_b[i];
		
		if (d < 0)
			d = -d;

		if (d > m)
			m = d;
	}

	return m;
}







void test_mask()
{
	// expected behavior around thresholds
	// test short and long to expsoe unrolling-issues
	// test 0, 127, 128, and 255?
}

void test_pad()
{
}

void test_crop()
{

}




bool perform_test_sdf(
	int w,
	int h,
	std::vector<uint8_t> img,
	int downscale,
	int spread,
	std::vector<uint8_t> expected_img,
	const char* name
)
{
	// ensure is mask
	int s = w * h;

	if (img.size() != s)
	{
		printf("FAILURE %s (image not correct size)\n", name);
		return false;
	}

	for (int i = 0 ; i < s; ++i)
	{
		if (img[i] > 1)
		{
			printf("FAILURE %s (image not a normal mask 0/1)\n", name);
			return false;
		}
	}

	bool verbose = false;
	int oW;
	int oH;
	ok_sdf_process_in_place(&img[0], w, h, downscale, spread, oW, oH, nullptr, verbose);

	int oS = oW * oH;
	if (expected_img.size() != oS)
	{
		printf("FAILURE %s (expected image not correct size)\n", name);
		return false;
	}


	int diff = diff_img(&img[0], &expected_img[0], oW, oH);
	if (diff > 0)
	{
		printf("FAILURE %s (bad image)\n", name);
		return false;
	}

	printf("SUCCESS %s\n", name);
	return true;
}


// simple tests, only one row or column
void test_strips()
{
	// 2x1 / 1x2
	perform_test_sdf(2, 1, { 0, 1 }, 1, 1, { 64, 192 }, "2x1-1-1");
	perform_test_sdf(1, 2, { 0, 1 }, 1, 1, { 64, 192 }, "1x2-1-1");

	// 3x1 / 1x3
	perform_test_sdf(3, 1, { 0, 1, 0 }, 1, 1, { 64, 192, 64 }, "3x1-1-1");
	perform_test_sdf(1, 3, { 0, 1, 0 }, 1, 1, { 64, 192, 64 }, "1x3-1-1");

	// 4x1 / 4x1
	perform_test_sdf(4, 1, { 1, 1, 0, 0 }, 1, 1, { 255, 192, 64, 0 }, "4x1-1-1");
	perform_test_sdf(1, 4, { 1, 1, 0, 0 }, 1, 1, { 255, 192, 64, 0 }, "1x4-1-1");

	// spread 2
	perform_test_sdf(6, 1, { 1, 1, 1, 0, 0, 0 }, 1, 2, { 255, 224, 160, 96, 32, 0 }, "6x1-1-2");
	perform_test_sdf(1, 6, { 1, 1, 1, 0, 0, 0 }, 1, 2, { 255, 224, 160, 96, 32, 0 }, "1x6-1-2");
}

void test_squares()
{
	perform_test_sdf(3, 3, { 0,0,0, 0,1,0, 0,0,0 }, 1, 1, { 0,64,0, 64,192,64, 0,64,0 }, "3x3-1-1");
}



void test_downscale()
{
	// should be same (similar) result as 2x1
	perform_test_sdf(4, 2, { 0,0,1,1, 0,0,1,1 }, 2, 2, { 64, 192 }, "4x1-2-2");

	// should blend down to 'on the edge'
	perform_test_sdf(4, 4, { 0,1,0,1, 1,0,1,0, 0,1,0,1, 1,0,1,0 }, 2, 2, { 128, 128, 128, 128 }, "4x4-2-2");
}

void test_edge()
{
	// make sure the edge is maintained at 127.5
	perform_test_sdf(2, 1, { 0,1 }, 1, 127, { 127, 128 }, "2x1-1-127");
}




void sdfer_tests()
{
	// test masking, cropping padding
	test_mask();
	test_pad();
	test_crop();

	// test actual sdf-code
	test_strips();
	test_squares();
	test_downscale();
	test_edge();
}