
/*
 ./renderText -d data -o /tmp/ -c forwardModel.json \
                -t 1
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <ft2build.h>
#include FT_FREETYPE_H

// Some gdcm libraries
#include "gdcmAttribute.h"
#include "gdcmDefs.h"
#include "gdcmGlobal.h"
#include "gdcmImage.h"
#include "gdcmImageChangeTransferSyntax.h"
#include "gdcmImageHelper.h"
#include "gdcmImageReader.h"
#include "gdcmImageWriter.h"
#include "gdcmPhotometricInterpretation.h"
#include "gdcmSystem.h"
#include <gdcmUIDGenerator.h>

// boost libraries
#include "boost/date_time.hpp"
#include "boost/filesystem.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

// argument parsing
#include "optionparser.h"

// Short alias for this namespace
namespace pt = boost::property_tree;

#define WIDTH 280
#define HEIGHT 28

/* origin is the upper left corner */
unsigned char image[HEIGHT][WIDTH];

/* Replace this function with something useful. */

void draw_bitmap(FT_Bitmap *bitmap, FT_Int x, FT_Int y) {
  FT_Int i, j, p, q;
  FT_Int x_max = x + bitmap->width;
  FT_Int y_max = y + bitmap->rows;

  for (i = x, p = 0; i < x_max; i++, p++) {
    for (j = y, q = 0; j < y_max; j++, q++) {
      if (i < 0 || j < 0 || i >= WIDTH || j >= HEIGHT)
        continue;

      image[j][i] |= bitmap->buffer[q * bitmap->width + p];
    }
  }
}

void show_image(void) {
  int i, j;

  for (i = 0; i < HEIGHT; i++) {
    for (j = 0; j < WIDTH; j++)
      putchar(image[i][j] == 0 ? ' ' : image[i][j] < 64 ? '.' : (image[i][j] < 128 ? '+' : '*'));
    putchar('\n');
  }
}

void show_json(char *c) {
  int i, j;

  int label[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
                 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
                 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
                 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};
  fprintf(stdout, "{ \"label\": [ ");
  int sizeOfLabel = sizeof(label) / sizeof(*label);
  for (int i = 0; i < sizeOfLabel; i++) {
    if (label[i] == (int)c[0])
      fprintf(stdout, "1");
    else
      fprintf(stdout, "0");
    if (i < sizeOfLabel - 1)
      fprintf(stdout, ",");
  }
  fprintf(stdout, " ],\n");

  fprintf(stdout, "  \"image\": [ ");
  for (i = 0; i < HEIGHT; i++) {
    for (j = 0; j < WIDTH; j++) {
      fprintf(stdout, "%d", (int)image[i][j]);
      if (!((i == HEIGHT - 1 && j == WIDTH - 1)))
        fprintf(stdout, ",");
    }
  }
  fprintf(stdout, " ] }\n");
}

struct Arg : public option::Arg {
  static option::ArgStatus Required(const option::Option &option, bool) {
    return option.arg == 0 ? option::ARG_ILLEGAL : option::ARG_OK;
  }
  static option::ArgStatus Empty(const option::Option &option, bool) {
    return (option.arg == 0 || option.arg[0] == 0) ? option::ARG_OK : option::ARG_IGNORE;
  }
};

enum optionIndex { UNKNOWN, FONTFILE, OUTPUT, DICOMS, CONFIG, HELP, TARGETNUM };
const option::Descriptor usage[] = {
    {UNKNOWN, 0, "", "", option::Arg::None,
     "USAGE: renderText [options]\n\n"
     "Options:"},
    {HELP, 0, "", "help", Arg::None, "  --help  \tAdd random text to DICOM files and store again."},
    {DICOMS, 0, "d", "dicom", Arg::Required, "  --dicom, -d  \tDirectory with DICOM files."},
    {OUTPUT, 0, "o", "output", Arg::Required, "  --output, -o  \tOutput directory."},
    {FONTFILE, 0, "f", "fontfile", Arg::Required,
     "  --fontfile, -f  \tName of the font to be used - needs to be available locally."},
    {TARGETNUM, 0, "t", "targetnum", Arg::Required,
     "  --targetnum, -t \tNumber of images to be created."},
    {CONFIG, 0, "c", "config", Arg::Required,
     "  --config, -c \tConfig file to program text generation."},
    {UNKNOWN, 0, "", "", Arg::None,
     "\nExamples:\n"
     "  anonymize --input directory --output directory --infofile /data/bla/info.json -b\n"
     "  anonymize --help\n"},
    {0, 0, 0, 0, 0, 0}};

