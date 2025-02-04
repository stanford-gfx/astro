// On os x:
// g++ simple.cpp -g -I ./include -I DarwinX86/pex_policy/10.1+1/include/ -I DarwinX86/daf_persistence/10.1+1/include/ -I DarwinX86/utils/10.1+1/include/ -I DarwinX86/daf_base/10.1+2/include/ -I DarwinX86/base/10.1+1/include/ -I DarwinX86/ndarray/10.1+2/include/ -I DarwinX86/pex_exceptions/10.1+1/include/ -I DarwinX86/eigen/3.2.0/include/ -I DarwinX86/afw/10.1+1/include -L ./bin -L DarwinX86/afw/10.1+1/lib -L DarwinX86/daf_base/10.1+2/lib/ -L DarwinX86/daf_persistence/10.1+1/lib/ -L DarwinX86/boost/1.55.0.1.lsst2+3/lib/ -lHalide -lafw -ldaf_base -ldaf_persistence -lboost_system `libpng-config --cflags --ldflags` -o simple -std=c++11
//
// DYLD_LIBRARY_PATH=./bin:DarwinX86/afw/10.1+1/lib/:DarwinX86/daf_persistence/10.1+1/lib/:DarwinX86/daf_base/10.1+2/lib/:DarwinX86/boost/1.55.0.1.lsst2+3/lib/:DarwinX86/xpa/2.1.15.lsst2/lib/:DarwinX86/pex_policy/10.1+1/lib/:DarwinX86/pex_logging/10.1+1/lib/:DarwinX86/utils/10.1+1/lib/:DarwinX86/pex_exceptions/10.1+1/lib/:DarwinX86/base/10.1+1/lib/ ./simple

#ifndef STANDALONE
#include "lsst/afw/image.h"
namespace afwImage = lsst::afw::image;
namespace afwMath  = lsst::afw::math;
#endif
#include <stdio.h>

#include "Halide.h"
#include <bitset>
#include "clock.h"
using namespace std;
using namespace Halide;

using Halide::Image;

