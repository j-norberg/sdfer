# README #

sdfer 1.0

Copyright 2021 Nils Jonas Norberg jnorberg@gmail.com

License: BSD-3-Clause ( https://opensource.org/licenses/BSD-3-Clause )

### What is this repository for? ###

* Generate SDF images (used for anti-aliased icons, text and more)
* Written in c/c++ with care for memory-access patterns
* Linear time over number of pixels


should add some example images here

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

1. clone this repo locally
```bash
git clone https://bitbucket.org/j_norberg/sdfer.git
cd sdfer
```

2. make
```bash
make
```

### Options ###

1. example uses

2. describe invert, pad, spread, downsample, crop, 

3. known limits:
	a. image-dimensions
	b. output format 8-bits (limits useful range of spread)

### Contribution guidelines ###

* please report bugs (include test image)

### Dependencies ###

only external dependencies is a cpp compiler 

sdfer relies on stb_image and stb_image_write (included in src/dep)
