

#include <stdint.h>
#include <vector>
#include <cstring> // for strrchr
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "dep/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "dep/stb_image_write.h"

#include "sdfer.h"
#include "tests.h"









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










void print_usage()
{
	puts("sdfer 1.0" );
	puts("usage: sdfer [opts] infile1.png [infile2.tga]");
	puts("opts:");
	puts(" -h     : help");
	puts(" -v     : print details of operation");
	puts(" -t=127 : threshold  (integer 0..254)     (default: 127)");
	puts(" -i     : invert inside-outside");
	puts(" -p     : pad");
	puts(" -s=100 : spread     (integer 1+)         (default: 100)");
	puts(" -d=2   : downsample (integer  1+)        (default:   2)");
	puts(" -c     : crop");
	puts(" -f=png : out-file-format png tga bmp jpg (default: png)");

	puts(" --test : run tests");

	// fixme allow:
	// out-directory
	// out-naming convention?
}

void print_help()
{
	puts("sdfer 1.0");
	puts("heeeeelp");
}






// defaults
struct Args
{
	// report-issues
	int unknown_opt = 0;
	int opt_missing_value = 0;

	// actual opts
	int threshold = 127;
	bool invert = false;
	bool pad = false;
	int spread = 100;
	int downsample = 2;
	bool crop = false;
	FileFormats file_format = ePng;

	bool verbose = false;
	bool help = false;

	// non-opts
	std::vector<char*> file_names;
};


void handle_opts(Args& args_out, int argc, char**argv, int i)
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
		// handle --test
		// --help?
		if (0 == strcmp(arg_run, "test"))
		{
			// handle test
			sdfer_tests();
			return;
		}

		if (0 == strcmp(arg_run, "help"))
		{
			// handle test
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

	char* eq = strchr(arg_run, '=');
	bool has_value = eq != nullptr;
	char* value = eq + 1;

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
		return false;
	}

	// read args (skip 1st)
	for (int i = 1; i < argc; ++i)
	{
		char* arg = argv[i];

		if (arg[0] == '-')
		{
			handle_opts(args_out, argc, argv, i);
			continue;
		}

		args_out.file_names.push_back(arg);
	}

	// check args
	bool ok = true;

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
	else
	{
		if (args_out.help)
		{
			print_help();
			ok = false;
		}
	}

	// if there is no worse issue, test for half-bad inputs
	if (ok)
	{
		check_valid_args_for_possible_issues(args_out);
	}

	return ok;
}











// allocate new and delete old
// only single channel mask-image
// memcpy is safe since writing to new memory
uint8_t* replace_with_padded_image(uint8_t* src_img, int w, int h, int pad)
{
	int pw = w + pad * 2;
	int ph = h + pad * 2;

	uint8_t* dst_img = (uint8_t*)malloc(pw * ph);

	// top band
	memset(dst_img, 0, pw*pad);

	int last_h = h - 1;

	// middle
	for (int y = 0; y < h; ++y)
	{
		uint8_t* src_row = src_img + y * w;
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
	memset(dst_img + (ph-pad) * pw, 0, pw*pad);

	stbi_image_free(src_img);
	return dst_img;
}


// adjust w and h
void crop_in_place(uint8_t* src_img, int& w_inout, int &h_inout)
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
		uint8_t* src_row = src_img + (y0+y) * oW + x0;
		memmove(dst_row, src_row, nW);
	}

	// 3. adjust w, h
	w_inout = nW;
	h_inout = nH;
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
	int extractChannel = 0;
	if (channels == 4)
	{
		if (args.verbose)
		{
			puts("detected 4-channel image, will extract alpha-channel");
		}
		extractChannel = 3; // alpha
	}

	// extract mask
	sdfer_masker_extract_mask_in_place(input_image, w, h, channels, extractChannel, args.threshold, args.invert);


	int spread = args.spread;

	// pad mask-image
	if (args.pad)
	{
		// replacement
		input_image = replace_with_padded_image(input_image, w, h, spread);
		w += spread * 2;
		h += spread * 2;

		if (args.verbose)
		{
			printf("Padded image has width of %d px, a height of %d\n", w, h);
		}
	}


	// allocate outside sdfer-code
	std::vector<uint8_t> scratchpad(sdfer_need_scratchpad_bytes(w, h));

	// process
	sdfer_process_in_place(input_image, w, h, args.downsample, args.spread, w, h, &scratchpad[0]);

	if (args.crop)
	{
		crop_in_place(input_image, w, h);
	}

	// choose right file format
	std::string outfname(fname);
	outfname += ".sdf";
	outfname += get_suffix(args.file_format);

	int write_result = 0;
	switch (args.file_format)
	{
		case eBmp: write_result = stbi_write_bmp(outfname.c_str(), w, h, 1, input_image);		break;
		case eTga: write_result = stbi_write_tga(outfname.c_str(), w, h, 1, input_image);		break;
		case eJpg: write_result = stbi_write_jpg(outfname.c_str(), w, h, 1, input_image, 80);	break;
		case ePng: write_result = stbi_write_png(outfname.c_str(), w, h, 1, input_image, w);	break;
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