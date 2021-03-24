// accept png image and convert to DICOM 16bit

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <filesystem>
#include <sys/stat.h>
#include "optionparser.h"

#include "gdcmAttribute.h"
#include "gdcmDefs.h"
#include "gdcmGlobal.h"
#include "gdcmImage.h"
#include "gdcmImageChangeTransferSyntax.h"
#include "gdcmImageHelper.h"
#include "gdcmImageReader.h"
#include "gdcmImageWriter.h"
#include "gdcmAnonymizer.h"
#include "gdcmPhotometricInterpretation.h"
#include "gdcmSystem.h"
#include <gdcmUIDGenerator.h>

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


void process_png_file( char *buf) {
    // we should have enough space in buf to add our values there (width, height, 16bit)
    size_t counter = 0;
    int16_t *b = (int16_t *)buf;
  for (int y = 0; y < height; y++) {
    png_bytep row = row_pointers[y];
    for (int x = 0; x < width; x++) {
      png_bytep px = &(row[x * 4]);
      // Do something awesome for each pixel here...
      b[counter] = (signed short)((px[0] + px[1] + px[2])/3.0);
      counter++;
      //printf("%4d, %4d = RGBA(%3d, %3d, %3d, %3d)\n", x, y, px[0], px[1], px[2], px[3]);
    }
  }
}


void InitializeGDCMFile( gdcm::File* origianlFilePtr ) {
  gdcm::File* filePtr = new gdcm::File;
  gdcm::Anonymizer anon;
  anon.SetFile( *filePtr );  

  anon.Replace( gdcm::Tag(0x0028,0x0002) , "1" );                       //SamplesperPixel
  anon.Replace( gdcm::Tag(0x0028,0x0004) , "MONOCHROME2" );             //PhotometricInterpretation
  
  char b[1024];
  snprintf(b,1024, "%d", width);
  anon.Replace( gdcm::Tag(0x0028,0x0010) , b );                     //Rows
  snprintf(b,1024, "%d", height);
  anon.Replace( gdcm::Tag(0x0028,0x0011) , b );                     //Columns
  anon.Replace( gdcm::Tag(0x0028,0x0030) , "1\\1" );            //PixelSpacing

  anon.Replace( gdcm::Tag(0x0028,0x1050) , "0" );                        //WindowCenter
  anon.Replace( gdcm::Tag(0x0028,0x1051) , "100" );                      //WindowWidth
  anon.Replace( gdcm::Tag(0x0028,0x1052) , "0" );                        //RescaleIntercept
  anon.Replace( gdcm::Tag(0x0028,0x1053) , "1" );                        //RescaleSlope

  *origianlFilePtr = *filePtr;
}


void InitializeGDCMImage(gdcm::Image* imagePtr, char *buf) {
  imagePtr->SetPhotometricInterpretation( gdcm::PhotometricInterpretation::MONOCHROME2 );

  imagePtr->SetNumberOfDimensions(2);
  unsigned int dims[2]={(unsigned int)width,(unsigned int)height};
  imagePtr->SetDimensions( dims );

  imagePtr->SetSpacing( 0 , 1. );
  imagePtr->SetSpacing( 1 , 1. );

  imagePtr->SetIntercept(0.);
  imagePtr->SetSlope(1.);

  double dirCos[6]={1.,0.,0.,0.,1.,0.};
  imagePtr->SetDirectionCosines( dirCos );

  char *buffer = new char[width*height*sizeof(int16_t)];
  // add values here from png


  imagePtr->SetPixelFormat( gdcm::PixelFormat::INT16 );
  imagePtr->GetDataElement().SetByteValue( (char *)buf/* buffer */ , width*height*sizeof(int16_t) );
  imagePtr->GetPixelFormat().SetSamplesPerPixel(1);
  delete[] buffer;
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
     "Convert png images (16bit) to DICOM with a random header.\n"
     "USAGE: png162dcm [options]\n\n"
     "Options:"},
    {HELP, 0, "", "help", Arg::None, "  --help  \tPrint this help message."},
    {INPUT, 0, "i", "input", Arg::Required, "  --input, -i  \tInput PNG file."},
    {OUTPUT, 0, "o", "output", Arg::Required, "  --output, -o  \tOutput directory, if not specified the input folder will be used instead."},
    {UNKNOWN, 0, "", "", Arg::None,
     "\nExamples:\n"
     "  ./png162dcm -i data/test.png -o /tmp/\n"
     "  ./png162dcm --help\n"},
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

  // now create space for the DICOM image given those dimensions
  gdcm::Image* imagePtr = new gdcm::Image;
  gdcm::File*  filePtr  = new gdcm::File;   


  char *buf = new char[width*height*sizeof(int16_t)];
  process_png_file(buf);

  InitializeGDCMImage(imagePtr, buf);
  InitializeGDCMFile( filePtr );

  std::string dn = std::filesystem::path(output).parent_path();
  { // create the output folder if it does not exist yet
    struct stat buf;
    if (!(stat(dn.c_str(), &buf) == 0)) {
        mkdir(dn.c_str(), 0777);
    }
  }

  gdcm::ImageWriter* writer = new gdcm::ImageWriter; 
  writer->SetFileName(  output.c_str() );
  writer->SetImage(    *imagePtr         );
  writer->SetFile(     *filePtr          );
  if( !writer->Write() ){
    std::cerr << "ERROR: Could not write to \"" << output <<  "\"" << std::endl;
  }
  delete writer;
  fprintf(stdout, "wrote: \"%s\"\n", output.c_str());

  return 0;
}