// get a list of all the DICOM files we should use
std::vector<std::string> listFilesSTD(const std::string &path) {
  std::vector<std::string> files;
  std::string extension;

  for (boost::filesystem::recursive_directory_iterator end, dir(path); dir != end; ++dir) {
    // std::cout << *dir << "\n";  // full path
    if (is_regular_file(dir->path())) {
      // reading zip and tar files might take a lot of time.. filter out here
      extension = boost::filesystem::extension(dir->path());
      if (extension == ".tar" || extension == ".gz" || extension == ".zip" || extension == ".tgz" ||
          extension == ".bz2")
        continue;
      files.push_back(dir->path().c_str());
      if ((files.size() % 200) == 0) {
        fprintf(stdout, "[reading files %05lu]\r", files.size());
      }
    }
  }
  fprintf(stdout, "[reading files %'5lu done]\r", files.size());

  return files;
}

// It might be better to use a markov chain model at least here
// this works if we use https://github.com/jsvine/markovify and
// a corpus of Norwegian text. Preferably we would use text that
// actively is added to images - so some technical text including
// medical and anatomical words.
std::string generateRandomText(int len) {
  std::string tmp_s;
  static const char alphanum[] = "0123456789"
                                 "      /_-."
                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                 "abcdefghijklmnopqrstuvwxyz";

  tmp_s.reserve(len);

  for (int i = 0; i < len; ++i)
    tmp_s += alphanum[std::rand() % (sizeof(alphanum) - 1)];

  return tmp_s;
}

