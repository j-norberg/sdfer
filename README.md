# README #

sdfer 1.0

Copyright 2021 Nils Jonas Norberg jnorberg@gmail.com

License: BSD-3-Clause ( https://opensource.org/licenses/BSD-3-Clause )

### What is this repository for? ###

* Generate SDF images (used for anti-aliased icons, text and more)
* Written in c/c++ with care for memory-access patterns
* Linear time over number of pixels

## Here are some examples: ##

Spread=2, extends the edge 2 pixels inside/outside
```bash
./sdfer -d=1 -s=2 test_images/square.png
```

|  Input image |  Output image | 
|---|---|
| ![](images/square.png?raw=true)  |  ![](images/example1.png?raw=true) |


Spread=8
```bash
./sdfer -d=1 -s=8 test_images/square.png
```

|  Input image |  Output image | 
|---|---|
| ![](images/square.png?raw=true)  |  ![](images/example2.png?raw=true) |


### How do I get set up? ###

(For Windows, install Msys2, also the executable becomes "sdfer.exe")

1. clone this repo locally
```bash
git clone https://github.com/j-norberg/sdfer.git

cd sdfer
```

2. make
```bash
make
```

3. run internal tests
```bash
./sdfer --test
```

### Options ###


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

Known limits:
	a. image-dimensions
	b. output format 8-bits (limits useful range of spread)

### Contribution guidelines ###

* please report bugs (include test image)

### Dependencies ###

only external dependencies is a cpp compiler 

sdfer relies on stb_image and stb_image_write (included in src/dep)
