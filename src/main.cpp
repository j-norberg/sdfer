

#include <stdint.h>
#include <vector>
#include <cstring> // for strrchr
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "dep/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "dep/stb_image_write.h"

#include "../../ok_sdf/src/ok_sdf.h"
#include "tests.h"






void downsample_in_place(uint8_t* rgbx, uint8_t* downsampled_a, int downsample, int big_w, int big_h)
{
	int dw = big_w / downsample;
	int dh = big_h / downsample;

	int half = downsample / 2;

	// fixme replace with multiply and shift
	// and maybe make the downsample function even cheaper (by skipping pixels)
	int div = downsample * downsample;

	// dw, dh are the downsampled ones
	for (int y = 0; y < dh; ++y)
	{
		for (int x = 0; x < dw; ++x)
		{
			const uint8_t alpha = *downsampled_a;
			++downsampled_a;

			const uint8_t* src = rgbx + (y*downsample*big_w*4) + (x*downsample)*4;
			uint8_t* dst = rgbx + y*dw*4 + x*4;

			if (alpha < 100)
			{ 
				// where alpha is low-ish, we're outside the expected image anyways...
				// simple case here, just grab center(ish)-pixel
				const uint8_t* p = src + (half*big_w * 4) + (half) * 4;
				dst[0] = p[0];
				dst[1] = p[1];
				dst[2] = p[2];
			}
			else
			{
				int sumR = 0;
				int sumG = 0;
				int sumB = 0;
				for (int yy = 0; yy < downsample; ++yy)
				{
					const uint8_t* row = src + yy * big_w*4;
					for (int xx = 0; xx < downsample; ++xx)
					{
						sumR += row[0];
						sumG += row[1];
						sumB += row[2];
						row += 4;
					}
				}
				dst[0] = sumR / div;
				dst[1] = sumG / div;
				dst[2] = sumB / div;
			}

		}
	}
}

void interleave_alpha(uint8_t* rgbx, uint8_t* alpha, int w, int h)
{
	int s = w * h;
	for (int i = 0; i < s; ++i)
	{
		rgbx[3] = alpha[0];
		rgbx += 4;
		alpha += 1;
	}
}







enum FileFormats { eUnknown, ePng, eTga, eJpg, eBmp };

const char* get_suffix(FileFormats f)
{
	switch (f)
	{
	case eUnknown: return ".unknown1";
	case ePng: return ".png";
	case eTga: return ".tga";
	case eJpg: return ".jpg";
	case eBmp: return ".bmp";
	}
	return ".unknown2";
}

FileFormats parse_file_format(const char* arg)
{
	if (arg == nullptr)
		return eUnknown;

	switch (arg[0])
	{
	case 'p':
		if (0 == strcmp("png", arg)) return ePng;
		break;

	case 't':
		if (0 == strcmp("tga", arg)) return eTga;
		break;

	case 'j':
		if (0 == strcmp("jpg", arg)) return eJpg;
		break;

	case 'b':
		if (0 == strcmp("bmp", arg)) return eBmp;
		break;

	default:
		break;
	}

	return eUnknown;
}








const char *app_name = "sdfer 1.0";

void print_usage()
{
	puts(app_name);

	const char *long_string = R""""(
usage: sdfer [opts] infile1.png [infile2.tga]

opts:
	-h     : show help
	-v     : print details of operation

	-t=127 : threshold  (integer 0..254)     (default: 127)
	-i     : invert inside-outside");
	-p     : pad
	-s=100 : spread     (integer 1+)         (default: 100)
	-d=2   : downsample (integer 1+)         (default:   2)
	-c     : crop
	-k     : Keep RGB
	-x     : Fix zero-alpha pixels from photoshop 

	-f=png : out-file-format, one of: png, tga, bmp, jpg (default: png)

	--help : show help
	--test : run internal tests
)"""";

	puts(long_string);

	// fixme allow:
	// out-directory
	// out-naming convention?
}

