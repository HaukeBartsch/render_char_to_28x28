
/*
 ./renderText -d data -o /tmp/ -c forwardModel.json \
                -t 1
*/

/*

#define png_infopp_NULL (png_infopp)NULL
#define int_p_NULL (int*)NULL
#include <boost/gil/gil_all.hpp>
#include <boost/gil/extension/io/png_dynamic_io.hpp>
using namespace boost::gil;
int main()
{
    rgb8_image_t img(512, 512);
    rgb8_pixel_t red(255, 0, 0);
    fill_pixels(view(img), red);
    png_write_view("redsquare.png", const_view(img));
}

*/


#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fstream>
#include <codecvt>
#include <filesystem>
//#include <stxutif.h>

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
#include "json.hpp"
#include "optionparser.h"

// export as an image
#include <boost/gil.hpp>
#include <boost/gil/extension/io/png.hpp>
#include <boost/gil/extension/dynamic_image/any_image.hpp>
#include <boost/gil/image_view.hpp>


#include <boost/mpl/vector.hpp>
#include <boost/gil/typedefs.hpp>
#include <boost/gil/extension/dynamic_image/any_image.hpp>
#include <boost/gil/image_view.hpp>
#include <boost/gil/planar_pixel_reference.hpp>
#include <boost/gil/color_convert.hpp>
#include <boost/gil/typedefs.hpp>
#include <boost/gil/image_view_factory.hpp>


//#include <boost/gil/extension/io/png_io.hpp>
//#include <boost/gil/extension/io/png_dynamic_io.hpp>
using namespace boost::gil;


// Short alias for this namespace
namespace pt = boost::property_tree;
using json = nlohmann::json;

#define WIDTH 280
#define HEIGHT 28

/* origin is the upper left corner */
unsigned char image_buffer[HEIGHT][WIDTH];

/* Replace this function with something useful. */

void draw_bitmap(FT_Bitmap *bitmap, FT_Int x, FT_Int y) {
  FT_Int i, j, p, q;
  FT_Int x_max = x + bitmap->width;
  FT_Int y_max = y + bitmap->rows;

  for (i = x, p = 0; i < x_max; i++, p++) {
    for (j = y, q = 0; j < y_max; j++, q++) {
      if (i < 0 || j < 0 || i >= WIDTH || j >= HEIGHT)
        continue;

      image_buffer[j][i] |= bitmap->buffer[q * bitmap->width + p];
    }
  }
}

