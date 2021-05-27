// accept png image and convert to DICOM 16bit

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <filesystem>
#include <sys/stat.h>
#include "optionparser.h"

#include <png.h>


#include "boost/date_time.hpp"
#include "boost/filesystem.hpp"


int width, height;
png_byte color_type;
png_byte bit_depth;
png_bytep *row_pointers = NULL;

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

void write_png_file(char *filename) {
  int y;

  FILE *fp = fopen(filename, "wb");
  if(!fp) abort();

  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) abort();

  png_infop info = png_create_info_struct(png);
  if (!info) abort();

  if (setjmp(png_jmpbuf(png))) abort();

  png_init_io(png, fp);

  // Output is 8bit depth, RGBA format.
  png_set_IHDR(
    png,
    info,
    width, height,
    8,
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

void process_png_file( char *buf) {
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
    for (int y = 0; y < height; y++) {
        png_bytep row = row_pointers[y];
        int i, j;
        for (int x = 0; x < width; x++) {
            png_bytep px = &(row[x * 4]);
            i = x+0; j = y+0;
            int idx00 = (i) * width + (j); // b[idx00];

            i = -1; j = 0;
            if (i < 0)            
              i = width-1;
            if (j < 0)
              j = height-1;
            if (i >= width)
              i = i - width;
            if (j >= height)
              j = j - height;
            int idx_00 = (i) * width + (j); // b[idx_00]
      

            int16_t val = 0;

            px[0] = val;
            px[1] = val;
            px[2] = val;
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

enum optionIndex { UNKNOWN, INPUT, OUTPUT, HELP };
const option::Descriptor usage[] = {
    {UNKNOWN, 0, "", "", option::Arg::None,
     "Enhances a png images (16bit) to make pixel changes more visible.\n"
     "USAGE: pngEnhance [options]\n\n"
     "Options:"},
    {HELP, 0, "", "help", Arg::None, "  --help  \tPrint this help message."},
    {INPUT, 0, "i", "input", Arg::Required, "  --input, -i  \tInput PNG file."},
    {OUTPUT, 0, "o", "output", Arg::Required, "  --output, -o  \tOutput directory, if not specified the input folder will be used instead."},
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
      output = std::filesystem::path(input_path).replace_extension(".dcm");
  } else {
      // if an output is provided and its only a directory add the name of the input file
      if (std::filesystem::is_directory(std::filesystem::path(output))) {
          output = output + std::string(std::filesystem::path(input_path).filename());
          output = std::filesystem::path(output).replace_extension(".dcm");
      }
  }

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

  fprintf(stdout, "wrote: \"%s\"\n", output.c_str());

  return 0;
}