void print_help()
{
	const char *long_string = R""""(
Explanation of the options:

-t=127 Threshhold
	When the input-image is read, the threshold-value determines
	what pixel is considered a part of the masked image. Defaults
	to 127, can be between 0 and 254 inclusive.

-i Invert
	When the input-image is read, anything above the threshold-value
	is considered part of the masked image. When -i is active the
	result is inverted, so anything above the threshold is
	considered outside.

-p Pad
	When the pad-option is active, the input-image is enlarged with
	the spread-amount in every direction, this ensures that the
	full fade-out will be present in the out-image, and not cut off.

-s=100 Spread
	The signed distance field will extend this far (in pixels). For
	use with font-shapes or similar usage, lower number will give
	you better resolution, but for a larger value can enable more
	advanced features (blur, shadow, outlines)

-d=2 Downsample
	This option allow you to down-sample the output-image. This	is
	common to increase the fidelity of angles in shapes.

-c Crop
	This options will look at the output image and crop it so that
	the fade touches the edges in each direction.

-k Keep RGB
	This options will keep the rgb components, and combine them
	with the sdf in the alpha-channel (can not be combined with
	crop or pad)

-x Fix zero-alpha pixels from photoshop 
	This options is only used with the "keep rgb" option. It will
	attempt to "heal" the color-information in pixels with zero
	alpha (that PS sets to white) by finding the closest pixel
	with a non-zero alpha

Examples go here:

)"""";

	puts(app_name);
	puts(long_string);
}






// defaults
struct Args
{
	// report-issues
	int unknown_opt = 0;
	int opt_missing_value = 0;

	// actual opts
	int threshold = 127;
	int spread = 100;
	int downsample = 2;
	FileFormats file_format = ePng;

	bool crop = false;
	bool invert = false;
	bool pad = false;
	bool verbose = false;
	bool help = false;
	bool test = false;

	bool keep_rgb = false;
	bool keep_rgb_ps_fix = false;

	// non-opts
	std::vector<char*> file_names;
};


void handle_opts(Args& args_out, char**argv, int i)
{
	char* arg = argv[i];

	char* arg_run = arg + 1;

	if (arg_run == 0)
	{
		puts("empty opt?");
		args_out.unknown_opt = true;
		return;
	}

	if (*arg_run == '-')
	{
		++arg_run;

		if (0 == strcmp(arg_run, "test"))
		{
			args_out.test = true;
			return;
		}

		if (0 == strcmp(arg_run, "help"))
		{
			args_out.help = true;
			return;
		}

		printf("unknown opt: %s\n", arg);
		args_out.unknown_opt = 1;
		return;
	}

	// allow to parse multiple single character opts in a group
	// like -chi
	for ( ; arg_run != 0; ++arg_run)
	{
		bool found = true;

		// test for opts with no value
		switch (*arg_run)
		{
		case 'c': args_out.crop = true;		break;
		case 'h': args_out.help = true;		break;
		case 'i': args_out.invert = true;	break;
		case 'p': args_out.pad = true;		break;
		case 'v': args_out.verbose = true;	break;
		case 'k': args_out.keep_rgb = true; break;
		case 'x': args_out.keep_rgb_ps_fix = true; break;

		default:
			found = false;
			break;
		}

		if (!found)
		{
			break;
		}
	}

	if (*arg_run == 0)
	{
		// done
		return;
	}

	bool has_value = (arg_run[1] == '=') && (arg_run[2] != 0);
	char* value = arg_run + 2;

	switch (*arg_run)
	{
	case 'd':
		if (!has_value)
		{
			printf("d/downsample need value\n");
			args_out.opt_missing_value = 1;
		}
		else
		{
			args_out.downsample = atoi(value);
		}
		break;

	case 'f':
		if (!has_value)
		{
			printf("f/format need value\n");
			args_out.opt_missing_value = 1;
		}
		else
		{
			args_out.file_format = parse_file_format(value);
		}
		break;
		
	case 's' :
		if (!has_value)
		{
			printf("s/spread need value\n");
			args_out.opt_missing_value = 1;
		}
		else
		{
			args_out.spread = atoi(value);
		}
		break;

	case 't':
		if (!has_value)
		{
			printf("t/threshold need value\n");
			args_out.opt_missing_value = 1;
		}
		else
		{
			args_out.threshold = atoi(value);
		}
		break;

	default:
		printf("unknown opt: %s\n", arg_run);
		args_out.unknown_opt = 1;
		break;
	}
}

