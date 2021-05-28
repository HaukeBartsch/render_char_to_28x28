// accept png image and convert to DICOM 16bit

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <filesystem>
#include <numeric>

#include <sys/stat.h>
#include "optionparser.h"

#include <png.h>


#include "boost/date_time.hpp"
#include "boost/filesystem.hpp"

enum modeType { MEAN_STD, KURTOSIS };


int width, height;
png_byte color_type;
png_byte bit_depth;
png_bytep *row_pointers = NULL;
int mask_size = 5;
float std_multiple = 2.0f;
int mode = MEAN_STD;

void read_png_file(const char *filename) {
  FILE *fp = fopen(filename, "rb");

  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png) abort();

  png_infop info = png_create_info_struct(png);
  if(!info) abort();

  if(setjmp(png_jmpbuf(png))) abort();

  png_init_io(png, fp);

  png_read_info(png, info);

  width      = png_get_image_width(png, info);
  height     = png_get_image_height(png, info);
  color_type = png_get_color_type(png, info);
  bit_depth  = png_get_bit_depth(png, info);

  // Read any color_type into 8bit depth, RGBA format.
  // See http://www.libpng.org/pub/png/libpng-manual.txt

  if(bit_depth == 16)
    png_set_strip_16(png);

  if(color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png);

  // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
  if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8(png);

  if(png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);

  // These color_type don't have an alpha channel then fill it with 0xff.
  if(color_type == PNG_COLOR_TYPE_RGB ||
     color_type == PNG_COLOR_TYPE_GRAY ||
     color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

  if(color_type == PNG_COLOR_TYPE_GRAY ||
     color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);

  png_read_update_info(png, info);

  if (row_pointers) abort();

  row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
  for(int y = 0; y < height; y++) {
    row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png,info));
  }

  png_read_image(png, row_pointers);

  fclose(fp);

  png_destroy_read_struct(&png, &info, NULL);
}

void write_png_file(const char *filename) {
  int y;

  FILE *fp = fopen(filename, "wb");
  if(!fp) abort();

  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) abort();

  png_infop info = png_create_info_struct(png);
  if (!info) abort();

  if (setjmp(png_jmpbuf(png))) abort();

  png_init_io(png, fp);

  // Output is 16bit depth, RGBA format.
  png_set_IHDR(
    png,
    info,
    width, height,
    bit_depth,
    PNG_COLOR_TYPE_RGBA,
    PNG_INTERLACE_NONE,
    PNG_COMPRESSION_TYPE_DEFAULT,
    PNG_FILTER_TYPE_DEFAULT
  );
  png_write_info(png, info);

  // To remove the alpha channel for PNG_COLOR_TYPE_RGB format,
  // Use png_set_filler().
  //png_set_filler(png, 0, PNG_FILLER_AFTER);

  if (!row_pointers) abort();

  png_write_image(png, row_pointers);
  png_write_end(png, NULL);

  for(int y = 0; y < height; y++) {
    free(row_pointers[y]);
  }
  free(row_pointers);

  fclose(fp);

  png_destroy_write_struct(&png, &info);
}

inline void fixborder(int *i, int *j, int width, int height) {
  if ((*i) < 0)
    *i = width+(*i);
  if ((*j) < 0)
    *j = height+(*j);
  if (*i >= width) {
    //fprintf(stdout, "FOUND: %d now %d\n", *i, width-((*i) % width));
    *i = width-1-((*i) % width); // (*i) - width - 1;
  }
  if (*j >= height) {
    //fprintf(stdout, "FOUND: %d now %d\n", *j, height-((*j) % height));
    *j = height-1-((*j) % height); // (*i) - width - 1;
  }

/*  if ((*i) < 0 || (*j) < 0)
    fprintf(stderr, "i: %d j: %d\n", *i, *j);
  if ((*i) >= width || (*j) >= height)
    fprintf(stderr, "i: %d j: %d\n", *i, *j); */
}