void show_image(void) {
  int i, j;

  for (i = 0; i < HEIGHT; i++) {
    for (j = 0; j < WIDTH; j++)
      putchar(image_buffer[i][j] == 0 ? ' ' : image_buffer[i][j] < 64 ? '.' : (image_buffer[i][j] < 128 ? '+' : '*'));
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
      fprintf(stdout, "%d", (int)image_buffer[i][j]);
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

enum optionIndex { UNKNOWN, FONTFILE, OUTPUT, DICOMS, CONFIG, HELP, TARGETNUM, EXPORTTYPE };
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
    {EXPORTTYPE, 0, "e", "export_type", Arg::Required,
     "  --export_type, -e \tFile format for output (dcm by default, or png)."},
    {UNKNOWN, 0, "", "", Arg::None,
     "\nExamples:\n"
     "  ./renderText -d data/LIDC-IDRI-0006/01-01-2000-92500/3000556-20957/ -o /tmp/bla -c "
     "forwardModel.json\n"
     "  ./renderText --help\n"},
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
std::vector<std::u32string> generateRandomText(int len) {
  std::vector<std::u32string> tmp_s;
  // static const unsigned char alphanum[] =
  //    "0123456789      /_-.ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

  // std::string unicodeChars = std::string(
  //    u8"0123456789      /_-.ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzøæåöüä");

  std::vector<std::u32string> unicodeChars = {
      U"0", U"1", U"2", U"3", U"4", U"5", U"6", U"7", U"8", U"9", U" ", U" ", U" ", U" ",
      U" ", U" ", U"/", U"_", U"-", U".", U"A", U"B", U"C", U"D", U"E", U"F", U"G", U"H",
      U"I", U"J", U"K", U"L", U"M", U"N", U"O", U"P", U"Q", U"R", U"S", U"T", U"U", U"V",
      U"W", U"X", U"Y", U"Z", U"a", U"b", U"c", U"d", U"e", U"f", U"g", U"h", U"i", U"j",
      U"k", U"l", U"m", U"n", U"o", U"p", U"q", U"r", U"s", U"t", U"u", U"v", U"w", U"x",
      U"y", U"z", U"ø", U"æ", U"å", U"ö", U"ü", U"ä", U"(", U")", U"]", U"["};

  // tmp_s.reserve(len);

  // for (int i = 0; i < len; ++i)
  //  tmp_s += alphanum[std::rand() % (sizeof(alphanum) - 1)];
  // for (int i = 0; i < len; ++i)
  //  tmp_s += unicodeChars[std::rand() % (unicodeChars.size() - 1)];
  for (int i = 0; i < len; ++i)
    tmp_s.push_back(unicodeChars[std::rand() % (unicodeChars.size() - 1)]);

  // std::cout << tmp_s << std::endl;
  // std::cout << unicodeChars << std::endl;
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
  std::string output = "";    // directory path
  std::string font_path = ""; // "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
  std::string target_type = "dcm";
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
    case EXPORTTYPE:
      if (opt.arg) {
        // fprintf(stdout, "--output '%s'\n", opt.arg);
        target_type = opt.arg;
        if (target_type != "png") {
          target_type = "dcm";
        } else {
          fprintf(stdout, "Detected output type of png\n");
        }
      } else {
        fprintf(stdout, "--target_type needs dcm or png\n");
        exit(-1);
      }
      break;


    case UNKNOWN:
      // not possible because Arg::Unknown returns ARG_ILLEGAL
      // which aborts the parse with an error
      break;
    }
  }
  int font_size = 12;
  int face_index;

  // we should parse the config file
  pt::ptree root;
  bool configFileExists = false;
  std::string dn = configfile_path;
  struct stat stat_buffer;
  if (!(stat(dn.c_str(), &stat_buffer) == 0)) {
    configFileExists = false;
  } else {
    configFileExists = true;
    fprintf(stdout, "  Found config file in: %s\n", configfile_path.c_str());
  }
  std::vector<std::string> font_paths;
  std::vector<std::vector<int>> font_sizes;   // pick a random size from these
  std::vector<std::vector<int>> face_indexes; // if the font has more than one face index
  if (configFileExists) {
    pt::read_json(configfile_path.c_str(), root);
    fprintf(stdout, "Found model in config file \"%s\"\n\n",
            root.get_child("model").get_value<std::string>().c_str());
    fprintf(stdout, "%s\n", root.get_child("description").get_value<std::string>().c_str());
    //
    // Check for a random font
    //
    if (font_path == "") {
      auto bounds = root.get_child("logic").get_child("font").equal_range("");
      for (auto iter = bounds.first; iter != bounds.second; ++iter) {
        std::string fname = iter->second.get_child("name").get_value<std::string>();
        font_paths.push_back(fname);
        // now read the sizes for this font
        auto bounds2 = iter->second.get_child("sizes").equal_range("");
        std::vector<int> this_font_sizes;
        for (auto iter2 = bounds2.first; iter2 != bounds2.second; ++iter2) {
          int val = iter2->second.get_value<int>();
          this_font_sizes.push_back(val);
        }
        font_sizes.push_back(this_font_sizes);
        //
        auto bounds3 = iter->second.get_child("face_indexes").equal_range("");
        std::vector<int> this_face_indexes;
        for (auto iter3 = bounds3.first; iter3 != bounds3.second; ++iter3) {
          int val = iter3->second.get_value<int>();
          this_face_indexes.push_back(val);
        }
        face_indexes.push_back(this_face_indexes);
      }
      if (font_paths.size() > 0) {
        int idx = std::rand() % font_paths.size();
        font_path = font_paths[idx]; // use a random font
        int idx2 = std::rand() % font_sizes[idx].size();
        font_size = font_sizes[idx][idx2];
        int idx3 = std::rand() % face_indexes[idx].size();
        face_index = face_indexes[idx][idx3];
        fprintf(stdout,
                "Selected a font [%d] from the config file (%lu font%s found). Size is %d. Face "
                "index is: %d.\n",
                idx, font_paths.size(), font_paths.size() > 1 ? "s" : "", font_size, face_index);
      }
    }
    if (font_path == "") {
      fprintf(stderr,
              "Error: no font path specified in either config file or on the command line.");
      exit(-1);
    }
  }
  dn = font_path;
  struct stat buf;
  if (!(stat(dn.c_str(), &buf) == 0)) {
    fprintf(stderr,
            "Error: no font provided. Use the -f option and provide the name of a ttf file.\n");
    exit(-1);
  }

  //
  // next we can look for a random placement
  //
  std::vector<std::map<std::string, std::vector<float>>>
      placements; // if the font has more than one face index
  if (configFileExists) {
    pt::read_json(configfile_path.c_str(), root);
    boost::property_tree::ptree d = root.get_child("logic").get_child("placement");
    for (const std::pair<std::string, boost::property_tree::ptree> &p : d) {
      std::map<std::string, std::vector<float>> entry;
      fprintf(stdout, "  Placement: %s\n", p.first.c_str());
      auto bounds = p.second.get_child("x").equal_range("");
      std::vector<float> values;
      for (auto iter = bounds.first; iter != bounds.second; ++iter) {
        // fprintf(stdout, "found an x value of %s\n",
        // iter->second.get_value<std::string>().c_str());
        values.push_back(iter->second.get_value<float>());
      }
      entry.insert(std::pair<std::string, std::vector<float>>("x", values));

      auto bounds2 = p.second.get_child("y").equal_range("");
      std::vector<float> values2;
      for (auto iter2 = bounds2.first; iter2 != bounds2.second; ++iter2) {
        // fprintf(stdout, "found an x value of %s\n",
        // iter2->second.get_value<std::string>().c_str());
        values2.push_back(iter2->second.get_value<float>());
      }
      entry.insert(std::pair<std::string, std::vector<float>>("y", values2));

      auto bounds3 = p.second.get_child("repeat-spacing").equal_range("");
      std::vector<float> values3;
      for (auto iter3 = bounds3.first; iter3 != bounds3.second; ++iter3) {
        // fprintf(stdout, "found an x value of %s\n",
        // iter2->second.get_value<std::string>().c_str());
        values3.push_back(iter3->second.get_value<float>());
      }
      entry.insert(std::pair<std::string, std::vector<float>>("repeat-spacing", values3));

      auto bounds4 = p.second.get_child("how-many").equal_range("");
      std::vector<float> values4;
      for (auto iter4 = bounds4.first; iter4 != bounds4.second; ++iter4) {
        // fprintf(stdout, "found an x value of %s\n",
        // iter2->second.get_value<std::string>().c_str());
        values4.push_back(iter4->second.get_value<float>());
      }
      entry.insert(std::pair<std::string, std::vector<float>>("how-many", values4));

      auto bounds5 = p.second.get_child("lengths").equal_range("");
      std::vector<float> values5;
      for (auto iter5 = bounds5.first; iter5 != bounds5.second; ++iter5) {
        // fprintf(stdout, "found an x value of %s\n",
        // iter2->second.get_value<std::string>().c_str());
        values5.push_back(iter5->second.get_value<float>());
      }
      entry.insert(std::pair<std::string, std::vector<float>>("lengths", values5));

      placements.push_back(entry);
    }
  }
  fprintf(stdout, "  Found %ld placements\n", placements.size());
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
  //char *json;

  //  if (argc < 3) {
  //    fprintf(stderr, "usage: %s font_file.ttf sample-text json|text\n", argv[0]);
  //    exit(1);
  //  }

  int font_length = 20;
 
  // const char *filename = font_path.c_str(); /* first argument     */
  // std::map<std::string, std::vector<bbox_t>> boundingBoxes;
  json boundingBoxes = json::object();

  for (int i = 0; i < target; i++) { // how many images

    json bboxes = json::array(); // all the bounding boxes for the current image
    //
    // for every new DICOM image we randomize some things
    //
    int idx = std::rand() % font_paths.size();
    font_path = font_paths[idx]; // use a random font
    int idx2 = std::rand() % font_sizes[idx].size();
    font_size = font_sizes[idx][idx2];
    int idx3 = std::rand() % face_indexes[idx].size();
    face_index = face_indexes[idx][idx3];
    fprintf(stdout,
            "Selected a font [%d] from the config file (%lu font%s found). Size is %d. Face "
            "index is: %d.\n",
            idx, font_paths.size(), font_paths.size() > 1 ? "s" : "", font_size, face_index);
    const char *filename = font_path.c_str(); /* first argument     */

    FT_Face face;
    FT_GlyphSlot slot;
    FT_Matrix matrix; /* transformation matrix */
    FT_Vector pen;    /* untransformed origin  */
    FT_Error error;
    gdcm::ImageReader reader;

    // the data is written into image so we need to clear it first before writing again in this
    // loop
    memset(image_buffer, 0, HEIGHT * WIDTH);

    //
    // lets read in a random DICOM image (check that it is a DICOM image...)
    //
    bool foundDICOM = false;
    int pickImageIdx;
    int maxIter = 10;
    while (maxIter > 0) {
      pickImageIdx = std::rand() % files.size();
      // to check for DICOM lets use the normal gdcm::Read and CanRead
      gdcm::Reader r;
      r.SetFileName(files[pickImageIdx].c_str());
      if (r.CanRead() != true) {
        maxIter--;
        fprintf(stdout, "read failed [%d] for \"%s\"... try next\n", maxIter,
                files[pickImageIdx].c_str());
        continue; // try again
      }

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
      // bbox.insert(std::pair<std::string, std::string>("filename", files[pickImageIdx].c_str()));
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

    gdcm::File &file = reader.GetFile();
    gdcm::DataSet &ds = file.GetDataSet();
    const gdcm::Image &change_image = change.GetOutput();
    const gdcm::PhotometricInterpretation &pi = change_image.GetPhotometricInterpretation();
    // We need to check what the phometric interpretation is. We cannot handle all of them.
    if (pi != gdcm::PhotometricInterpretation::MONOCHROME2) {
      fprintf(stderr, "Warning: We only understand MONOCHROME2, skip this image.\n");
      // we could change the interpreation as well
      /*        gdcm::ImageChangePhotometricInterpretation icpi;
         icpi.SetInput(image);
         icpi.SetPhotometricInterpretation(gdcm::PhotometricInterpretation::MONOCHROME2);
         if (!icpi.Change())
         {
           itkExceptionMacro(<< "Failed to change to Photometric Interpretation");
         }
         itkWarningMacro(<< "Converting from MONOCHROME1 to MONOCHROME2 may impact the meaning of
         DICOM attributes related " "to pixel values."); image = icpi.GetOutput(); */

      continue;
    }

    gdcm::PixelFormat pf = change_image.GetPixelFormat();
    // pf could be gdcm::PixelFormat::INT8, or INT16, etc...
    // We need to change our cast further down on the pixel data.

    unsigned short pixelsize = pf.GetPixelSize();
    fprintf(stdout, "  pixelsize of input is: %d\n", pixelsize);
    unsigned long len = change_image.GetBufferLength();
    // fprintf(stdout, "Found buffer of length: %ld\n", len);
    char *buffer = new char[len];

    change_image.GetBuffer(buffer);

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
    unsigned short xmax;
    unsigned short ymax;

    // image dimensions
    std::vector<unsigned int> extent = gdcm::ImageHelper::GetDimensionsValue(reader.GetFile());

    xmax = (unsigned short)extent[0];
    ymax = (unsigned short)extent[1];
    // fprintf(stdout, "  dimensions: %d %d, len: %lu\n", xmax, ymax, len);

    // lets do all the text placements
    for (int placement = 0; placement < placements.size(); placement++) {
      fprintf(stdout, "Use placement: %d\n", placement);
      float vx_min = placements[placement]["x"][0];
      float vx_max = placements[placement]["x"][1];
      float vx = vx_min + ((rand() * 1.0f) / (1.0f * RAND_MAX)) * (vx_max - vx_min);
      float vy_min = placements[placement]["y"][0];
      float vy_max = placements[placement]["y"][1];
      float vy = vy_min + ((rand() * 1.0f) / (1.0f * RAND_MAX)) * (vy_max - vy_min);
      int start_px, start_py;
      bool neg_x = false;
      bool neg_y = false;
      if (vx >= 0) {
        start_px = std::floor(xmax * vx);
      } else {
        start_px = xmax - std::floor(xmax * -vx);
        neg_x = true;
      }
      if (vy >= 0) {
        start_py = std::floor(ymax * vy);
      } else {
        start_py = ymax - std::floor(ymax * -vy);
        neg_y = true;
      }
      // fprintf(stdout, "     %d %d\n", start_px, start_py);
      int howmany_min = placements[placement]["how-many"][0];
      int howmany_max = placements[placement]["how-many"][1];
      int howmany = (rand() % (howmany_max - howmany_min)) + howmany_min;

      float repeat_spacingx = placements[placement]["repeat-spacing"][0];
      float repeat_spacingy = placements[placement]["repeat-spacing"][1];
      float repeat_spacing = repeat_spacingx + (1.0 * rand() / (1.0f * RAND_MAX)) *
                                                   (repeat_spacingy - repeat_spacingx);
      // fprintf(stdout, "repeat_spacing here is: %f (%f .. %f), repeat is: %d\n", repeat_spacing,
      //        repeat_spacingx, repeat_spacingy, howmany);
      // generate text more than once here
      for (int text_lines = 0; text_lines < howmany; text_lines++) {
        memset(image_buffer, 0, HEIGHT * WIDTH);

        int px = start_px;
        int py = start_py + (neg_y ? -1 : 1) * (text_lines * font_size +
                                                text_lines * (repeat_spacing * 0.5 * font_size));

        int lengths_min = placements[placement]["lengths"][0];
        int lengths_max = placements[placement]["lengths"][1];
        font_length = (rand() % (lengths_max - lengths_min)) + lengths_min;
        // fprintf(stdout, "FONTLENGTH IS: %d\n", font_length);

        std::vector<std::u32string> text2print = generateRandomText(font_length);
        // char *text = strdup(text2print.c_str()); // print the text (no utf-8 here)
        // json = (char *)"text";
        // fprintf(stdout, "  text is now: \"%s\" ScalarType in DICOM: %s\n", text,
        //        pf.GetScalarTypeAsString());
        // if (argc == 4) {
        //  json = argv[3];
        //}
        num_chars = text2print.size(); // strlen(text);
        angle = 0; // ( 25.0 / 360 ) * 3.14159 * 2;      /* use 25 degrees     */
        target_height = HEIGHT;

        error = FT_Init_FreeType(&library); /* initialize library */
        /* error handling omitted */
        if (error != 0) {
          fprintf(stderr,
                  "Error: The freetype libbrary could not be initialized with this font.\n");
          exit(-1);
        }

        error = FT_New_Face(library, filename, face_index, &face); /* create face object */
        /* error handling omitted */
        if (face == NULL) {
          fprintf(stderr, "Error: no face found, provide the filename of a ttf file...\n");
          exit(-1);
        }
        // fprintf(stdout, "  number of face_index: %ld\n", face->num_faces);

        /* use 50pt at 100dpi
        FT_F26Dot6  char_width, in 1/64thh of points
                    FT_F26Dot6  char_height, in 1/64th of points
                    FT_UInt     horz_resolution, device resolution
                    FT_UInt     vert_resolution, device resolution
        */
        float font_size_in_pixel = font_size;
        error = FT_Set_Char_Size(face, font_size_in_pixel * 64, 0, 96, 0); /* set character size */
        /* error handling omitted */
        if (error != 0) {
          fprintf(stdout, "Error: FT_Set_Char_Size returned error.\n");
          exit(-1);
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
          // unsigned long c = FT_Get_Char_Index(face, text2print[n]);
          // error = FT_Load_Glyph(face, c, FT_LOAD_RENDER);

          error = FT_Load_Char(face, text2print[n][0], FT_LOAD_RENDER);
          if (error)
            continue; /* ignore errors */

          /* now, draw to our target surface (convert position) */
          draw_bitmap(&slot->bitmap, slot->bitmap_left, target_height - slot->bitmap_top);

          /* increment pen position */
          pen.x += slot->advance.x;
          pen.y += slot->advance.y;
        }

        // if (strcmp(json, "text") == 0) {
        //  show_image();
        //} // else {
        //  show_json(text);
        //}
        FT_Done_Face(face);

        // now copy the string in
        signed short *bvals = (signed short *)buffer;
        std::vector<int> boundingBox = {INT_MAX, INT_MAX, 0, 0}; // xmin, ymin, xmax, ymax
        for (int yi = 0; yi < HEIGHT; yi++) {
          for (int xi = 0; xi < WIDTH; xi++) {
            if (image_buffer[yi][xi] == 0)
              continue;
            // I would like to copy the value from image over to
            // the buffer. At some good location...
            int newx = px + xi;
            int newy = py + yi;
            int idx = newy * xmax + newx;
            if (newx < 0 || newx >= xmax || newy < 0 || newy >= ymax)
              continue;
            float f = 0.5;
            if (image_buffer[yi][xi] == 0)
              continue;
            if (newx < boundingBox[0])
              boundingBox[0] = newx;
            if (newy < boundingBox[1])
              boundingBox[1] = newy;
            if (newy >= boundingBox[3])
              boundingBox[3] = newy;
            if (newx >= boundingBox[2])
              boundingBox[2] = newx;

            float v = (f * bvals[idx]) + ((1.0 - f) * ((1.0 * image_buffer[yi][xi]) / 255.0 * 4095.0));
            // fprintf(stdout, "%d %d: %d\n", xi, yi, bvals[idx]);
            bvals[idx] = (signed short)std::max(0.0f, std::min(4095.0f, v));
            // fprintf(stdout, "%d %d: %d\n", xi, yi, bvals[idx]);
          }
        }
        // we have a bounding box now for the text on this picture
        fprintf(stdout, "  bounding box: %d %d %d %d\n", boundingBox[0], boundingBox[1],
                boundingBox[2], boundingBox[3]);
        if (boundingBox[0] == INT_MAX) {
          // nothing got written, empty bounding box, don't safe
          continue;
        }
        // bbox.insert(files[pickImageIdx].c_str()
        json bbox = json::object();
        std::ostringstream o;
        std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
        for (int a = 0; a < text2print.size(); a++) {
          o << conv.to_bytes(text2print[a]);
          //sprintf(str_buf, "%s%n", str_buf, text2print[a].c_str());
          // bbox.name += text2print[a].c_str();
        }
        //bbox["text"] = json::parse(text2print);
        bbox["name"] = std::string(o.str());
        bbox["xmin"] = boundingBox[0];
        bbox["ymin"] = boundingBox[1];
        bbox["xmax"] = boundingBox[2];
        bbox["ymax"] = boundingBox[3];
        bbox["width"] = xmax;
        bbox["height"] = ymax;
        bbox["filename"] = std::filesystem::path(files[pickImageIdx]).filename();
        bboxes[bboxes.size()] = bbox;

        // boundingBoxes.insert(std::pair<std::string, std::map<std::string, std::string>>(
        //    (files[pickImageIdx] + bb_count).c_str(), bbox));
      }
    }
    // store all the bounding boxes of the current target image
    boundingBoxes[files[pickImageIdx]] = bboxes;

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
    im->SetSlope(change_image.GetSlope());
    im->SetIntercept(change_image.GetIntercept());

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

  // safe the boundingBoxes dataset now as well as a json
  //std::u32string res = boundingBoxes.dump(4);
  std::locale::global(std::locale(""));
  std::ofstream out(output+"/boundingBoxes.json");
//  std::locale utf8_locale(std::locale(), new utf8cvt<false>);
//  out.imbue(utf8_locale);
  out << std::setw(4) << boundingBoxes << std::endl;
  out.close();

  // test writing png
  rgb8_image_t img(512, 512);
  rgb8_pixel_t red(255, 0, 0);
  fill_pixels(view(img), red);
  write_view("redsquare.png", const_view(img), png_tag{});


  return 0;
}

/* EOF */