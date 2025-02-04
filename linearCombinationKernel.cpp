// On os x:
// g++ linearCombinationKernel.cpp -g -I ./include -I DarwinX86/pex_policy/10.1+1/include/ -I DarwinX86/daf_persistence/10.1+1/include/ -I DarwinX86/utils/10.1+1/include/ -I DarwinX86/daf_base/10.1+2/include/ -I DarwinX86/base/10.1+1/include/ -I DarwinX86/ndarray/10.1+2/include/ -I DarwinX86/pex_exceptions/10.1+1/include/ -I DarwinX86/eigen/3.2.0/include/ -I DarwinX86/afw/10.1+1/include -L ./bin -L DarwinX86/afw/10.1+1/lib -L DarwinX86/daf_base/10.1+2/lib/ -L DarwinX86/daf_persistence/10.1+1/lib/ -L DarwinX86/boost/1.55.0.1.lsst2+3/lib/ -lHalide -lafw -ldaf_base -ldaf_persistence -lboost_system `libpng-config --cflags --ldflags` -o linearCombinationKernel -std=c++11
//
// DYLD_LIBRARY_PATH=./bin:DarwinX86/afw/10.1+1/lib/:DarwinX86/daf_persistence/10.1+1/lib/:DarwinX86/daf_base/10.1+2/lib/:DarwinX86/boost/1.55.0.1.lsst2+3/lib/:DarwinX86/xpa/2.1.15.lsst2/lib/:DarwinX86/pex_policy/10.1+1/lib/:DarwinX86/pex_logging/10.1+1/lib/:DarwinX86/utils/10.1+1/lib/:DarwinX86/pex_exceptions/10.1+1/lib/:DarwinX86/base/10.1+1/lib/ ./linearCombinationKernel
//
// On linux:
// g++ linearCombinationKernel.cpp -g -I ./include -I Linux64/pex_policy/10.1+1/include/ -I Linux64/daf_persistence/10.1+1/include/ -I Linux64/utils/10.1+1/include/ -I Linux64/daf_base/10.1+2/include/ -I Linux64/base/10.1+1/include/ -I Linux64/ndarray/10.1+2/include/ -I Linux64/pex_exceptions/10.1+1/include/ -I Linux64/eigen/3.2.0/include/ -I Linux64/afw/10.1+1/include -L ./bin -L Linux64/afw/10.1+1/lib -L Linux64/daf_base/10.1+2/lib/ -L Linux64/daf_persistence/10.1+1/lib/ -L Linux64/boost/1.55.0.1.lsst2+3/lib/ -L Linux64/wcslib/4.14+7/lib/ -lHalide -lafw -ldaf_base -ldaf_persistence -lboost_system `libpng-config --cflags --ldflags` -lpthread -ldl -o linearCombinationKernel -std=c++11
//
//LD_LIBRARY_PATH=./bin:Linux64/afw/10.1+1/lib/:Linux64/daf_persistence/10.1+1/lib/:Linux64/daf_base/10.1+2/lib/:Linux64/boost/1.55.0.1.lsst2+3/lib/:Linux64/xpa/2.1.15.lsst2/lib/:Linux64/pex_policy/10.1+1/lib/:Linux64/pex_logging/10.1+1/lib/:Linux64/utils/10.1+1/lib/:Linux64/pex_exceptions/10.1+1/lib/:Linux64/base/10.1+1/lib/:Linux64/wcslib/4.14+7/lib/:Linux64/cfitsio/3360.lsst1/lib/:Linux64/gsl/1.16.lsst1/lib/:Linux64/minuit2/5.28.00/lib:Linux64/mysql/5.1.65.lsst2/lib/ ./linearCombinationKernel

//#define TESTING_GPU //define to simplify GPU testing
//#define STANDALONE

#if !defined(STANDALONE) && !defined(TESTING_GPU)
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