void process_png_file( char *buf) {
    fprintf(stdout, "start processing the image...\n");
    // we should have enough space in buf to add our values there (width, height, 16bit)
    size_t counter = 0;
    int16_t *b = (int16_t *)buf;
    for (int y = 0; y < height; y++) {
        png_bytep row = row_pointers[y];
        for (int x = 0; x < width; x++) {
            png_bytep px = &(row[x * 4]);
            // Do something awesome for each pixel here...
            b[counter] = (signed short)( (0.3 * px[0]) + (0.59 * px[1]) + (0.11 * px[2]) );
            counter++;
            //printf("%4d, %4d = RGBA(%3d, %3d, %3d, %3d)\n", x, y, px[0], px[1], px[2], px[3]);
        }
    }
    // now enhance the black and white version (use values in buf and write result to image)
    counter = 0;
    std::vector<int16_t> intensities;
    for (int y = 0; y < height; y++) {
        if (y % 10 == 0)  
          fprintf(stdout, "image row: %04d/%d\r", y, height-1); fflush(stdout);
        png_bytep row = row_pointers[y];
        int i, j;
        int idx;
        for (int x = 0; x < width; x++) {
            intensities.resize(0);
            png_bytep px = &(row[x * 4]);
            for (int sx = -mask_size; sx <= mask_size; sx++) {
              for (int sy = -mask_size; sy <= mask_size; sy++) {
                i = x+sx; j = y+sy;
                //fprintf(stdout, "%d %d\t", i, j);
                fixborder(&i,&j,width,height);
                //fprintf(stdout, " fixed: %d %d width: %d height: %d\n", i, j, width, height); fflush(stdout);
                idx = (j) * width + (i); // b[idx00];
                intensities.push_back(b[idx]);
              }
            }

            float sum = std::accumulate(intensities.begin(), intensities.end(), 0.0);
            double mean = sum/intensities.size();
            float sq_sum = std::inner_product(intensities.begin(), intensities.end(), intensities.begin(), 0.0);
            double stdev = std::sqrt( (sq_sum / intensities.size()) - (mean * mean));

            double v = 0.0;
            if (bit_depth == 8) {
              if (mode == MEAN_STD) {
                v = 128 + 128.0*(((double)b[y*width + x] - mean)/(std_multiple*stdev));
                v = std::max((double)0.0, std::min((double)255.0,(double)v));
              } else {
                v = 0.0;
                for (int i = 0; i < intensities.size(); i++) {
                    v += std::pow((intensities[i] - mean) / stdev, 4.0);
                }
                v /= intensities.size();

                v = 128 + 64.0*(v - 3.0);
                v = std::max((double)0.0, std::min((double)255.0,(double)v));
              }
            } else {
              if (mode == MEAN_STD) {
                v = 4096 + 4096.0*(((double)b[y*width + x] - mean)/(std_multiple*stdev));
                v = std::max((double)0.0, std::min((double)4095.0,(double)v));
              } else {
                v = 0.0;
                for (int i = 0; i < intensities.size(); i++) {
                    v += std::pow((intensities[i] - mean) / stdev, 4.0);
                }
                v /= intensities.size();

                v = 4096 + 4096.0*(v - 3.0);
                v = std::max((double)0.0, std::min((double)4095.0,(double)v));
              }
            }
            px[0] = v;
            px[1] = px[0];
            px[2] = px[0];
        }
    }
}

struct Arg : public option::Arg {
  static option::ArgStatus Required(const option::Option &option, bool) {
    return option.arg == 0 ? option::ARG_ILLEGAL : option::ARG_OK;
  }
  static option::ArgStatus Empty(const option::Option &option, bool) {
    return (option.arg == 0 || option.arg[0] == 0) ? option::ARG_OK : option::ARG_IGNORE;
  }
};

enum optionIndex { UNKNOWN, INPUT, OUTPUT, MASK, STD, MODE, HELP };
const option::Descriptor usage[] = {
    {UNKNOWN, 0, "", "", option::Arg::None,
     "Enhances a png images (16bit) to make pixel changes more visible.\n"
     "USAGE: pngEnhance [options]\n\n"
     "Options:"},
    {HELP, 0, "", "help", Arg::None, "  --help  \tPrint this help message."},
    {INPUT, 0, "i", "input", Arg::Required, "  --input, -i  \tInput PNG file."},
    {OUTPUT, 0, "o", "output", Arg::Required, "  --output, -o  \tOutput directory, if not specified the input folder will be used instead."},
    {MASK, 0, "m", "mask_size", Arg::Required, "  --mask_size, -m  \tSize of the gliding window (3, 5, 7)."},
    {STD, 0, "s", "std_multiple", Arg::Required, "  --std_multiple, -m  \tMultiple of the local standard deviation (1, 2, 3, ...)."},
    {MODE, 0, "e", "mode", Arg::Required, "  --mode, -m  \tComputational method (MEAN_STD or KURTOSIS)."},
    {UNKNOWN, 0, "", "", Arg::None,
     "\nExamples:\n"
     "  ./pngEnhance -i data/test.png -o /tmp/\n"
     "  ./pngEnhance --help\n"},
    {0, 0, 0, 0, 0, 0}};