void check_valid_args_for_possible_issues(const Args& args_out)
{
	// stuff to warn about

	// result in banding?
	float downsampled_spread = (float)args_out.spread / (float)args_out.downsample;
	if (downsampled_spread > 127)
	{
		printf("WARNING: The spread in the downsampled image is > 127 (%.2f)\n", downsampled_spread);
		puts("The 8-bit output-image(s) might have banding and not work well as a signed distance field. Depending on use:");
		puts(" 1. you wouldn't get anti-aliased edges");
		puts(" 2. you wouldn't be able to calculate the dx/dy inside a band");
	}

}

bool handle_args(Args& args_out, int argc, char**argv)
{
	if (argc < 2)
	{
		// fast skip for no args
		print_usage();
		return false;
	}

	// read args (skip 1st)
	for (int i = 1; i < argc; ++i)
	{
		char* arg = argv[i];

		if (arg[0] == '-')
		{
			handle_opts(args_out, argv, i);
			continue;
		}

		args_out.file_names.push_back(arg);
	}

	// check args
	bool ok = true;

	// need help?
	if (args_out.help)
	{
		print_help();
		return false;
	}

	if (args_out.test)
	{
		sdfer_tests();
		return false;
	}

	// any files
	if (args_out.file_names.size() < 1)
	{
		puts("missing filenames");
		ok = false;
	}

	if (args_out.downsample < 1)
	{
		puts("downsample can not be < 1 (default 1)");
		ok = false;
	}

	if (args_out.spread < 1)
	{
		puts("spread can not be < 1 (default 100)");
		ok = false;
	}

	if (args_out.threshold < 0 || args_out.threshold > 254)
	{
		puts("threshold need to be between 0 and 254 (default 127)");
		ok = false;
	}

	if (args_out.file_format == eUnknown)
	{
		printf("file-format need to be png, tga, jpg, or bmp\n");
		ok = false;
	}

	if (args_out.keep_rgb_ps_fix && !args_out.keep_rgb)
	{
		printf("can not do ps-fix without keep-rgb\n");
		ok = false;
	}

	if (args_out.keep_rgb)
	{
		if (args_out.pad)
		{
			printf("can not use pad with keep-rgb\n");
			ok = false;
		}

		if (args_out.crop)
		{
			printf("can not use crop with keep-rgb\n");
			ok = false;
		}

		if (args_out.file_format == eJpg)
		{
			printf("can not use keep-rgb when writing JPGs (since no alpha)\n");
			ok = false;
		}

	}

	if (args_out.opt_missing_value)
	{
		ok = false;
	}

	if (args_out.unknown_opt)
	{
		ok = false;
	}

	if ( !ok )
	{
		print_usage();
	}

	// if there is no worse issue, test for half-bad inputs
	if (ok)
	{
		check_valid_args_for_possible_issues(args_out);
	}

	return ok;
}