int main(int argc, char *argv[]) {
#if !defined(STANDALONE) && !defined(TESTING_GPU)
    auto im = afwImage::MaskedImage<float>("../calexp-004207-g3-0123.fits");
    int width = im.getWidth(), height = im.getHeight();

#else
    int width = 2048, height = 1489;
//    int width = 200, height = 200;
    printf("[no load]");
#endif
    printf("Loaded: %d x %d\n", width, height);

    //store image data in img_var(x, y, 0) and variance data in img_var(x, y, 1)
    Image<float> image(width, height);
    Image<float> variance(width, height);
    Image<uint16_t> mask(width, height);

#if !defined(STANDALONE) && !defined(TESTING_GPU) 
    //Read image in
    for (int y = 0; y < im.getHeight(); y++) {
        afwImage::MaskedImage<float, lsst::afw::image::MaskPixel, lsst::afw::image::VariancePixel>::x_iterator inPtr = im.x_at(0, y);
        for (int x = 0; x < im.getWidth(); x++){
            image(x, y) = (*inPtr).image();
            variance(x, y) = (*inPtr).variance();
            mask(x, y) = (*inPtr).mask();
            inPtr++;
        }
    }
#endif

    int boundingBox = 5; 
    Var x, y, i_v, y0, yi;

    //compute output image and variance
    //Polynomials that define weights of spatially variant linear combination of 5 kernels
    Func polynomial1, polynomial2, polynomial3, polynomial4, polynomial5;
    polynomial1(x, y) = 0.1f + 0.002f*x + 0.003f*y + 0.4f*x*x + 0.5f*x*y
                     + 0.6f*y*y +  0.0007f*x*x*x + 0.0008f*x*x*y + 0.0009f*x*y*y
                     + 0.00011f*y*y*y;

    //for experimenting with optimizations
    polynomial2(x, y) = 1.1f + 1.002f*x + 1.003f*y + 1.4f*x*x + 1.5f*x*y
                     + 1.6f*y*y +  1.0007f*x*x*x + 1.0008f*x*x*y + 1.0009f*x*y*y
                     + 1.00011f*y*y*y;

    //for experimenting with optimizations

    polynomial3(x, y) = 2.1f + 2.002f*x + 2.003f*y + 2.4f*x*x + 2.5f*x*y
                     + 2.6f*y*y +  2.0007f*x*x*x + 2.0008f*x*x*y + 2.0009f*x*y*y
                     + 2.00011f*y*y*y;

    //for experimenting with optimizations
    polynomial4(x, y) = 3.1f + 3.002f*x + 3.003f*y + 3.4f*x*x + 3.5f*x*y
                     + 3.6f*y*y +  3.0007f*x*x*x + 3.0008f*x*x*y + 3.0009f*x*y*y
                     + 3.00011f*y*y*y;

    //for experimenting with optimizations
    polynomial5(x, y) = 4.1f + 4.002f*x + 4.003f*y + 4.4f*x*x + 4.5f*x*y
                     + 4.6f*y*y +  4.0007f*x*x*x + 4.0008f*x*x*y + 4.0009f*x*y*y
                     + 4.00011f*y*y*y;

    //Kernel #1
    Func kernel1;
    float sigmaX1 = 2.0f;
    float sigmaY1 = 2.0f;
    float theta1 = 0.0f; //rotation of sigmaX axis
    kernel1(x, y) = (exp(-((x*cos(theta1) +y*sin(theta1))*(x*cos(theta1) +y*sin(theta1)))
                    /(2*sigmaX1*sigmaX1)) / (sqrtf(2*M_PI)*sigmaX1))
                    *(exp(-((y*cos(theta1) - x*sin(theta1))*(y*cos(theta1) - x*sin(theta1)))
                    /(2*sigmaY1*sigmaY1)) / (sqrtf(2*M_PI)*sigmaY1));



    //Kernel #2
    Func kernel2;
    float sigmaX2 = 0.5f;
    float sigmaY2 = 4.0f;
    float theta2 = 0.0f; //rotation of sigmaX axis
    kernel2(x, y) = (exp(-((x*cos(theta2) +y*sin(theta2))*(x*cos(theta2) +y*sin(theta2)))
                    /(2*sigmaX2*sigmaX2)) / (sqrtf(2*M_PI)*sigmaX2))
                    *(exp(-((y*cos(theta2) - x*sin(theta2))*(y*cos(theta2) - x*sin(theta2)))
                    /(2*sigmaY2*sigmaY2)) / (sqrtf(2*M_PI)*sigmaY2));

    //Kernel #3
    Func kernel3;
    float sigmaX3 = 0.5f;
    float sigmaY3 = 4.0f;
    float theta3 = 3.14159f/4; //rotation of sigmaX axis
    kernel3(x, y) = (exp(-((x*cos(theta3) +y*sin(theta3))*(x*cos(theta3) +y*sin(theta3)))
                    /(2*sigmaX3*sigmaX3)) / (sqrtf(2*M_PI)*sigmaX3))
                    *(exp(-((y*cos(theta3) - x*sin(theta3))*(y*cos(theta3) - x*sin(theta3)))
                    /(2*sigmaY3*sigmaY3)) / (sqrtf(2*M_PI)*sigmaY3));
    //Kernel #4
    Func kernel4;
    float sigmaX4 = 0.5f;
    float sigmaY4 = 4.0f;
    float theta4 = 3.14159f/2; //rotation of sigmaX axis
    kernel4(x, y) = (exp(-((x*cos(theta4) +y*sin(theta4))*(x*cos(theta4) +y*sin(theta4)))
                    /(2*sigmaX4*sigmaX4)) / (sqrtf(2*M_PI)*sigmaX4))
                    *(exp(-((y*cos(theta4) - x*sin(theta4))*(y*cos(theta4) - x*sin(theta4)))
                    /(2*sigmaY4*sigmaY4)) / (sqrtf(2*M_PI)*sigmaY4));


    //Kernel #5
    Func kernel5;
    float sigmaX5 = 4.0f;
    float sigmaY5 = 4.0f;
    float theta5 = 0.0; //rotation of sigmaX axis
    kernel5(x, y) = (exp(-((x*cos(theta5) +y*sin(theta5))*(x*cos(theta5) +y*sin(theta5)))
                    /(2*sigmaX5*sigmaX5)) / (sqrtf(2*M_PI)*sigmaX5))
                    *(exp(-((y*cos(theta5) - x*sin(theta5))*(y*cos(theta5) - x*sin(theta5)))
                    /(2*sigmaY5*sigmaY5)) / (sqrtf(2*M_PI)*sigmaY5));


    //Compute output image plane
    Func image_bounded ("image_bounded");
    image_bounded = BoundaryConditions::repeat_edge(image);


    //Spatially Invariant Implementation 1
/*    Expr blur_image_help = 0.0f;
    Expr norm = 0.0f;
    for(int i = -boundingBox; i <= boundingBox; i++){
        for(int j = -boundingBox; j <= boundingBox; j++){
            blur_image_help += image_bounded(x + i, y + j) * (kernel1(i, j) + kernel2(i, j) +
                                kernel3(i, j) + kernel4(i, j) + kernel5(i, j)); 
            norm += (kernel1(i, j) + kernel2(i, j) + kernel3(i, j) + kernel4(i, j) + kernel5(i, j));
        }
    }
    blur_image_help = blur_image_help/norm;
    Func blurImage ("blurImage");
    blurImage(x, y) = blur_image_help;
*/

    //Spatially Invariant Implementation 2
/*
    Expr blur_image_help1 = 0.0f;
    Expr norm1 = 0.0f;
    for(int i = -boundingBox; i <= boundingBox; i++){
        for(int j = -boundingBox; j <= boundingBox; j++){
            blur_image_help1 += image_bounded(x + i, y + j) * kernel1(i, j); 
            norm1 += kernel1(i, j);
        }
    }
//    blur_image_help1 = blur_image_help1/norm1;
    Func blurImage1 ("blurImage1");
    blurImage1(x, y) = blur_image_help1;

    Expr blur_image_help2 = 0.0f;
    Expr norm2 = 0.0f;
    for(int i = -boundingBox; i <= boundingBox; i++){
        for(int j = -boundingBox; j <= boundingBox; j++){
            blur_image_help2 += image_bounded(x + i, y + j) * kernel2(i, j); 
            norm2 += kernel2(i, j);
        }
    }
//    blur_image_help2 = blur_image_help2/norm2;
    Func blurImage2 ("blurImage2");
    blurImage2(x, y) = blur_image_help2;

    Expr blur_image_help3 = 0.0f;
    Expr norm3 = 0.0f;
    for(int i = -boundingBox; i <= boundingBox; i++){
        for(int j = -boundingBox; j <= boundingBox; j++){
            blur_image_help3 += image_bounded(x + i, y + j) * kernel3(i, j); 
            norm3 += kernel3(i, j);
        }
    }
//    blur_image_help3 = blur_image_help3/norm3;
    Func blurImage3 ("blurImage3");
    blurImage3(x, y) = blur_image_help3;

    Expr blur_image_help4 = 0.0f;
    Expr norm4 = 0.0f;
    for(int i = -boundingBox; i <= boundingBox; i++){
        for(int j = -boundingBox; j <= boundingBox; j++){
            blur_image_help4 += image_bounded(x + i, y + j) * kernel4(i, j); 
            norm4 += kernel4(i, j);
        }
    }
//    blur_image_help4 = blur_image_help4/norm4;
    Func blurImage4 ("blurImage4");
    blurImage4(x, y) = blur_image_help4;

    Expr blur_image_help5 = 0.0f;
    Expr norm5 = 0.0f;
    for(int i = -boundingBox; i <= boundingBox; i++){
        for(int j = -boundingBox; j <= boundingBox; j++){
            blur_image_help5 += image_bounded(x + i, y + j) * kernel5(i, j); 
            norm5 += kernel5(i, j);
        }
    }
//    blur_image_help5 = blur_image_help5/norm5;
    Func blurImage5 ("blurImage5");
    blurImage5(x, y) = blur_image_help5;


    Func blurImage ("blurImage");
//    blurImage(x, y) = (blurImage1(x, y) + blurImage2(x, y) + blurImage3(x, y) +
//                        blurImage4(x, y) + blurImage5(x, y))/(5*norm1);
    blurImage(x, y) = (blur_image_help1 + blur_image_help2 + blur_image_help3 + 
                        blur_image_help4 + blur_image_help5)/(5*norm1);
*/




    //Spatially Variant Implementation 1
    Expr blur_image_help = 0.0f;
    Expr norm = 0.0f;
    for(int i = -boundingBox; i <= boundingBox; i++){
        for(int j = -boundingBox; j <= boundingBox; j++){
            blur_image_help += image_bounded(x + i, y + j) * (polynomial1(x, y)*kernel1(i, j) +
                polynomial2(x, y)*kernel2(i, j) + polynomial3(x, y)*kernel3(i, j) + 
                polynomial4(x, y)*kernel4(i, j) + polynomial5(x, y)*kernel5(i, j)); 
            norm += (polynomial1(x, y)*kernel1(i, j) + polynomial2(x, y)*kernel2(i, j) + 
                polynomial3(x, y)*kernel3(i, j) + polynomial4(x, y)*kernel4(i, j) + 
                polynomial5(x, y)*kernel5(i, j));
        }
    }
    blur_image_help = blur_image_help/norm;
    Func blurImage ("blurImage");
    blurImage(x, y) = blur_image_help;





    //Compute output variance plane
    Func variance_bounded ("variance_bounded");
    variance_bounded = BoundaryConditions::repeat_edge(variance);
    //compute Variance output
    Func blurVariance ("blurVariance");
    Expr blur_variance_help = 0.0f;
    Expr vNorm2 = 0.0f;
    for(int i = -boundingBox; i <= boundingBox; i++){
        for(int j = -boundingBox; j <= boundingBox; j++){
            blur_variance_help += variance_bounded(x + i, y + j) * (polynomial1(x, y)*kernel1(i, j) +
                polynomial2(x, y)*kernel2(i, j) + polynomial3(x, y)*kernel3(i, j) + 
                polynomial4(x, y)*kernel4(i, j) + polynomial5(x, y)*kernel5(i, j))
                *(polynomial1(x, y)*kernel1(i, j) +
                polynomial2(x, y)*kernel2(i, j) + polynomial3(x, y)*kernel3(i, j) + 
                polynomial4(x, y)*kernel4(i, j) + polynomial5(x, y)*kernel5(i, j)); 
            vNorm2 += (polynomial1(x, y)*kernel1(i, j) + polynomial2(x, y)*kernel2(i, j) + 
                polynomial3(x, y)*kernel3(i, j) + polynomial4(x, y)*kernel4(i, j) + 
                polynomial5(x, y)*kernel5(i, j))
                *(polynomial1(x, y)*kernel1(i, j) + polynomial2(x, y)*kernel2(i, j) + 
                polynomial3(x, y)*kernel3(i, j) + polynomial4(x, y)*kernel4(i, j) + 
                polynomial5(x, y)*kernel5(i, j));
        }
    }
//    blur_variance_help = blur_variance_help/(norm(x,y)*norm(x,y));
    blur_variance_help = blur_variance_help/(vNorm2*vNorm2);
    blurVariance(x, y) = blur_variance_help;



    //Compute output mask plane
    Func mask_bounded ("mask_bounded");
    mask_bounded = BoundaryConditions::repeat_edge(mask);

    Func maskOut ("maskOut");

    Expr maskOutHelp = 0;

    for(int i = -boundingBox; i <= boundingBox; i++){
        for(int j = -boundingBox; j <= boundingBox; j++){
            maskOutHelp = select((polynomial1(x, y)*kernel1(i, j) + polynomial2(x, y)*kernel2(i, j) + 
                polynomial3(x, y)*kernel3(i, j) + polynomial4(x, y)*kernel4(i, j) + 
                polynomial5(x, y)*kernel5(i, j)) == 0.0f, maskOutHelp, maskOutHelp | mask_bounded(x + i, y + j));
//            maskOutHelp = maskOutHelp | mask_bounded(x + i, y + j);    
        }
    }
    maskOut(x, y) = maskOutHelp;



    //Schedule
  //  blur.reorder(i_v, x, y);


//    kernel1.compute_at(blurImage, x);
//    kernel1.vectorize(x, 8);
//    kernel1.split(y, y0, yi, 4);
//    kernel1.parallel(y0);

/*    kernel1.compute_root();
    kernel2.compute_root();
    kernel3.compute_root();
    kernel4.compute_root();
    kernel5.compute_root();
*/
    //best schedule found:

#ifdef TESTING_GPU
        blurImage.gpu_tile(x, y, 16, 16);

        // JIT-compile the pipeline for the GPU. CUDA or OpenCL are
        // not enabled by default. We have to construct a Target
        // object, enable one of them, and then pass that target
        // object to compile_jit. Otherwise your CPU will very slowly
        // pretend it's a GPU, and use one thread per output pixel.

        // Start with a target suitable for the machine you're running
        // this on.
        Target target = get_host_target();

        // Then enable OpenCL or CUDA.

        // We'll enable OpenCL here, because it tends to give better
        // performance than CUDA, even with NVidia's drivers, because
        // NVidia's open source LLVM backend doesn't seem to do all
        // the same optimizations their proprietary compiler does.
        target.set_feature(Target::OpenCL);

        // Uncomment the next line and comment out the line above to
        // try CUDA instead.
        // target.set_feature(Target::CUDA);

        // If you want to see all of the OpenCL or CUDA API calls done
        // by the pipeline, you can also enable the Debug
        // flag. This is helpful for figuring out which stages are
        // slow, or when CPU -> GPU copies happen. It hurts
        // performance though, so we'll leave it commented out.
        // target.set_feature(Target::Debug);

        blurImage.compile_jit(target);
#else
        blurImage.split(y, y0, yi, 4);
        blurImage.parallel(y0);
        blurImage.vectorize(x, 8);
#endif

    // Split the y coordinate of the consumer into strips:
    blurVariance.split(y, y0, yi, 4);
    // Compute the strips using a thread pool and a task queue.
    blurVariance.parallel(y0);
    // Vectorize across x.
    blurVariance.vectorize(x, 8);

//    polynomial1.compute_at(blurImage, x).vectorize(x, 8);
//    kernel1.compute_at(blurImage, x).vectorize(x, 8);


    // Split the y coordinate of the consumer into strips of 16 scanlines:
    maskOut.split(y, y0, yi, 30);
    // Compute the strips using a thread pool and a task queue.
    maskOut.parallel(y0);
    // Vectorize across x by a factor of four.
    maskOut.vectorize(x, 8);

//    kernel1.trace_stores();
//    blurImage.trace_stores();




    //Check out what is happening
    blurImage.print_loop_nest();
    // Print out pseudocode for the pipeline.
    blurImage.compile_to_lowered_stmt("linearCombinationKernelBlurImage.html", {image}, HTML);
//    blurImage.compile_to_c("linearCombinationKernel_C_Code.cpp", std::vector<Argument>(), "linearCombinationKernel_C_Code");
//    blurVariance.compile_to_lowered_stmt("blur.html", {variance}, HTML);



    // Benchmark the pipeline.
#ifdef TESTING_GPU
    Buffer image_output(Float(32), image.width(), image.height()); //for GPU testing
#else
    Image<float> image_output(image.width(), image.height());
#endif

    blurImage.realize(image_output);

    Image<float> variance_output(variance.width(), variance.height());
    blurVariance.realize(variance_output);

    Image<int32_t> mask_output(mask.width(), mask.height());
    maskOut.realize(mask_output);

#ifdef TESTING_GPU 
    // Run the filter once to initialize any GPU runtime state.
    blurImage.realize(image_output);

    // Now take the best of 3 runs for timing.
    double best_time;
    for (int i = 0; i < 3; i++) {

        double t1 = current_time();

        // Run the filter 100 times.
        for (int j = 0; j < 100; j++) {
            blurImage.realize(image_output);
        }

        // Force any GPU code to finish by copying the buffer back to the CPU.
        image_output.copy_to_host();

        double t2 = current_time();

        double elapsed = (t2 - t1)/100;
        if (i == 0 || elapsed < best_time) {
            best_time = elapsed;
        }
    }

    printf("%1.4f milliseconds\n", best_time);
#else

	double average = 0;
    double min;
    double max;
    double imgTime;
    double varTime;
    double maskTime;
    int numberOfRuns = 5;
    for (int i = 0; i < numberOfRuns; i++) {
        double t1 = current_time();
        blurImage.realize(image_output);
        double t2 = current_time();
        blurVariance.realize(variance_output);
        double t3 = current_time();
        maskOut.realize(mask_output);
        double t4 = current_time();
        double curTime = (t4-t1);
        average += curTime;
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
    std::cout << "Average Time: " << average << ", Min = " <<
    min << ", Max = " << max << ", with " << numberOfRuns <<
    " runs" << '\n';
    cout << "For fastest run total time = " << min << ", imgTime = " << imgTime << ", varTime = " << varTime << 
    "maskTime = " << maskTime << endl;
#endif



#if !defined(STANDALONE) && !defined(TESTING_GPU)    
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

	imOut.writeFits("./halideLinearCombination5x5.fits");
#endif

}



/*    Func polynomial1 ("polynomial1");
    polynomial1(x, y) = 0.1f + 0.001f*x + 0.001f*y + 0.000001f*x*x + 0.000001f*x*y
                     + 0.000001f*y*y +  0.000000001f*x*x*x + 0.000000001f*x*x*y + 0.000000001f*x*y*y
                     + 0.000000001f*y*y*y;

    //for experimenting with optimizations
    Func polynomial2 ("polynomial2");
    polynomial2(x, y) = 0.1f + 0.001f*x + 0.001f*y + 0.000001f*x*x + 0.000001f*x*y
                     + 0.000001f*y*y +  0.000000001f*x*x*x + 0.000000001f*x*x*y + 0.000000001f*x*y*y
                     + 0.000000001f*y*y*y;

    //for experimenting with optimizations
    Func polynomial3 ("polynomial3");
    polynomial3(x, y) = 0.1f + 0.001f*x + 0.001f*y + 0.000001f*x*x + 0.000001f*x*y
                     + 0.000001f*y*y +  0.000000001f*x*x*x + 0.000000001f*x*x*y + 0.000000001f*x*y*y
                     + 0.000000001f*y*y*y;

    //for experimenting with optimizations
    Func polynomial4 ("polynomial4");
    polynomial4(x, y) = 0.1f + 0.001f*x + 0.001f*y + 0.000001f*x*x + 0.000001f*x*y
                     + 0.000001f*y*y +  0.000000001f*x*x*x + 0.000000001f*x*x*y + 0.000000001f*x*y*y
                     + 0.000000001f*y*y*y;

    //for experimenting with optimizations
    Func polynomial5 ("polynomial5");
    polynomial5(x, y) = 0.1f + 0.001f*x + 0.001f*y + 0.000001f*x*x + 0.000001f*x*y
                     + 0.000001f*y*y +  0.000000001f*x*x*x + 0.000000001f*x*x*y + 0.000000001f*x*y*y
                     + 0.000000001f*y*y*y;
*/
    //Testing different polynomials
/*    Func polynomial1 ("polynomial1");
    polynomial1(x, y) = 1.1f + 2.001f*x + 0.301f*y + 0.00401f*x*x + 0.034501f*x*y
                     + 0.0023451f*y*y +  0.0234534001f*x*x*x + 0.0234500001f*x*x*y + 5.0300001f*x*y*y
                     + 0.000123412000001f*y*y*y;

    //for experimenting with optimizations
    Func polynomial2 ("polynomial2");
    polynomial2(x, y) = 05.1f + 0.0401f*x + 0.4031f*y + 03.06401f*x*x + 04.000001f*x*y
                     + 50.000234001f*y*y +  0.002345340001f*x*x*x + 0.054300001f*x*x*y + 0.03400000001f*x*y*y
                     + 0.0634543001f*y*y*y;

    //for experimenting with optimizations
    Func polynomial3 ("polynomial3");
    polynomial3(x, y) = 3.1f + 0.005431f*x + 0.2345001f*y + 0.2345000001f*x*x + 0.532000001f*x*y
                     + 0.003451f*y*y +  0.0005340001f*x*x*x + 0.023450001f*x*x*y + 235.000000001f*x*y*y
                     + 345.000000001f*y*y*y;

    //for experimenting with optimizations
    Func polynomial4 ("polynomial4");
    polynomial4(x, y) = 30.1f + 0.345001f*x + 0.543001f*y + 0.4567f*x*x + 0.2345000001f*x*y
                     + 0.003453401f*y*y +  0.000657860001f*x*x*x + 0.5342000000001f*x*x*y + 0.2345000000001f*x*y*y
                     + 0.5234000000001f*y*y*y;

    //for experimenting with optimizations
    Func polynomial5 ("polynomial5");
    polynomial5(x, y) = 6.1f + 34.001f*x + 0.543001f*y + 0.34000001f*x*x + 0.534000001f*x*y
                     + 0.345601f*y*y +  0345.000000001f*x*x*x + 0.053400000001f*x*x*y + 0.0003245000001f*x*y*y
                     + 0.0006345001f*y*y*y;

*/


/*    Func polynomial1 ("polynomial1");
    polynomial1(x, y) = 0.1f + 0.001f*x + 0.001f*y + 0.000001f*x*x + 0.000001f*x*y
                     + 0.000001f*y*y +  0.000000001f*x*x*x + 0.000000001f*x*x*y + 0.000000001f*x*y*y
                     + 0.000000001f*y*y*y;

    //for experimenting with optimizations
    Func polynomial2 ("polynomial2");
    polynomial2(x, y) = 1.1f + 1.001f*x + 1.001f*y + 1.000001f*x*x + 1.000001f*x*y
                     + 1.000001f*y*y +  1.000000001f*x*x*x + 1.000000001f*x*x*y + 1.000000001f*x*y*y
                     + 1.000000001f*y*y*y;

    //for experimenting with optimizations
    Func polynomial3 ("polynomial3");
    polynomial3(x, y) = 2.1f + 2.001f*x + 2.001f*y + 2.000001f*x*x + 2.000001f*x*y
                     + 2.000001f*y*y +  2.000000001f*x*x*x + 2.000000001f*x*x*y + 2.000000001f*x*y*y
                     + 2.000000001f*y*y*y;

    //for experimenting with optimizations
    Func polynomial4 ("polynomial4");
    polynomial4(x, y) = 3.1f + 3.001f*x + 3.001f*y + 3.000001f*x*x + 3.000001f*x*y
                     + 3.000001f*y*y +  3.000000001f*x*x*x + 3.000000001f*x*x*y + 3.000000001f*x*y*y
                     + 3.000000001f*y*y*y;

    //for experimenting with optimizations
    Func polynomial5 ("polynomial5");
    polynomial5(x, y) = 4.1f + 4.001f*x + 4.001f*y + 4.000001f*x*x + 4.000001f*x*y
                     + 4.000001f*y*y +  4.000000001f*x*x*x + 4.000000001f*x*x*y + 4.000000001f*x*y*y
                     + 4.000000001f*y*y*y;
*/


    //compute output image and variance
/*    Func polynomial1 ("polynomial1");
    polynomial1(x, y) = 0.1f + 0.001f*x + 0.001f*y + 0.000001f*x*x + 0.000001f*x*y
                     + 0.000001f*y*y +  0.000000001f*x*x*x + 0.000000001f*x*x*y + 0.000000001f*x*y*y
                     + 0.000000001f*y*y*y;

    //for experimenting with optimizations
    Func polynomial2 ("polynomial2");
    polynomial2(x, y) = 0.1f + 1.001f*x + 1.001f*y + 0.000001f*x*x + 0.000001f*x*y
                     + 0.000001f*y*y +  0.000000001f*x*x*x + 0.000000001f*x*x*y + 0.000000001f*x*y*y
                     + 0.000000001f*y*y*y;

    //for experimenting with optimizations
    Func polynomial3 ("polynomial3");
    polynomial3(x, y) = 0.1f + 2.001f*x + 2.001f*y + 0.000001f*x*x + 0.000001f*x*y
                     + 0.000001f*y*y +  0.000000001f*x*x*x + 0.000000001f*x*x*y + 0.000000001f*x*y*y
                     + 0.000000001f*y*y*y;

    //for experimenting with optimizations
    Func polynomial4 ("polynomial4");
    polynomial4(x, y) = 0.1f + 3.001f*x + 3.001f*y + 0.000001f*x*x + 0.000001f*x*y
                     + 0.000001f*y*y +  0.000000001f*x*x*x + 0.000000001f*x*x*y + 0.000000001f*x*y*y
                     + 0.000000001f*y*y*y;

    //for experimenting with optimizations
    Func polynomial5 ("polynomial5");
    polynomial5(x, y) = 0.1f + 4.001f*x + 4.001f*y + 0.000001f*x*x + 0.000001f*x*y
                     + 0.000001f*y*y +  0.000000001f*x*x*x + 0.000000001f*x*x*y + 0.000000001f*x*y*y
                     + 0.000000001f*y*y*y;


    Func polynomial1 ("polynomial1");
    polynomial1(x, y) = 0.1f + 0.002f*x + 0.003f*y + 0.000004f*x*x + 0.000005f*x*y
                     + 0.000006f*y*y +  0.000000007f*x*x*x + 0.000000008f*x*x*y + 0.000000009f*x*y*y
                     + 0.0000000011f*y*y*y;

    //for experimenting with optimizations
    Func polynomial2 ("polynomial2");
    polynomial2(x, y) = 1.1f + 1.002f*x + 1.003f*y + 1.000004f*x*x + 1.000005f*x*y
                     + 1.000006f*y*y +  1.000000007f*x*x*x + 1.000000008f*x*x*y + 1.000000009f*x*y*y
                     + 1.0000000011f*y*y*y;

    //for experimenting with optimizations
    Func polynomial3 ("polynomial3");
    polynomial3(x, y) = 2.1f + 2.002f*x + 2.003f*y + 2.000004f*x*x + 2.000005f*x*y
                     + 2.000006f*y*y +  2.000000007f*x*x*x + 2.000000008f*x*x*y + 2.000000009f*x*y*y
                     + 2.0000000011f*y*y*y;

    //for experimenting with optimizations
    Func polynomial4 ("polynomial4");
    polynomial4(x, y) = 3.1f + 3.002f*x + 3.003f*y + 3.000004f*x*x + 3.000005f*x*y
                     + 3.000006f*y*y +  3.000000007f*x*x*x + 3.000000008f*x*x*y + 3.000000009f*x*y*y
                     + 3.0000000011f*y*y*y;

    //for experimenting with optimizations
    Func polynomial5 ("polynomial5");
    polynomial5(x, y) = 4.1f + 4.002f*x + 4.003f*y + 4.000004f*x*x + 4.000005f*x*y
                     + 4.000006f*y*y +  4.000000007f*x*x*x + 4.000000008f*x*x*y + 4.000000009f*x*y*y
                     + 4.0000000011f*y*y*y;
*/