int main(int argc, char **argv) {

    //This image has 3 planes (image, mask, variance) and dimensions 2048 x 1489
#ifndef STANDALONE
    auto im = afwImage::MaskedImage<float>("./images/calexp-004207-g3-0123.fits");
    int width = im.getWidth(), height = im.getHeight();
#else
    int width = 2048, height = 1489;
    printf("[no load]");
#endif
    printf("Loaded: %d x %d\n", width, height);

    //store image data in img_var(x, y, 0) and variance data in img_var(x, y, 1)
    Image<float> image(width, height);
    Image<float> variance(width, height);
    Image<uint16_t> mask(width, height);

#ifndef STANDALONE
    //Read image in
    for (int y = 0; y < height; y++) {
        afwImage::MaskedImage<float, lsst::afw::image::MaskPixel, lsst::afw::image::VariancePixel>::x_iterator inPtr = im.x_at(0, y);
        for (int x = 0; x < width; x++){
            image(x, y) = (*inPtr).image();
            variance(x, y) = (*inPtr).variance();
            mask(x, y) = (*inPtr).mask();
            inPtr++;
        }
    }
#endif
	
	int boundingBox = 2;
    
    Var x, y, c, y0, yi, i, j, x_outer, y_outer, x_inner, y_inner, fused, tile_index;

    Func kernel;
    Func kernel1;
    Func kernel3;

//    kernel(i, j) = (i*j/3.1f);
    kernel(x, y) = exp(x*x/(y+10.0f))/(x*y+10.0f);

    kernel1(x, y) = exp(x*x/(y+10.0f))/(x*y+10.0f);

    kernel3(x, y) = exp(x*x/(y+10.0f))/(x*y+10.0f);


    Func image_bounded = BoundaryConditions::repeat_edge(image);
    Func variance_bounded = BoundaryConditions::repeat_edge(variance);

    //compute new Image values
/*	Func blur_image;
	Expr blur_image_help = 0.0f;
	for(int i = -boundingBox; i <= boundingBox; i++){
		for(int j = -boundingBox; j <= boundingBox; j++){
			blur_image_help += image_bounded(x + i, y + j) * kernel(i, j); 
		}
	}
*/
//    Func blur_image;
//    Expr blur_image_help = 0.0f;
//    for(int i = -boundingBox; i <= boundingBox; i++){
//        for(int j = -boundingBox; j <= boundingBox; j++){
//            blur_image_help += image_bounded(x, y) * kernel(x, y) +
//                image_bounded(x, y) * kernel(x-1, y) + 
//                image_bounded(x, y) * kernel(x-2, y) +
//                image_bounded(x, y) * kernel(x-3, y); 
//            blur_image_help += image_bounded(x, y) * kernel(x - (x%10), y - (y%10)); 
//            blur_image_help += image_bounded(x, y) * kernel(x/10, y/10); 
//            blur_image_help += image_bounded(x, y) * kernel(x, y); 
//            blur_image_help += image_bounded(x, y) + 1; 
//            blur_image_help += image_bounded(x, y) * kernel(0, 0); 
//        }
//    }   

    Func blur_image ("blur_image");
  blur_image(x, y) = image_bounded(x, y) * kernel(x/10, y/10);
//    blur_image(x, y) = image_bounded(x, y) * kernel(0, 0);

	//compute new Variance values 

/*	Func blur_variance;
	Expr blur_variance_help = 0.0f;
	for(int i = -boundingBox; i <= boundingBox; i++){
		for(int j = -boundingBox; j <= boundingBox; j++){
			blur_variance_help += variance_bounded(x + i, y + j)*kernel(i, j) * kernel(i, j); 
//			blur_variance_help += variance_bounded(x + i, y + j)/(25*25); 
//			blur_variance_help += variance_bounded(x + i, y + j) * kernel(i, j)/norm; 


		}
	}
    blur_variance(x, y) = blur_variance_help;

*/
    Func blur_variance ("blur_variance");
	blur_variance(x, y) = image_bounded(x, y) * kernel1(x, y);


//	kernel.compute_root();
//    kernel1.compute_root();

    kernel.compute_at(blur_image, x).store_at(blur_image, y0);
    kernel1.compute_at(blur_variance, x).store_at(blur_variance, y0);

    blur_image.split(y, y0, yi, 10);
    blur_variance.split(y, y0, yi, 10);

    blur_image.parallel(y0);
    blur_variance.parallel(y0);

    blur_image.vectorize(x, 8);
    blur_variance.vectorize(x, 8);
//    blur_image.unroll(x, 32);
/*
    blur_image.tile(x, y, x_outer, y_outer, x_inner, y_inner, 100, 10)
        .fuse(x_outer, y_outer, tile_index)
        .parallel(tile_index)
//        .vectorize(x_inner, 8);
//        .fuse(x_inner, y_inner, fused)
        .unroll(x_inner, 100); //FAST!
//        .unroll(x_inner, 10);
//        .unroll(fused, 100);
//        .parallel(y_outer);
*/



    blur_image.compile_to_lowered_stmt("simple.html", {image}, HTML);
//    blur_image.compile_to_lowered_stmt("simple_inline.html", {image}, HTML);
//    blur_image.compile_to_lowered_stmt("simple_compute_root.html", {image}, HTML);
  


    // Benchmark the pipeline.
    Image<float> image_output(image.width(), image.height());
    Image<float> variance_output(image.width(), image.height());

    blur_image.realize(image_output);
    blur_variance.realize(variance_output);



    //Compute mask
    Func mask_bounded = BoundaryConditions::repeat_edge(mask);

    Func maskOut;

    Expr maskOutHelp = 0;
    for(int i = -boundingBox; i <= boundingBox; i++)
    	for(int j = -boundingBox; j <= boundingBox; j++){
    		//bitwise OR in following line seems to cast result to int32, so mask_output must be typed int32, is this a problem?
    		maskOutHelp = select(kernel3(i, j) == 0.0f, maskOutHelp, maskOutHelp | mask_bounded(x + i, y + j));


    		//maskOutHelp = select(kernel3(i, j) == 0.0f, maskOutHelp, maskOutHelp + mask_bounded(x + i, y + j));

    	}

    maskOut(x, y) = maskOutHelp;

    // Split the y coordinate of the consumer into strips of 16 scanlines:
    maskOut.split(y, y0, yi, 30);
    // Compute the strips using a thread pool and a task queue.
    maskOut.parallel(y0);
    // Vectorize across x by a factor of four.
    maskOut.vectorize(x, 8);

    //
    Image<int32_t> mask_output(mask.width(), mask.height());

    maskOut.realize(mask_output);


//    blur_mask.realize(mask_output);

#ifndef STANDALONE
    //write image out
    auto imOut = afwImage::MaskedImage<float, lsst::afw::image::MaskPixel, lsst::afw::image::VariancePixel>(im.getWidth(), im.getHeight());
    for (int y = 0; y < imOut.getHeight(); y++) {
    	afwImage::MaskedImage<float, lsst::afw::image::MaskPixel, lsst::afw::image::VariancePixel>::x_iterator inPtr = imOut.x_at(0, y);

        for (int x = 0; x < imOut.getWidth(); x++){
        	afwImage::pixel::SinglePixel<float, lsst::afw::image::MaskPixel, lsst::afw::image::VariancePixel> 
            curPixel(image_output(x, y), mask_output(x, y), variance_output(x, y));
        	(*inPtr) = curPixel;
        	inPtr++;

        }
    }

	imOut.writeFits("./images/halideSimple.fits");
#endif

    double average = 0;
    double min;
    double max;
    double imgTime;
    double varTime;
    double maskTime;
    double avgImg = 0;
    double avgVar = 0;
    int numberOfRuns = 5;
    for (int i = 0; i < numberOfRuns; i++) {
        double t1 = current_time();
        blur_image.realize(image_output);
        double t2 = current_time();
        blur_variance.realize(variance_output);
        double t3 = current_time();
    	maskOut.realize(mask_output);
        double t4 = current_time();
        double curTime = (t4-t1);
        average += curTime;
        avgImg += t2-t1;
        avgVar += t3-t2;
        if(i == 0){
            min = curTime;
            max = curTime;
            imgTime = t2-t1;
            varTime = t3-t2;
            maskTime = t4-t3;
        }
        else{
            if(curTime < min){
                min = curTime;
                imgTime = t2-t1;
                varTime = t3-t2;
                maskTime = t4-t3;
            }
            if(curTime > max)
                max = curTime;
        }
    }
    average = average/numberOfRuns;
    avgImg = avgImg/numberOfRuns;
    avgVar = avgVar/numberOfRuns;
    std::cout << "Average Time: " << average << ", Min = " <<
    min << ", Max = " << max << ", with " << numberOfRuns <<
    " runs" << '\n';
    cout << "For fastest run total time = " << min << ", imgTime = " << imgTime << ", varTime = " << varTime << 
    "maskTime = " << maskTime << endl;
    cout << "Over 5 runs, average image time = " << avgImg << " average variance time = " << avgVar << endl;

    return 0;
}