bool handle_one_image( const char* fname, Args args )
{
	// load image 
	int w = 0;
	int h = 0;
	int channels = 0;
	uint8_t *input_image = stbi_load(fname, &w, &h, &channels, 0);

	if (input_image == NULL)
	{
		printf("Error in loading the image (%s)\n", fname);
		return false;
	}

	if ((w*h*channels) < 1)
	{
		printf("Image does not have enough data (%s)\n", fname);
		stbi_image_free(input_image);
		return false;
	}

	if (args.verbose)
	{
		printf("Loaded image with a width of %d px, a height of %d px and %d channels\n", w, h, channels);
		float mpx = (w * h) / (float)(1024 * 1024);
		printf("Total of %.2f megapixels\n", mpx);
	}

	// test downscale size
	int oW = w / args.downsample;
	int oH = h / args.downsample;
	if (oW < 2 || oH < 2)
	{
		printf("Image is not large enough to be downsampled (w:%d h:%d) %d\n", w, h, args.downsample);
		stbi_image_free(input_image);
		return false;
	}

	// fixme clarify how channels are picked in readme
	uint8_t* stored_rgba = nullptr;
	int extractChannel = 0;
	if (channels == 4)
	{
		if (args.verbose)
		{
			puts("detected 4-channel image, will extract alpha-channel");
		}
		extractChannel = 3; // alpha

		if (args.keep_rgb)
		{
			if (args.crop)
			{
				puts("keep-rgb is not compatible with crop");
				args.keep_rgb = false;
			}

			if (args.pad)
			{
				puts("keep-rgb is not compatible with pad");
				args.keep_rgb = false;
			}

			if (args.file_format == eJpg)
			{
				puts("keep-rgb is not compatible with jpg file type");
				args.keep_rgb = false;
			}
		}

		if (args.keep_rgb)
		{
			puts("keeping rgb");

			// leave room for alpha
			stored_rgba = (uint8_t*)malloc(w*h*4); // rgbx
			memcpy(stored_rgba, input_image, w*h*4);
		}
	}

	// extract mask
	ok_sdf_extract_mask_in_place(input_image, w, h, channels, extractChannel, args.threshold, args.invert);

	int spread = args.spread;

	// pad mask-image
	if (args.pad)
	{
		// allocate padded-replacement
		int pw = w + spread * 2;
		int ph = h + spread * 2;
		uint8_t* dst_img = (uint8_t*)malloc(pw * ph);
		ok_sdf_pad_mask(dst_img, input_image, w, h, spread);

		// free old
		stbi_image_free(input_image);
		
		// 
		input_image = dst_img;
		w += spread * 2;
		h += spread * 2;

		if (args.verbose)
		{
			printf("Padded image has width of %d px, a height of %d\n", w, h);
		}
	}

	uint8_t* rgba_to_ps_fix = nullptr;
	if (args.keep_rgb_ps_fix)
	{
		if (args.keep_rgb)
		{
			puts("ps-fix");
			rgba_to_ps_fix = stored_rgba;
		}
		else
		{
			printf("ps-fix only available with keep-rgb\n");
		}
	}

	//
	int big_w = w;
	int big_h = h;

	// process
	ok_sdf_process_in_place(input_image, w, h, args.downsample, args.spread, w, h, rgba_to_ps_fix, args.verbose);

	if (stored_rgba != nullptr)
	{
		// downsample rgb
		if (args.downsample > 1)
		{
			downsample_in_place(stored_rgba, input_image, args.downsample, big_w, big_h);
		}
	}

	if (args.crop)
	{
		ok_sdf_crop_image_in_place(input_image, w, h);
	}

	// choose right file format
	std::string outfname(fname);

	// remove ".png" (or something)
	int period_pos = (int)outfname.size() - 4;
	if (period_pos >= 0 && outfname[period_pos] == '.')
	{
		outfname.resize(period_pos);
	}

	// add suffix
	outfname += "_sdf";
	outfname += get_suffix(args.file_format);

	int write_result = 0;

	if (stored_rgba != nullptr)
	{
		interleave_alpha(stored_rgba, input_image, w, h);

		switch (args.file_format)
		{
		case eUnknown: break;

		case eBmp: write_result = stbi_write_bmp(outfname.c_str(), w, h, 4, stored_rgba);		break;
		case eTga: write_result = stbi_write_tga(outfname.c_str(), w, h, 4, stored_rgba);		break;
		case eJpg: write_result = -1; break; // no alpha on jpg
		case ePng: write_result = stbi_write_png(outfname.c_str(), w, h, 4, stored_rgba, w*4);		break;
		}

		free(stored_rgba);
	}
	else
	{
		switch (args.file_format)
		{
		case eUnknown: break;

		case eBmp: write_result = stbi_write_bmp(outfname.c_str(), w, h, 1, input_image);		break;
		case eTga: write_result = stbi_write_tga(outfname.c_str(), w, h, 1, input_image);		break;
		case eJpg: write_result = stbi_write_jpg(outfname.c_str(), w, h, 1, input_image, 80);	break;
		case ePng: write_result = stbi_write_png(outfname.c_str(), w, h, 1, input_image, w);	break;
		}
	}

	if (write_result == 0)
	{
		printf("failed writing file %s\n", outfname.c_str() );
	}

	// free
	stbi_image_free(input_image);

	return true;
}





int main(int argc, char**argv)
{
	Args args;
	bool ok = handle_args(args, argc, argv);
	if (!ok)
	{
		return -1;
	}

	for (int i = 0; i < args.file_names.size(); ++i)
	{
		handle_one_image(args.file_names[i], args);
	}
}