int main(int argc, char **argv) {
  setlocale(LC_NUMERIC, "en_US.utf-8");
  // std::srand(std::time(0));
  std::srand((unsigned)time(NULL) * getpid());

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

  std::string dicom_path = "";
  std::string configfile_path = "";
  std::string output = ""; // directory path
  std::string font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
  int target = 1;

  for (option::Option *opt = options[UNKNOWN]; opt; opt = opt->next())
    std::cout << "Unknown option: " << std::string(opt->name, opt->namelen) << "\n";

  for (int i = 0; i < parse.optionsCount(); ++i) {
    option::Option &opt = buffer[i];
    // fprintf(stdout, "Argument #%d is ", i);
    switch (opt.index()) {
    case HELP:
      // not possible, because handled further above and exits the program
    case DICOMS:
      if (opt.arg) {
        // fprintf(stdout, "--input '%s'\n", opt.arg);
        dicom_path = opt.arg;
      } else {
        fprintf(stdout, "--dicoms needs a directory specified\n");
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
    case FONTFILE:
      if (opt.arg) {
        // fprintf(stdout, "--output '%s'\n", opt.arg);
        font_path = opt.arg;
      } else {
        fprintf(stdout, "--fontfile needs a font file specified\n");
        exit(-1);
      }
      break;
    case TARGETNUM:
      if (opt.arg) {
        // fprintf(stdout, "--output '%s'\n", opt.arg);
        target = std::stoi(opt.arg);
      } else {
        fprintf(stdout, "--targetnum needs a number as an argument\n");
        exit(-1);
      }
      break;
    case CONFIG:
      if (opt.arg) {
        // fprintf(stdout, "--output '%s'\n", opt.arg);
        configfile_path = opt.arg;
      } else {
        fprintf(stdout, "--config needs a file name specified\n");
        exit(-1);
      }
      break;
    case UNKNOWN:
      // not possible because Arg::Unknown returns ARG_ILLEGAL
      // which aborts the parse with an error
      break;
    }
  }
  std::string dn = font_path;
  struct stat buf;
  if (!(stat(dn.c_str(), &buf) == 0)) {
    fprintf(stderr,
            "Error: no font provided. Use the -f option and provide the name of a ttf file.\n");
    exit(-1);
  }

  // we should parse the config file
  pt::ptree root;
  bool configFileExists = false;
  dn = configfile_path;
  struct stat stat_buffer;
  if (!(stat(dn.c_str(), &stat_buffer) == 0)) {
    configFileExists = false;
  } else {
    configFileExists = true;
  }
  if (configFileExists) {
    pt::read_json(configfile_path.c_str(), root);
    fprintf(stdout, "Found model in config file \"%s\"\n\n",
            root.get_child("model").get_value<std::string>().c_str());
    fprintf(stdout, "%s\n", root.get_child("description").get_value<std::string>().c_str());
    //
    // Check for a random font
    //
    std::vector<std::string> font_paths;
    if (font_path == "") {
      auto bounds = root.get_child("logic.font").equal_range("");
      for (auto iter = bounds.first; iter != bounds.second; ++iter) {
        std::string fname = iter->second.get_child("name").get_value<std::string>();
        font_paths.push_back(fname);
      }
      if (font_paths.size() > 0) {
        int idx = std::rand() % font_paths.size();
        font_path = font_paths[idx]; // use a random font
        fprintf(stdout, "Selected a font [%d] from the config file (%lu font%s found).\n", idx,
                font_paths.size(), font_paths.size() > 1 ? "s" : "");
      }
    }
    if (font_path == "") {
      fprintf(stderr,
              "Error: no font path specified in either config file or on the command line.");
      exit(-1);
    }
    //
    // next we can look for a random color
    //
  }
  // we should find all DICOM files
  fprintf(stdout, "\n");
  std::vector<std::string> files = listFilesSTD(dicom_path);
  fprintf(stdout, "\n");
  // fprintf(stdout, "found %'ld files\n", files.size());

  // We will generate images randomly --- but we could generate one for each as well.

  FT_Library library;

  double angle;
  int target_height;
  int n, num_chars;
  char *json;

  //  if (argc < 3) {
  //    fprintf(stderr, "usage: %s font_file.ttf sample-text json|text\n", argv[0]);
  //    exit(1);
  //  }

  int font_length = 20;
  const char *filename = font_path.c_str(); /* first argument     */
  gdcm::ImageReader reader;

  for (int i = 0; i < target; i++) {
    FT_Face face;
    FT_GlyphSlot slot;
    FT_Matrix matrix; /* transformation matrix */
    FT_Vector pen;    /* untransformed origin  */
    FT_Error error;

    // the data is written into image so we need to clear it first before writing again in this loop
    memset(image, 0, HEIGHT * WIDTH);

    //
    // lets read in a random DICOM image (check that it is a DICOM image...)
    //
    bool foundDICOM = false;
    int pickImageIdx;
    int maxIter = 10;
    while (maxIter > 0) {
      pickImageIdx = std::rand() % files.size();
      reader.SetFileName(files[pickImageIdx].c_str());
      fprintf(stdout, "Try to read \"%s\"\n", files[pickImageIdx].c_str());
      if (!reader.Read()) {
        maxIter--;
        fprintf(stdout, "read failed... try next\n");
        continue; // try again
      }
      foundDICOM = true;
      break;
    }
    if (!foundDICOM) {
      fprintf(stderr, "Error: I tried to find a DICOM image I could read. I could not... ");
      exit(-1);
    } else {
      fprintf(stdout, "  DICOM file: \"%s\"\n", files[pickImageIdx].c_str());
    }
    const gdcm::Image &iimage = reader.GetImage();
    // we should probably change the transfer syntax here, uncompress JPEG2000 so that
    // we can write out the image pair uncompressed...
    gdcm::ImageChangeTransferSyntax change;
    // change.SetTransferSyntax(gdcm::TransferSyntax::JPEG2000Lossless);
    // change.SetTransferSyntax(gdcm::TransferSyntax::JPEGLosslessProcess14_1);
    change.SetTransferSyntax(gdcm::TransferSyntax::ImplicitVRLittleEndian);
    change.SetInput(iimage);
    bool b = change.Change();
    if (!b) {
      std::cerr << "Could not change the Transfer Syntax" << std::endl;
      return 1;
    }
    // unsigned int n_dim = image.GetNumberOfDimensions();
    // const unsigned int *dims = image.GetDimensions();
    // Origin
    // const double *origin = image.GetOrigin();
    // const gdcm::PhotometricInterpretation &pl = image.GetPhotometricInterpretation();

    gdcm::File &file = reader.GetFile();
    gdcm::DataSet &ds = file.GetDataSet();
    const gdcm::Image &change_image = change.GetOutput();
    gdcm::PixelFormat pf = change_image.GetPixelFormat();
    unsigned short pixelsize = pf.GetPixelSize();
    fprintf(stdout, "  pixelsize of input is: %d\n", pixelsize);
    unsigned long len = change_image.GetBufferLength();
    // fprintf(stdout, "Found buffer of length: %ld\n", len);
    char *buffer = new char[len];

    change_image.GetBuffer(buffer);

    char *text = strdup(generateRandomText(font_length).c_str()); /* second argument    */
    json = (char *)"text";
    fprintf(stdout, "  text is now: \"%s\" %s\n", text, pf.GetScalarTypeAsString());
    // if (argc == 4) {
    //  json = argv[3];
    //}
    num_chars = strlen(text);
    angle = 0; // ( 25.0 / 360 ) * 3.14159 * 2;      /* use 25 degrees     */
    target_height = HEIGHT;

    error = FT_Init_FreeType(&library); /* initialize library */
    /* error handling omitted */
    if (error != 0) {
      fprintf(stderr, "Error: The freetype libbrary could not be initialized with this font.\n");
      exit(-1);
    }

    error = FT_New_Face(library, filename, 0, &face); /* create face object */
    /* error handling omitted */
    if (face == NULL) {
      fprintf(stderr, "Error: no face found, provide the filename of a ttf file...\n");
      exit(-1);
    }

    /* use 50pt at 100dpi
    FT_F26Dot6  char_width,
                FT_F26Dot6  char_height,
                FT_UInt     horz_resolution,
                FT_UInt     vert_resolution
    */
    error = FT_Set_Char_Size(face, 20 * 64, 0, 90, 0); /* set character size */
    /* error handling omitted */
    if (error != 0) {
      fprintf(stdout, "we have an error here!\n");
    }

    slot = face->glyph;

    /* set up matrix */
    matrix.xx = (FT_Fixed)(cos(angle) * 0x10000L);
    matrix.xy = (FT_Fixed)(-sin(angle) * 0x10000L);
    matrix.yx = (FT_Fixed)(sin(angle) * 0x10000L);
    matrix.yy = (FT_Fixed)(cos(angle) * 0x10000L);

    /* the pen position in 26.6 cartesian space coordinates; */
    /* start at (300,200) relative to the upper left corner  */
    pen.x = 1 * 64;
    pen.y = (target_height - 20) * 64;

    for (n = 0; n < num_chars; n++) {
      /* set transformation */
      FT_Set_Transform(face, &matrix, &pen);

      /* load glyph image into the slot (erase previous one) */
      error = FT_Load_Char(face, text[n], FT_LOAD_RENDER);
      if (error)
        continue; /* ignore errors */

      /* now, draw to our target surface (convert position) */
      draw_bitmap(&slot->bitmap, slot->bitmap_left, target_height - slot->bitmap_top);

      /* increment pen position */
      pen.x += slot->advance.x;
      pen.y += slot->advance.y;
    }

    if (strcmp(json, "text") == 0) {
      show_image();
    } else {
      show_json(text);
    }
    FT_Done_Face(face);

    //
    // write the image pair
    //
    dn = output;
    // struct stat buf;
    if (!(stat(dn.c_str(), &buf) == 0)) {
      mkdir(dn.c_str(), 0777);
    }

    dn = output + "/with_text/";
    // struct stat buf;
    if (!(stat(dn.c_str(), &buf) == 0)) {
      mkdir(dn.c_str(), 0777);
    }
    dn = output + "/without_text/";
    if (!(stat(dn.c_str(), &buf) == 0)) {
      mkdir(dn.c_str(), 0777);
    }

    char outputfilename[1024];

    gdcm::ImageWriter writer2;
    writer2.SetImage(change.GetOutput());
    writer2.SetFile(reader.GetFile());
    snprintf(outputfilename, 1024 - 1, "%s/without_text/%08d.dcm", output.c_str(), i);
    writer2.SetFileName(outputfilename);
    if (!writer2.Write()) {
      return 1;
    }

    // image dimensions
    std::vector<unsigned int> extent = gdcm::ImageHelper::GetDimensionsValue(reader.GetFile());

    unsigned short xmax = (unsigned short)extent[0];
    unsigned short ymax = (unsigned short)extent[1];
    fprintf(stdout, "dimensions: %d %d, len: %lu\n", xmax, ymax, len);

    // now copy the string in
    signed short *bvals = (signed short *)buffer;
    for (int yi = 0; yi < HEIGHT; yi++) {
      for (int xi = 0; xi < WIDTH; xi++) {
        if (image[yi][xi] == 0)
          continue;
        // I would like to copy the value from image over to
        // the buffer. At some good location...
        int px = 100;
        int py = 100;
        int newx = px + xi;
        int newy = py + yi;
        int idx = newx * ymax + newy;
        if (newx < 0 || newx >= xmax || newy < 0 || newy >= ymax)
          continue;
        float f = 0.5;
        float v = (f * bvals[idx]) + ((1.0 - f) * ((1.0 * image[yi][xi]) / 255.0 * 4095.0));
        // fprintf(stdout, "%d %d: %d\n", xi, yi, bvals[idx]);
        bvals[idx] = (signed short)std::max(0.0f, std::min(4095.0f, v));
        // fprintf(stdout, "%d %d: %d\n", xi, yi, bvals[idx]);
      }
    }

    // now we can add the bitmap to the original data and write again
    // change_image.SetBuffer(buffer);
    // gdcm::DataElement pixeldata = change_image.GetDataElement();
    gdcm::DataElement pixeldata(gdcm::Tag(0x7fe0, 0x0010));
    pixeldata.SetByteValue(buffer, len);

    delete[] buffer;
    gdcm::SmartPointer<gdcm::Image> im = new gdcm::Image;
    im->SetNumberOfDimensions(2);
    im->SetDimension(0, xmax);
    im->SetDimension(1, ymax);
    im->SetPhotometricInterpretation(change_image.GetPhotometricInterpretation());
    im->GetPixelFormat().SetSamplesPerPixel(1);
    im->GetPixelFormat().SetBitsAllocated(change_image.GetPixelFormat().GetBitsAllocated());
    im->GetPixelFormat().SetBitsStored(change_image.GetPixelFormat().GetBitsStored());
    im->GetPixelFormat().SetHighBit(change_image.GetPixelFormat().GetHighBit());
    im->GetPixelFormat().SetPixelRepresentation(
        change_image.GetPixelFormat().GetPixelRepresentation());

    // gdcm::Image im = change_image;
    ds = reader.GetFile().GetDataSet();
    im->SetDataElement(pixeldata);
    gdcm::UIDGenerator uid;
    gdcm::Attribute<0x0008, 0x18> ss;
    ss.SetValue(uid.Generate());
    ds.Replace(ss.GetAsDataElement());

    gdcm::Attribute<0x0020, 0x000e> ss2;
    ss2.SetValue(uid.Generate());
    ds.Replace(ss2.GetAsDataElement());

    gdcm::ImageWriter writer;
    writer.SetImage(*im);
    writer.SetFile(reader.GetFile());
    snprintf(outputfilename, 1024 - 1, "%s/with_text/%08d.dcm", output.c_str(), i);
    writer.SetFileName(outputfilename);
    if (!writer.Write()) {
      return 1;
    }
  }
  FT_Done_FreeType(library);

  return 0;
}

/* EOF */