int main(int argc, char **argv) {
  setlocale(LC_NUMERIC, "en_US.utf-8");
  // std::srand(std::time(0));
  // std::srand((unsigned)time(NULL) * getpid());

  argc -= (argc > 0);
  argv += (argc > 0); // skip program name argv[0] if present

  option::Stats stats(usage, argc, argv);
  std::vector<option::Option> options(stats.options_max);
  std::vector<option::Option> buffer(stats.buffer_max);
  option::Parser parse(usage, argc, argv, &options[0], &buffer[0]);

  if (parse.error())
    return 1;

  if (options[HELP] || argc == 0) {
    option::printUsage(std::cout, usage);
    return 0;
  }

  std::string input_path;
  std::string output = "";    // directory path

  bool verbose = false;

  for (option::Option *opt = options[UNKNOWN]; opt; opt = opt->next())
    std::cout << "Unknown option: " << std::string(opt->name, opt->namelen) << "\n";

  for (int i = 0; i < parse.optionsCount(); ++i) {
    option::Option &opt = buffer[i];
    // fprintf(stdout, "Argument #%d is ", i);
    switch (opt.index()) {
    case HELP:
      // not possible, because handled further above and exits the program
      break;
    case INPUT:
      if (opt.arg) {
        // fprintf(stdout, "--input '%s'\n", opt.arg);
        input_path = opt.arg;
      } else {
        fprintf(stdout, "--input needs a png file specified\n");
        exit(-1);
      }
      break;
    case MODE:
      if (opt.arg) {
        // fprintf(stdout, "--input '%s'\n", opt.arg);
        if (strcmp(opt.arg, "MEAN_STD") == 0)
          mode = MEAN_STD;
        else
          mode = KURTOSIS;
      } else {
        fprintf(stdout, "--mode needs to be either MEAN_STD or KURTOSIS\n");
        exit(-1);
      }
      break;
    case MASK:
      if (opt.arg) {
        // fprintf(stdout, "--input '%s'\n", opt.arg);
        mask_size = std::atoi(opt.arg);
      } else {
        fprintf(stdout, "--mask_size needs a number specified (3, 5, 7)\n");
        exit(-1);
      }
      break;
    case STD:
      if (opt.arg) {
        // fprintf(stdout, "--input '%s'\n", opt.arg);
        std_multiple = std::atof(opt.arg);
      } else {
        fprintf(stdout, "--std_multiple needs a number specified (1.0, 2.0, ...)\n");
        exit(-1);
      }
      break;
    case OUTPUT:
      if (opt.arg) {
        // fprintf(stdout, "--output '%s'\n", opt.arg);
        output = opt.arg;
      } else {
        fprintf(stdout, "--output needs a directory specified\n");
        exit(-1);
      }
      break;
    case UNKNOWN:
      // not possible because Arg::Unknown returns ARG_ILLEGAL
      // which aborts the parse with an error
      break;
    }
  }
  if (!std::filesystem::exists(input_path)) {
      fprintf(stderr, "Error: input file \"%s\" does not exist.\n", input_path.c_str());
      exit(-1);
  }

  if (output == "") {
      output = std::filesystem::path(input_path).replace_extension(".png");
  } else {
      // if an output is provided and its only a directory add the name of the input file
      if (std::filesystem::is_directory(std::filesystem::path(output))) {
          output = output + std::string(std::filesystem::path(input_path).filename());
          output = std::filesystem::path(output).replace_extension(".png");
      }
  }
  fprintf(stdout, "Run with:\n\tmask size: %d\n\tstandard deviation multiple: %f\n\tMode: %d\n", mask_size, std_multiple, mode);

  read_png_file(input_path.c_str());

  fprintf(stdout, "width: %d, height: %d, bit depth: %d, color_type (numeric): %d\n", width, height, bit_depth, color_type);
  if (color_type == PNG_COLOR_MASK_COLOR)
    fprintf(stdout, "color type is color\n");
  if (color_type == PNG_COLOR_TYPE_GRAY)
    fprintf(stdout, "color type is gray-scale\n");

  char *buf = new char[width*height*sizeof(int16_t)];
  process_png_file(buf);

  std::string dn = std::filesystem::path(output).parent_path();
  { // create the output folder if it does not exist yet
    struct stat buf;
    if (!(stat(dn.c_str(), &buf) == 0)) {
        mkdir(dn.c_str(), 0777);
    }
  }

  write_png_file(output.c_str());

  fprintf(stdout, "wrote: \"%s\"\n", output.c_str()); fflush(stdout);

  return 0;
}