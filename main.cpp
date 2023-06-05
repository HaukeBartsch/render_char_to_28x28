/*
 ./renderText -d data -o /tmp/ -c forwardModel.json \
                -t 1
*/

// Done: change color of text relative to background
//       may make detection more difficult.. use uniform distribution, maybe exclude intensity of background?
// TODO: there is a gamma correction (use 1.8) needed before using the bitmap alpha value 
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fstream>
#include <codecvt>
#include <filesystem>
#include <exception>
// #include <stxutif.h>

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
// we can also support the xml output
#include <boost/property_tree/xml_parser.hpp>

// argument parsing
#include "json.hpp"
#include "optionparser.h"

// export as an image
#include <boost/gil.hpp>
#include <boost/gil/extension/io/png.hpp>

#include <boost/mpl/vector.hpp>
#include <boost/gil/typedefs.hpp>
#include <boost/gil/extension/dynamic_image/any_image.hpp>
#include <boost/gil/image_view.hpp>
#include <boost/gil/planar_pixel_reference.hpp>
#include <boost/gil/color_convert.hpp>
#include <boost/gil/typedefs.hpp>
#include <boost/gil/image_view_factory.hpp>

#include "markov_chain.h"
using namespace markov_chain_model;


// #include <boost/gil/extension/io/png_io.hpp>
// #include <boost/gil/extension/io/png_dynamic_io.hpp>
using namespace boost::gil;
using namespace boost::filesystem;

// Short alias for this namespace
namespace pt = boost::property_tree;
using json = nlohmann::json;

#define WIDTH 280
#define HEIGHT 28

/* origin is the upper left corner */
unsigned char image_buffer[HEIGHT][WIDTH];

/* switch between global mode for one or two class bounding box generation */
bool twoclass = false;

/* Replace this function with something useful. */

void draw_bitmap(FT_Bitmap *bitmap, FT_Int x, FT_Int y)
{
  FT_Int i, j, p, q;
  FT_Int x_max = x + bitmap->width;
  FT_Int y_max = y + bitmap->rows;

  for (i = x, p = 0; i < x_max; i++, p++)
  {
    for (j = y, q = 0; j < y_max; j++, q++)
    {
      if (i < 0 || j < 0 || i >= WIDTH || j >= HEIGHT)
        continue;

      image_buffer[j][i] |= bitmap->buffer[q * bitmap->width + p];
    }
  }
}

void show_image(void)
{
  int i, j;

  for (i = 0; i < HEIGHT; i++)
  {
    for (j = 0; j < WIDTH; j++)
      putchar(image_buffer[i][j] == 0 ? ' ' : image_buffer[i][j] < 64 ? '.'
                                                                      : (image_buffer[i][j] < 128 ? '+' : '*'));
    putchar('\n');
  }
}

void show_json(char *c)
{
  int i, j;

  int label[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
                 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
                 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
                 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};
  fprintf(stdout, "{ \"label\": [ ");
  int sizeOfLabel = sizeof(label) / sizeof(*label);
  for (int i = 0; i < sizeOfLabel; i++)
  {
    if (label[i] == (int)c[0])
      fprintf(stdout, "1");
    else
      fprintf(stdout, "0");
    if (i < sizeOfLabel - 1)
      fprintf(stdout, ",");
  }
  fprintf(stdout, " ],\n");

  fprintf(stdout, "  \"image\": [ ");
  for (i = 0; i < HEIGHT; i++)
  {
    for (j = 0; j < WIDTH; j++)
    {
      fprintf(stdout, "%d", (int)image_buffer[i][j]);
      if (!((i == HEIGHT - 1 && j == WIDTH - 1)))
        fprintf(stdout, ",");
    }
  }
  fprintf(stdout, " ] }\n");
}

struct Arg : public option::Arg
{
  static option::ArgStatus Required(const option::Option &option, bool)
  {
    return option.arg == 0 ? option::ARG_ILLEGAL : option::ARG_OK;
  }
  static option::ArgStatus Empty(const option::Option &option, bool)
  {
    return (option.arg == 0 || option.arg[0] == 0) ? option::ARG_OK : option::ARG_IGNORE;
  }
};

enum optionIndex
{
  UNKNOWN,
  FONTFILE,
  OUTPUT,
  DICOMS,
  CONFIG,
  HELP,
  TARGETNUM,
  EXPORTTYPE,
  RANDOMSEED,
  VERBOSE,
  BATCH,
  TWOCLASS,
  MARKOV
};
const option::Descriptor usage[] = {
    {UNKNOWN, 0, "", "", option::Arg::None,
     "Annotation detection sample data: Create deep learning data from collections of DICOM files by adding text annotations.\n"
     "USAGE: renderText [options]\n\n"
     "Options:"},
    {HELP, 0, "", "help", Arg::None, "  --help  \tPrint this help message."},
    {DICOMS, 0, "d", "dicom", Arg::Required, "  --dicom, -d  \tDirectory with DICOM files."},
    {OUTPUT, 0, "o", "output", Arg::Required, "  --output, -o  \tOutput directory."},
    {FONTFILE, 0, "f", "fontfile", Arg::Required,
     "  --fontfile, -f  \tName of the font to be used. Also specific in the config file."},
    {TARGETNUM, 0, "t", "targetnum", Arg::Required,
     "  --targetnum, -t \tNumber of image pairs to be created. Images are chosen at random from the DICOM folder."},
    {CONFIG, 0, "c", "config", Arg::Required,
     "  --config, -c \tConfiguration file for text generation."},
    {EXPORTTYPE, 0, "e", "export_type", Arg::Required,
     "  --export_type, -e \tFile format for output (\"dcm\" by default, or \"png\"). Only if the png option is used the PASCAL VOC annotation folder with xml files will be created."},
    {RANDOMSEED, 0, "s", "random_seed", Arg::Required,
     "  --random_seed, -s \tSpecify random seed for text placement (default random)."},
    {BATCH, 0, "b", "batch", Arg::Required,
     "  --batch, -b \tA string prepended to the numbered files to allow for repeated creation of samples without overwrite."},
    {VERBOSE, 0, "v", "verbose", Arg::None,
     "  --verbose, -v \tVerbose output."},
    {MARKOV, 0, "a", "markov", Arg::Required, "  --markov, -a \tA file with domain text. If this option is provided text in images will be generated from a Markov model trained on the text data."},
    {TWOCLASS, 0, "m", "multiclass", Arg::None,
     "  --multiclass, -m \tProvide examples for text (\"text\") and for non-text (\"background\") objects. Default is that only bounding boxes for actual text are generated."},
    {UNKNOWN, 0, "", "", Arg::None,
     "\nExamples:\n"
     "  ./renderText -d data/LIDC-IDRI-0009 -c forwardModel.json \\\n"
     "               -o /tmp/bla -e png -t 100 -v -m\n"
     "  ./renderText --help\n"},
    {0, 0, 0, 0, 0, 0}};

// get a list of all the DICOM files we should use
std::vector<std::string> listFilesSTD(const std::vector<std::string> &paths, bool verbose)
{
  std::vector<std::string> files;
  std::string extension;

  for (int i = 0; i < paths.size(); i++)
  {
    std::string path = paths[i];
    for (boost::filesystem::recursive_directory_iterator end, dir(path); dir != end; ++dir)
    {
      // std::cout << *dir << "\n";  // full path
      if (is_regular_file(dir->path()))
      {
        // reading zip and tar files might take a lot of time.. filter out here
        boost::filesystem::path bpath(dir->path());
        extension = bpath.extension().string();
        if (extension == ".tar" || extension == ".gz" || extension == ".zip" || extension == ".tgz" ||
            extension == ".bz2")
          continue;
        files.push_back(dir->path().c_str());
        if ((files.size() % 2000) == 0 && verbose)
        {
          fprintf(stdout, "[listing file names %'5lu...]\r", files.size());
          fflush(stdout);
        }
      }
    }
  }
  if (verbose)
    fprintf(stdout, "[Listing file names done. %'5lu files found.]\r", files.size());

  return files;
}

// It might be better to use a markov chain model at least here
// this works if we use https://github.com/jsvine/markovify and
// a corpus of Norwegian text. Preferably we would use text that
// actively is added to images - so some technical text including
// medical and anatomical words.
std::vector<std::u32string> generateRandomText(int len)
{
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
  {
    int idx = std::rand() % (unicodeChars.size() - 1);
    while ((i == 0 || i == len - 1) && unicodeChars[idx] == U" ")
    {
      // I don't accept that
      idx = std::rand() % (unicodeChars.size() - 1); // try again
    }
    tmp_s.push_back(unicodeChars[idx]);
  }

  // std::cout << tmp_s << std::endl;
  // std::cout << unicodeChars << std::endl;
  return tmp_s;
}

std::string generateMarkovText(int len, markov_chain &model)
{

  // check if the model has been trained yet
  if ( model.num_states() == 0 ) {
    fprintf(stderr, "Error: there is no sample added to the model yet.\n");
    exit(-1);
  }

  const std::string delimiter(" ");
  std::vector<std::string> first_words = { "Now", "Then", "Whilst", "She", "He", "It", "Look", "Ah!", "Come", "The", "True", "There" };
  first_words.push_back(model.randomString());
  first_words.push_back(model.randomString());
  first_words.push_back(model.randomString());
  std::reverse(first_words.begin(), first_words.end());

  std::ostringstream o;
  for (const std::string &word : first_words) {
    try
      {
        model.generate_data(o, delimiter, word, [](const std::string &curr_word) -> bool
        {
          // check for a period to end our sentence
          return curr_word.back() == '.';
        });
      }
      catch (std::exception &e)
      {
        // std::cout << "Data generation failed starting from word '" << word << "'. Reason: " << e.what() << std::endl;
        // don't print out anything
      }
  }
  // get the strings out
  std::string aa(o.str());
  // can we convert the std::string to 


  return aa;
}


// The idea is to load all the glyphs from a predefined text and check
// if any one of them is missing. Return the proportion of missing glyphs [0..1].
float proportionOfGlyphsMissing(std::string font_file_name, int face_index, int font_size) {
  std::vector<std::u32string> unicodeChars = {
      U"0", U"1", U"2", U"3", U"4", U"5", U"6", U"7", U"8", U"9", U" ", U" ", U" ", U" ",
      U" ", U" ", U"/", U"_", U"-", U".", U"A", U"B", U"C", U"D", U"E", U"F", U"G", U"H",
      U"I", U"J", U"K", U"L", U"M", U"N", U"O", U"P", U"Q", U"R", U"S", U"T", U"U", U"V",
      U"W", U"X", U"Y", U"Z", U"a", U"b", U"c", U"d", U"e", U"f", U"g", U"h", U"i", U"j",
      U"k", U"l", U"m", U"n", U"o", U"p", U"q", U"r", U"s", U"t", U"u", U"v", U"w", U"x",
      U"y", U"z", U"ø", U"æ", U"å", U"ö", U"ü", U"ä", U"(", U")", U"]", U"["};
  FT_Library library;
  FT_Face face;
  FT_GlyphSlot slot;
  FT_Matrix matrix; /* transformation matrix */
  //FT_Vector pen;    /* untransformed origin  */
  FT_Error error;
  float angle = 0;                     // ( 25.0 / 360 ) * 3.14159 * 2;      /* use 25 degrees     */
  //int target_height = HEIGHT;

  error = FT_Init_FreeType(&library); /* initialize library */
  /* error handling omitted */
  if (error != 0)
  {
    fprintf(stderr,
            "Error: The freetype libbrary could not be initialized with this font.\n");
    exit(-1);
  }

  error = FT_New_Face(library, font_file_name.c_str(), face_index, &face); /* create face object */
  if (face == NULL)
  {
    fprintf(stderr, "Error: no face found, provide the filename of a ttf file...\n");
    return 1.0; // could not even get started 
  }
  float font_size_in_pixel = font_size;
  error = FT_Set_Char_Size(face, font_size_in_pixel * 64, 0, 96, 0); /* set character size */
  if (error != 0) {
    return 1.0; // could not even get started with this font and the given size
  }
  slot = face->glyph;

  /* set up matrix */
  matrix.xx = (FT_Fixed)(cos(angle) * 0x10000L);
  matrix.xy = (FT_Fixed)(-sin(angle) * 0x10000L);
  matrix.yx = (FT_Fixed)(sin(angle) * 0x10000L);
  matrix.yy = (FT_Fixed)(cos(angle) * 0x10000L);

  /* the pen position in 26.6 cartesian space coordinates; */
  /* start at (300,200) relative to the upper left corner  */
  //pen.x = 1 * 64;
  //pen.y = (target_height - 20) * 64;

  float countMissing = 0;
  for (int n = 0; n < unicodeChars.size(); n++)
  {
    /* set transformation */
    // FT_Set_Transform(face, &matrix, &pen);
    /* load glyph image into the slot (erase previous one) */
    // unsigned long c = FT_Get_Char_Index(face, text2print[n]);
    // error = FT_Load_Glyph(face, c, FT_LOAD_RENDER);
    if (FT_Get_Char_Index(face, unicodeChars[n][0]) == 0) {
      countMissing += 1.0f;
      continue;    
    }

    error = FT_Load_Char(face, unicodeChars[n][0], FT_LOAD_RENDER);
    if (error)
      countMissing += 1.0f;
  }
  // everything is fine
  fprintf(stdout, "font check: %0.2f%% characters missing for %s [font size: %d]\n", 100.0f*(countMissing / unicodeChars.size()), font_file_name.c_str(), font_size);
  return countMissing / unicodeChars.size();
}

void exportBoundingBoxesAsXML(json boundingBoxes, std::string path, std::string folderWithText)
{
  // we convert the json to a property_tree to save as XML

  for (json::iterator it = boundingBoxes.begin(); it != boundingBoxes.end(); ++it)
  {
    std::string dcmFileName = it.key();
    std::string outputfilename;
    int width = 0;
    int height = 0;
    int depth = 3;
    std::string class_string;

    boost::property_tree::ptree pt;
    boost::property_tree::ptree annotation;

    boost::property_tree::ptree segmented;
    segmented.put_value(0);
    annotation.push_back(std::make_pair("segmented", segmented));

    boost::property_tree::ptree folder;
    folder.put_value(path);
    annotation.push_back(std::make_pair("folder", folder));

    boost::property_tree::ptree pathNode;
    pathNode.put_value(folderWithText);
    annotation.push_back(std::make_pair("path", pathNode));

    boost::property_tree::ptree source;
    boost::property_tree::ptree database;
    database.put_value("Unknown");
    source.push_back(std::make_pair("database", database));
    annotation.push_back(std::make_pair("source", source));

    for (json::iterator it2 = it.value().begin(); it2 != it.value().end(); ++it2)
    {
      // std::cout << it.key() << " : " << it2.value() << "\n";
      // fprintf(stdout, "%s\n", it2.key().c_str());
      // std::string k = it2.key();
      // fprintf(stdout, "done with key\n");
      //  now do the keys in each object
      boost::property_tree::ptree object;
      boost::property_tree::ptree entry;
      entry.put_value(std::string("Unspecified"));
      object.push_back(std::make_pair("pose", entry));
      entry.put_value(0);
      object.push_back(std::make_pair("truncated", entry));
      object.push_back(std::make_pair("difficult", entry));

      boost::property_tree::ptree box;
      for (json::iterator it3 = it2.value().begin(); it3 != it2.value().end(); ++it3)
      {
        std::string k = it3.key();
        if (k == "filename")
        {
          outputfilename = it3.value(); // should be always the same filename
        }
        if (k == "class")
        {
          class_string = it3.value();
          entry.put_value(std::string(class_string));
          object.push_back(std::make_pair("name", entry));
        }
        if (k == "imagewidth")
        {
          width = it3.value();
        }
        if (k == "imageheight")
        {
          height = it3.value();
        }
        if (k == "xmin" || k == "xmax" || k == "ymin" || k == "ymax")
        {
          int v = it3.value();
          boost::property_tree::ptree entry;
          entry.put_value(v);
          box.push_back(std::make_pair(k, entry));
        }
      }
      object.push_back(std::make_pair("bndbox", box));
      annotation.push_back(std::make_pair("object", object));
    }
    boost::property_tree::ptree fn;
    fn.put_value(outputfilename);
    annotation.push_back(std::make_pair("filename", fn));

    boost::property_tree::ptree sizeNode;
    boost::property_tree::ptree widthNode;
    widthNode.put_value(width);
    boost::property_tree::ptree heightNode;
    heightNode.put_value(height);
    boost::property_tree::ptree depthNode;
    depthNode.put_value(depth);
    sizeNode.push_back(std::make_pair("width", widthNode));
    sizeNode.push_back(std::make_pair("height", heightNode));
    sizeNode.push_back(std::make_pair("depth", depthNode));
    annotation.push_back(std::make_pair("size", sizeNode));

    pt.push_back(std::make_pair("annotation", annotation));

    const boost::property_tree::xml_writer_settings<std::string> w(' ', 2);
    std::string filename = path + "/" + outputfilename.replace(outputfilename.end() - 4, outputfilename.end(), "") + ".xml";
    //  fprintf(stdout, "write to file %s\n", filename.c_str());
    boost::property_tree::write_xml(filename, pt, std::locale(), w);
  }
}

// for debugging use:
//    cmake -DCMAKE_BUILD_TYPE=Debug
int main(int argc, char **argv)
{
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

  if (options[HELP] || argc == 0)
  {
    option::printUsage(std::cout, usage);
    return 0;
  }

  std::vector<std::string> dicom_paths;
  std::string configfile_path = "";
  std::string output = "";    // directory path
  std::string font_path = ""; // "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
  unsigned int random_seed = (unsigned)time(NULL) * getpid();
  std::string target_type = "dcm";
  std::string batch_string = "";

  // if we use a text file to generate text we need such a model
  markov_chain markov_model_text_generation;

  int target = 1;
  bool verbose = false;

  for (option::Option *opt = options[UNKNOWN]; opt; opt = opt->next())
    std::cout << "Unknown option: " << std::string(opt->name, opt->namelen) << "\n";

  for (int i = 0; i < parse.optionsCount(); ++i)
  {
    option::Option &opt = buffer[i];
    // fprintf(stdout, "Argument #%d is ", i);
    switch (opt.index())
    {
    case HELP:
      // not possible, because handled further above and exits the program
      break;
    case VERBOSE:
      verbose = true;
      break;
    case TWOCLASS:
      twoclass = true;
      break;
    case DICOMS:
      if (opt.arg)
      {
        // fprintf(stdout, "--input '%s'\n", opt.arg);
        // check if the path exists and cancel if not
        if (!boost::filesystem::exists(opt.arg) || !boost::filesystem::is_directory(opt.arg)) {
          fprintf(stderr, "Error: data directory (%s) could not be found.\n", opt.arg);
          exit(-1);
        }
        dicom_paths.push_back(opt.arg);
      }
      else
      {
        fprintf(stdout, "--dicoms needs a directory specified\n");
        exit(-1);
      }
      break;
    case OUTPUT:
      if (opt.arg)
      {
        // fprintf(stdout, "--output '%s'\n", opt.arg);
        output = opt.arg;
      }
      else
      {
        fprintf(stdout, "--output needs a directory specified\n");
        exit(-1);
      }
      break;
    case BATCH:
      if (opt.arg)
      {
        // fprintf(stdout, "--output '%s'\n", opt.arg);
        batch_string = opt.arg;
      }
      else
      {
        fprintf(stdout, "--batch needs a string specified\n");
        exit(-1);
      }
      break;
    case FONTFILE:
      if (opt.arg)
      {
        // fprintf(stdout, "--output '%s'\n", opt.arg);
        font_path = opt.arg;
      }
      else
      {
        fprintf(stdout, "--fontfile needs a font file specified\n");
        exit(-1);
      }
      break;
    case TARGETNUM:
      if (opt.arg)
      {
        // fprintf(stdout, "--output '%s'\n", opt.arg);
        target = std::stoi(opt.arg);
      }
      else
      {
        fprintf(stdout, "--targetnum needs a number as an argument\n");
        exit(-1);
      }
      break;
    case RANDOMSEED:
      if (opt.arg)
      {
        // fprintf(stdout, "--output '%s'\n", opt.arg);
        random_seed = std::stoi(opt.arg);
      }
      else
      {
        fprintf(stdout, "--random_seed needs a number as an argument\n");
        exit(-1);
      }
      break;
    case CONFIG:
      if (opt.arg)
      {
        // fprintf(stdout, "--output '%s'\n", opt.arg);
        configfile_path = opt.arg;
      }
      else
      {
        fprintf(stdout, "--config needs a file name specified\n");
        exit(-1);
      }
      break;
    case EXPORTTYPE:
      if (opt.arg)
      {
        // fprintf(stdout, "--output '%s'\n", opt.arg);
        target_type = opt.arg;
        if (target_type != "png")
        {
          target_type = "dcm";
        }
      }
      else
      {
        fprintf(stdout, "--target_type needs dcm or png\n");
        exit(-1);
      }
      break;
    case MARKOV:
      if (opt.arg) {
        // train the model
        // markov_model_text_generation
        std::ifstream ifs(opt.arg);
        size_t last_state = -1;
        std::string word;
        while (ifs >> word) {
            last_state = markov_model_text_generation.record_sample(word, last_state);
        }
      } else {
        fprintf(stderr, "--markov needs a text file path as argument");
        exit(-1);
      }
      break;
    case UNKNOWN:
      // not possible because Arg::Unknown returns ARG_ILLEGAL
      // which aborts the parse with an error
      break;
    }
  }
  std::srand(random_seed);
  int font_size = 12;
  int face_index;

  // we should parse the config file
  pt::ptree root;
  bool configFileExists = false;
  std::string dn = configfile_path;
  struct stat stat_buffer;
  if (!(stat(dn.c_str(), &stat_buffer) == 0))
  {
    configFileExists = false;
  }
  else
  {
    configFileExists = true;
    if (verbose)
      fprintf(stdout, "  Found config file in: %s\n", configfile_path.c_str());
  }
  std::vector<std::string> font_paths;
  std::vector<std::vector<int>> font_sizes;   // pick a random size from these
  std::vector<std::vector<int>> face_indexes; // if the font has more than one face index
  if (configFileExists)
  {
    pt::read_json(configfile_path.c_str(), root);
    if (verbose)
    {
      fprintf(stdout, "Found model in config file \"%s\"\n\n",
              root.get_child("model").get_value<std::string>().c_str());
      fprintf(stdout, "%s\n", root.get_child("description").get_value<std::string>().c_str());
    }
    if (font_path != "")
    {
      // a font_path could be a real file or a wildcard string
      boost::filesystem::path bdir(font_path);
      if (boost::filesystem::is_directory(bdir) && boost::filesystem::exists(bdir))
      {
        // directory exists
        // search for any ttf files
        std::string extension;
        boost::filesystem::recursive_directory_iterator end, dir(font_path);
        while (dir != end) {
        // for (boost::filesystem::recursive_directory_iterator end, dir(font_path); dir != end; ++dir)
        // {
          if (is_regular_file(dir->path()))
          {
            boost::filesystem::path bpath(dir->path());
            extension = bpath.extension().string();
            if (extension != ".ttf") {
              ++dir;
              continue;
            }
            // we should check if this font, font-size, index is suitable
            // proportionOfGlyphsMissing
            std::vector<int> this_font_sizes;
            for (int fs = 6; fs < 14; fs+=2) {
              if (proportionOfGlyphsMissing(std::string(dir->path().c_str()), 0, fs) < 0.1)
                this_font_sizes.push_back(fs);
            }
            if (this_font_sizes.size() == 0) {
              ++dir;
              continue; // do not use this font
            }

            font_paths.push_back(dir->path().c_str());
            font_sizes.push_back(this_font_sizes);

            std::vector<int> this_face_indexes;
            this_face_indexes.push_back(0);
            face_indexes.push_back(this_face_indexes);
          }
          try {
            ++dir;
          } catch( const std::exception&  ex) {
            fprintf(stderr, "Can't enter directory/file: %s, skip.\n", ex.what());
            //dir.disable_recursion_pending();
            continue;
          }
        }
      }
      else if (boost::filesystem::is_regular_file(font_path) && boost::filesystem::exists(font_path))
      {
        // add the manually specified font to the font_paths array
        std::vector<int> this_font_sizes;
        for (int fs = 6; fs < 18; fs+=2) {
          if (proportionOfGlyphsMissing(font_path, 0, fs) < 0.1)
            this_font_sizes.push_back(fs);
        }
        if (this_font_sizes.size() > 0) {
          font_paths.push_back(font_path);
          // add some default sizes as well
          font_sizes.push_back(this_font_sizes);

          std::vector<int> this_face_indexes;
          this_face_indexes.push_back(0);
          face_indexes.push_back(this_face_indexes);
        }
      }
    }

    //
    // Check for a random font
    //
    if (font_path == "")
    {
      auto bounds = root.get_child("logic").get_child("font").equal_range("");
      for (auto iter = bounds.first; iter != bounds.second; ++iter)
      {
        std::string fname = iter->second.get_child("name").get_value<std::string>();
        // check if the file exists first
        struct stat buf;
        if (!(stat(fname.c_str(), &buf) == 0))
        {
          fprintf(stderr,
                  "Warning: font \"%s\" not found.\n", fname.c_str());
          continue;
        }

        font_paths.push_back(fname);
        // now read the sizes for this font
        auto bounds2 = iter->second.get_child("sizes").equal_range("");
        std::vector<int> this_font_sizes;
        for (auto iter2 = bounds2.first; iter2 != bounds2.second; ++iter2)
        {
          int val = iter2->second.get_value<int>();
          this_font_sizes.push_back(val);
        }
        font_sizes.push_back(this_font_sizes);
        //
        auto bounds3 = iter->second.get_child("face_indexes").equal_range("");
        std::vector<int> this_face_indexes;
        for (auto iter3 = bounds3.first; iter3 != bounds3.second; ++iter3)
        {
          int val = iter3->second.get_value<int>();
          this_face_indexes.push_back(val);
        }
        face_indexes.push_back(this_face_indexes);
      }
      if (font_paths.size() > 0)
      {
        int idx = std::rand() % font_paths.size();
        font_path = font_paths[idx]; // use a random font
        int idx2 = std::rand() % font_sizes[idx].size();
        font_size = font_sizes[idx][idx2];
        int idx3 = std::rand() % face_indexes[idx].size();
        face_index = face_indexes[idx][idx3];
        if (verbose)
          fprintf(stdout,
                  "Selected a font [%d] from the config file (%lu font%s found). Size is %d. Face "
                  "index is: %d.\n",
                  idx, font_paths.size(), font_paths.size() > 1 ? "s" : "", font_size, face_index);
      }
    }
    if (font_path == "")
    {
      fprintf(stderr,
              "Error: no font path specified in either config file or on the command line.");
      exit(-1);
    }
  }
  dn = font_path;
  struct stat buf;
  if (!(stat(dn.c_str(), &buf) == 0))
  {
    fprintf(stderr,
            "Error: no font provided. Use the -f option and provide the name of a ttf file.\n");
    exit(-1);
  }

  //
  // next we can look for a random placement
  //
  std::vector<std::map<std::string, std::vector<float>>>
      placements; // if the font has more than one face index
  if (configFileExists)
  {
    pt::read_json(configfile_path.c_str(), root);
    boost::property_tree::ptree d = root.get_child("logic").get_child("placement");
    for (const std::pair<std::string, boost::property_tree::ptree> &p : d)
    {
      std::map<std::string, std::vector<float>> entry;
      if (verbose)
        fprintf(stdout, "  Placement: %s\n", p.first.c_str());
      auto bounds = p.second.get_child("x").equal_range("");
      std::vector<float> values;
      for (auto iter = bounds.first; iter != bounds.second; ++iter)
      {
        // fprintf(stdout, "found an x value of %s\n",
        // iter->second.get_value<std::string>().c_str());
        values.push_back(iter->second.get_value<float>());
      }
      entry.insert(std::pair<std::string, std::vector<float>>("x", values));

      auto bounds2 = p.second.get_child("y").equal_range("");
      std::vector<float> values2;
      for (auto iter2 = bounds2.first; iter2 != bounds2.second; ++iter2)
      {
        // fprintf(stdout, "found an x value of %s\n",
        // iter2->second.get_value<std::string>().c_str());
        values2.push_back(iter2->second.get_value<float>());
      }
      entry.insert(std::pair<std::string, std::vector<float>>("y", values2));

      auto bounds3 = p.second.get_child("repeat-spacing").equal_range("");
      std::vector<float> values3;
      for (auto iter3 = bounds3.first; iter3 != bounds3.second; ++iter3)
      {
        // fprintf(stdout, "found an x value of %s\n",
        // iter2->second.get_value<std::string>().c_str());
        values3.push_back(iter3->second.get_value<float>());
      }
      entry.insert(std::pair<std::string, std::vector<float>>("repeat-spacing", values3));

      auto bounds4 = p.second.get_child("how-many").equal_range("");
      std::vector<float> values4;
      for (auto iter4 = bounds4.first; iter4 != bounds4.second; ++iter4)
      {
        // fprintf(stdout, "found an x value of %s\n",
        // iter2->second.get_value<std::string>().c_str());
        values4.push_back(iter4->second.get_value<float>());
      }
      entry.insert(std::pair<std::string, std::vector<float>>("how-many", values4));

      auto bounds5 = p.second.get_child("lengths").equal_range("");
      std::vector<float> values5;
      for (auto iter5 = bounds5.first; iter5 != bounds5.second; ++iter5)
      {
        // fprintf(stdout, "found an x value of %s\n",
        // iter2->second.get_value<std::string>().c_str());
        values5.push_back(iter5->second.get_value<float>());
      }
      entry.insert(std::pair<std::string, std::vector<float>>("lengths", values5));

      placements.push_back(entry);
    }
  }
  if (verbose)
    fprintf(stdout, "  Found %ld placements\n", placements.size());

  //
  // Read in the image contrasts
  //
  std::vector<std::vector<float>> image_contrasts; // lower [min%, max%], upper [min%, max%]
  if (configFileExists)
  {
    pt::read_json(configfile_path.c_str(), root);
    auto bounds = root.get_child("logic").get_child("image_contrasts").equal_range("");
    for (auto iter = bounds.first; iter != bounds.second; ++iter)
    {
      std::string fname = iter->second.get_child("name").get_value<std::string>();
      // now read the sizes for this font
      auto bounds2 = iter->second.get_child("min").equal_range("");
      std::vector<float> this_contrast;
      for (auto iter2 = bounds2.first; iter2 != bounds2.second; ++iter2)
      {
        float val = iter2->second.get_value<float>();
        this_contrast.push_back(val);
      }
      //
      auto bounds3 = iter->second.get_child("max").equal_range("");
      for (auto iter3 = bounds3.first; iter3 != bounds3.second; ++iter3)
      {
        float val = iter3->second.get_value<float>();
        this_contrast.push_back(val);
      }
      image_contrasts.push_back(this_contrast);
    }
  }
  if (verbose)
    fprintf(stdout, "Found %ld contrast range%s to choose from.\n", image_contrasts.size(), image_contrasts.size() > 1 ? "s" : "");
  float image_contrastx; // a single choice, value between  the two specified, something like 0.1
  float image_contrasty; // a single choice
  {
    int idx = std::rand() % image_contrasts.size();
    image_contrastx = image_contrasts[idx][0] + (((1.0f * std::rand()) / (1.0f * RAND_MAX)) * (image_contrasts[idx][1] - image_contrasts[idx][0]));
    image_contrasty = image_contrasts[idx][2] + (((1.0f * std::rand()) / (1.0f * RAND_MAX)) * (image_contrasts[idx][3] - image_contrasts[idx][2]));
    if (verbose)
      fprintf(stdout, "Selected image contrast of %f %f\n", image_contrastx, image_contrasty);
  }

  //
  // Read in the colors
  // TODO: check what happens if a variable is not there
  std::vector<std::vector<float>> colors; // the background and pen color for the annotations
  if (configFileExists)
  {
    pt::read_json(configfile_path.c_str(), root);
    auto bounds = root.get_child("logic").get_child("colors").equal_range("");
    for (auto iter = bounds.first; iter != bounds.second; ++iter)
    {
      std::string fname = iter->second.get_child("name").get_value<std::string>();
      // now read the sizes for this font
      auto bounds2 = iter->second.get_child("background_size").equal_range("");
      std::vector<float> this_background_size;
      for (auto iter2 = bounds2.first; iter2 != bounds2.second; ++iter2)
      {
        float val = iter2->second.get_value<float>();
        this_background_size.push_back(val);
      }
      //
      auto bounds3 = iter->second.get_child("background_color").equal_range("");
      for (auto iter3 = bounds3.first; iter3 != bounds3.second; ++iter3)
      {
        float val = iter3->second.get_value<float>();
        this_background_size.push_back(val);
      }
      //
      auto bounds4 = iter->second.get_child("pen_color").equal_range("");
      for (auto iter4 = bounds4.first; iter4 != bounds4.second; ++iter4)
      {
        float val = iter4->second.get_value<float>();
        this_background_size.push_back(val);
      }
      // vary_percent
      float vary_percent = iter->second.get_child("vary_percent").get_value<float>();
      this_background_size.push_back(vary_percent);

      /*auto bounds5 = iter->second.get_child("vary_percent").equal_range("");
      for (auto iter5 = bounds5.first; iter5 != bounds4.second; ++iter5)
      {
        float val = iter5->second.get_value<float>();
        this_background_size.push_back(val);
      }*/
      // if we don't have enough messages, skip attaching the array
      colors.push_back(this_background_size);
    }
  }
  if (verbose)
    fprintf(stdout, "Found %ld color setting%s to choose from.\n", colors.size(), colors.size() > 1 ? "s" : "");
  std::vector<float> color_background_size;  // a single choice, value between  the two specified, something like 0.1
  std::vector<float> color_background_color; // a single choice
  std::vector<float> color_pen_color;
  float vary_percent;

  // we should find all DICOM files
  fprintf(stdout, "\n");
  std::vector<std::string> files = listFilesSTD(dicom_paths, true);
  fprintf(stdout, "\n");
  // fprintf(stdout, "found %'ld files\n", files.size());

  // We will generate images randomly --- but we could generate one for each as well.

  FT_Library library;

  double angle;
  int target_height;
  int n, num_chars;
  // char *json;

  //  if (argc < 3) {
  //    fprintf(stderr, "usage: %s font_file.ttf sample-text json|text\n", argv[0]);
  //    exit(1);
  //  }

  int font_length = 20;

  // check for the available fonts first
  if (font_paths.size() == 0)
  {
    fprintf(stderr, "Error: No valid font files specified\n");
    exit(-1);
  }
  if (font_sizes.size() < font_paths.size())
  {
    // assert that size of font_sizes is same as size of font_paths
    while (font_sizes.size() < font_paths.size())
    {
      // add a default size array
      std::vector<int> this_font_sizes = {6, 8, 10, 11, 12};
      font_sizes.push_back(this_font_sizes);
    }
  }
  if (face_indexes.size() < font_paths.size())
  {
    while (face_indexes.size() < font_paths.size())
    {
      std::vector<int> this_face_indexes = {0};
      face_indexes.push_back(this_face_indexes);
    }
  }

  // const char *filename = font_path.c_str(); /* first argument     */
  // std::map<std::string, std::vector<bbox_t>> boundingBoxes;
  json boundingBoxes = json::object();
  int bboxCounter = 0;
  for (int i = 0; i < target; i++)
  { // how many images
    fprintf(stdout, "[ create file %d/%d ]\r", i + 1, target);
    fflush(stdout);
    json bboxes = json::array(); // all the bounding boxes for the current image
    //
    // for every new DICOM image we randomize some things
    //
    // fprintf(stdout, "font paths size is: %lu\n", font_paths.size());
    // fflush(stdout);
    int idx = std::rand() % font_paths.size();
    font_path = font_paths[idx]; // use a random font
    // fprintf(stdout, "font sizes array is: %d\n", font_sizes.size());
    int idx2 = std::rand() % font_sizes[idx].size();
    font_size = font_sizes[idx][idx2];
    int idx3 = std::rand() % face_indexes[idx].size();
    face_index = face_indexes[idx][idx3];
    if (verbose)
      fprintf(stdout,
              "\nSelected a font [%d] from the config file (%lu font%s found). Size is %d. Face "
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
    int maxIter = 100;
    while (maxIter > 0)
    {
      pickImageIdx = std::rand() % files.size();
      // to check for DICOM lets use the normal gdcm::Read and CanRead
      gdcm::Reader r;
      r.SetFileName(files[pickImageIdx].c_str());
      if (r.CanRead() != true)
      {
        maxIter--;
        if (verbose)
          fprintf(stdout, "read failed [%d] for \"%s\"... try next\n", maxIter,
                  files[pickImageIdx].c_str());
        continue; // try again
      }

      reader.SetFileName(files[pickImageIdx].c_str());
      if (verbose)
        fprintf(stdout, "Try to read \"%s\"\n", files[pickImageIdx].c_str());
      if (!reader.Read())
      {
        maxIter--;
        fprintf(stdout, "read failed... try next\n");
        continue; // try again
      }
      foundDICOM = true;
      break;
    }
    if (!foundDICOM)
    {
      fprintf(stderr, "Error: I tried to find a DICOM image I could read. I could not... ");
      exit(-1);
    }
    else
    {
      if (verbose)
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
    if (!b)
    {
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
    if (pi != gdcm::PhotometricInterpretation::MONOCHROME2)
    {
      fprintf(stderr, "Warning: We only understand MONOCHROME2, skip \"%s\" for this image %s.\n", gdcm::PhotometricInterpretation::GetPIString(pi), files[pickImageIdx].c_str());
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
    unsigned short xmax;
    unsigned short ymax;

    // image dimensions
    std::vector<unsigned int> extent = gdcm::ImageHelper::GetDimensionsValue(reader.GetFile());

    xmax = (unsigned short)extent[0];
    ymax = (unsigned short)extent[1];

    gdcm::PixelFormat pf = change_image.GetPixelFormat();
    // pf could be gdcm::PixelFormat::INT8, or INT16, etc...
    // We need to change our cast further down on the pixel data.
    unsigned short pixelsize;
    int bitsAllocated = pf.GetBitsAllocated(); // could be 8 or 16
    if ((bitsAllocated % 8) != 0)
    {
      fprintf(stderr, "Warning: BitsAllocated is not a multiple of 8, could be a segmentation object, skip file \"%s\".\n", files[pickImageIdx].c_str());
      continue;
    }
    pixelsize = pf.GetPixelSize();
    if (verbose)
      fprintf(stdout, "  pixelsize of input is: %d\n", pixelsize);
    unsigned long len = change_image.GetBufferLength();
    // fprintf(stdout, "Found buffer of length: %ld\n", len);
    char *buffer = new char[len];

    change_image.GetBuffer(buffer);

    //
    // Up to here everything is about DICOM but at the end we have buffer.
    // We should be able to do the same for png (16bit using libpng).
    //

    // what is max and min here? (don't change by the overlay)
    float current_image_min_value, current_image_max_value;
    {
      if (bitsAllocated == 16)
      {
        signed short *bvals = (signed short *)buffer;
        current_image_min_value = (float)bvals[0];
        current_image_max_value = (float)bvals[0];
        for (int p = 1; p < xmax * ymax; p++)
        {
          if (current_image_min_value > bvals[p])
            current_image_min_value = bvals[p];
          if (current_image_max_value < bvals[p])
            current_image_max_value = bvals[p];
        }
      }
      else if (bitsAllocated == 8)
      {
        unsigned char *bvals = (unsigned char *)buffer;
        current_image_min_value = (float)bvals[0];
        current_image_max_value = (float)bvals[0];
        for (int p = 1; p < xmax * ymax; p++)
        {
          if (current_image_min_value > bvals[p])
            current_image_min_value = bvals[p];
          if (current_image_max_value < bvals[p])
            current_image_max_value = bvals[p];
        }
      }
      else
      {
        fprintf(stderr, "Error: this bits allocated value is not supported %d.\n", bitsAllocated);
        continue;
      }
    }

    // apply an image contrast change before using the buffer values
    {
      float span = current_image_max_value - current_image_min_value;
      if (span == 0) {
        span = 1.0f;
      }
      if (bitsAllocated == 16)
      {
        signed short *bvals = (signed short *)buffer;
        float new_min = current_image_min_value + ((span) * image_contrastx);
        float new_max = current_image_max_value - ((span) * (1.0f - image_contrasty));
        // fprintf(stdout, "new min: %f, new_max: %f (old %f %f)\n", new_min, new_max, current_image_min_value, current_image_max_value);
        for (int p = 0; p < xmax * ymax; p++)
        {
          float A = (bvals[p] - current_image_min_value) / span;
          float nv = new_min + (A * (new_max - new_min));
          bvals[p] = (signed short)std::max(0.0f, std::min(nv, current_image_max_value));
        }
      }
      else if (bitsAllocated == 8)
      {
        unsigned char *bvals = (unsigned char *)buffer;
        float new_min = current_image_min_value + ((span) * image_contrastx);
        float new_max = current_image_max_value - ((span) * (1.0f - image_contrasty));
        // fprintf(stdout, "new min: %f, new_max: %f (old %f %f)\n", new_min, new_max, current_image_min_value, current_image_max_value);
        for (int p = 0; p < xmax * ymax; p++)
        {
          float A = (bvals[p] - current_image_min_value) / (span);
          float nv = new_min + (A * (new_max - new_min));
          bvals[p] = (unsigned char)std::max(0.0f, std::min(nv, current_image_max_value));
        }
      }
    }

    //
    // write the image pair
    //
    dn = output;
    // struct stat buf;
    if (!(stat(dn.c_str(), &buf) == 0))
    {
      mkdir(dn.c_str(), 0777);
    }

    dn = output + "/with_text/";
    // struct stat buf;
    if (!(stat(dn.c_str(), &buf) == 0))
    {
      mkdir(dn.c_str(), 0777);
    }
    dn = output + "/without_text/";
    if (!(stat(dn.c_str(), &buf) == 0))
    {
      mkdir(dn.c_str(), 0777);
    }

    char outputfilename[1024];

    if (target_type == "dcm")
    {
      gdcm::ImageWriter writer2;
      writer2.SetImage(change.GetOutput());
      writer2.SetFile(reader.GetFile());
      snprintf(outputfilename, 1024 - 1, "%s/without_text/%s%08d.dcm", output.c_str(), batch_string.c_str(), i);
      writer2.SetFileName(outputfilename);
      if (!writer2.Write())
      {
        return 1;
      }
    }
    else
    {
      snprintf(outputfilename, 1024 - 1, "%s/without_text/%s%08d.png", output.c_str(), batch_string.c_str(), i);
      rgb16_image_t img(xmax, ymax);
      rgb16_pixel_t rgb(65535, 0, 0);
      // we should copy the values over now --- from the buffer? Or from the DICOM Image
      // fill_pixels(view(img), red);
      // stretch the intensities from 0 to max for png (0...65535)
      float pmin = current_image_min_value;
      float pmax = current_image_max_value;
      auto v = view(img);
      auto it = v.begin();
      if (bitsAllocated == 16)
      {
        signed short *bvals = (signed short *)buffer;
        while (it != v.end())
        {
          //++hist[*it];
          float pixel_val = ((1.0 * bvals[0]) - pmin) / (pmax - pmin);
          *it = rgb16_pixel_t{(short unsigned int)(pixel_val * 65535), (short unsigned int)(pixel_val * 65535), (short unsigned int)(pixel_val * 65535)};
          bvals++;
          it++;
        }
        write_view(outputfilename, const_view(img), png_tag{});
      }
      else if (bitsAllocated == 8)
      {
        rgb8_image_t img8(xmax, ymax);
        auto v = view(img8);
        auto it = v.begin();
        unsigned char *bvals = (unsigned char *)buffer;
        while (it != v.end())
        {
          float pixel_val = ((1.0 * bvals[0]) - pmin) / (pmax - pmin);
          *it = rgb8_pixel_t{(unsigned char)(pixel_val * 255), (unsigned char)(pixel_val * 255), (unsigned char)(pixel_val * 255)};
          bvals++;
          it++;
        }
        write_view(outputfilename, const_view(img8), png_tag{});
      }
    }
    // fprintf(stdout, "  dimensions: %d %d, len: %lu\n", xmax, ymax, len);

    // lets do all the text placements
    for (int placement = 0; placement < placements.size(); placement++)
    {
      {                                        // color setting by placement
        int idx = std::rand() % colors.size(); // colors.size() should not be 0
        if (verbose)
          fprintf(stdout, "  Select color set %d, opacity of background: %.2f\n", idx, colors[idx][7]);
        if (colors[idx].size() < 13) {
          fprintf(stderr, "Error: selected a colors entry that does not all variable defined, cancel.\n");
          exit(-1);
        }
        // size here is how much bigger the background is supposed to be compared to the bounding box of the text.
        // we draw the image, draw the background and draw the text (in that order)
        color_background_size = {colors[idx][0], colors[idx][1], colors[idx][2], colors[idx][3]};
        // a size of 0,0,0,0 indicates that no background needs to be drawn (default normal mode, text is white)
        color_background_color = {colors[idx][4], colors[idx][5], colors[idx][6], colors[idx][7]};
        color_pen_color = {colors[idx][8], colors[idx][9], colors[idx][10], colors[idx][11]};
        vary_percent = colors[idx][12];
      }
      if (verbose)
        fprintf(stdout, "  Use placement: %d\n", placement);
      float vx_min = placements[placement]["x"][0];
      float vx_max = placements[placement]["x"][1];
      float vx = vx_min + ((rand() * 1.0f) / (1.0f * RAND_MAX)) * (vx_max - vx_min);
      float vy_min = placements[placement]["y"][0];
      float vy_max = placements[placement]["y"][1];
      float vy = vy_min + ((rand() * 1.0f) / (1.0f * RAND_MAX)) * (vy_max - vy_min);
      int start_px, start_py;
      bool neg_x = false;
      bool neg_y = false;
      if (vx >= 0)
      {
        start_px = std::floor(xmax * vx);
      }
      else
      {
        start_px = xmax - std::floor(xmax * -vx);
        neg_x = true;
      }
      if (vy >= 0)
      {
        start_py = std::floor(ymax * vy);
      }
      else
      {
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
      for (int text_lines = 0; text_lines < howmany; text_lines++)
      {
        memset(image_buffer, 0, HEIGHT * WIDTH);

        int px = start_px;
        int py = start_py + (neg_y ? -1 : 1) * (text_lines * font_size +
                                                text_lines * (repeat_spacing * 0.5 * font_size));

        int lengths_min = placements[placement]["lengths"][0];
        int lengths_max = placements[placement]["lengths"][1];
        font_length = (rand() % (lengths_max - lengths_min)) + lengths_min;
        // fprintf(stdout, "FONTLENGTH IS: %d\n", font_length);

        // use markov in case we have it
        std::vector<std::u32string> text2print;
        std::string text2printSTD;
        if (markov_model_text_generation.num_states() > 0) {
          text2printSTD = generateMarkovText(font_length, markov_model_text_generation);  
        } else {
          text2print = generateRandomText(font_length);
        }
        // char *text = strdup(text2print.c_str()); // print the text (no utf-8 here)
        // json = (char *)"text";
        // fprintf(stdout, "  text is now: \"%s\" ScalarType in DICOM: %s\n", text,
        //        pf.GetScalarTypeAsString());
        // if (argc == 4) {
        //  json = argv[3];
        //}
        if (markov_model_text_generation.num_states() > 0) {
          num_chars = text2printSTD.size();
        } else {
          num_chars = text2print.size(); // strlen(text);
        }
        angle = 0;                     // ( 25.0 / 360 ) * 3.14159 * 2;      /* use 25 degrees     */
        target_height = HEIGHT;

        error = FT_Init_FreeType(&library); /* initialize library */
        /* error handling omitted */
        if (error != 0)
        {
          fprintf(stderr,
                  "Error: The freetype libbrary could not be initialized with this font.\n");
          exit(-1);
        }

        error = FT_New_Face(library, filename, face_index, &face); /* create face object */
        /* error handling omitted */
        if (face == NULL)
        {
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
        if (error != 0)
        {
          // try with the next font size
          fprintf(stderr, "Warning: FT_Set_Char_Size returned error for size %d.\n", font_size);
          continue;
          //fprintf(stderr, "Error: FT_Set_Char_Size returned error.\n");
          //exit(-1);
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

        for (n = 0; n < num_chars; n++)
        {
          /* set transformation */
          FT_Set_Transform(face, &matrix, &pen);

          /* load glyph image into the slot (erase previous one) */
          // unsigned long c = FT_Get_Char_Index(face, text2print[n]);
          // error = FT_Load_Glyph(face, c, FT_LOAD_RENDER);

          if (markov_model_text_generation.num_states() > 0) {
            error = FT_Load_Char(face, text2printSTD[n], FT_LOAD_RENDER);
          } else {
            error = FT_Load_Char(face, text2print[n][0], FT_LOAD_RENDER);
          }
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

        // image min max is 0 .. 255
        /*float pmi = image_buffer[0][0];
        float pma = image_buffer[0][0];
        for (int yi = 0; yi < HEIGHT; yi++) {
          for (int xi = 0; xi < WIDTH; xi++) {
            if (pmi > image_buffer[yi][xi]) pmi = image_buffer[yi][xi];
            if (pma < image_buffer[yi][xi]) pma = image_buffer[yi][xi];
          }
        }
        fprintf(stdout, "bitmap font min %f and max %f\n", pmi, pma); */
        std::vector<int> boundingBox = {INT_MAX, INT_MAX, 0, 0}; // xmin, ymin, xmax, ymax

        // if we have two class we need to decide here by chance if this box has text or not
        bool leaveWithoutText = ((std::rand() % 2) == 0);
        if (!twoclass)
        {
          leaveWithoutText = false;
        }

        // now copy the string in
        if (bitsAllocated == 16)
        {
          signed short *bvals = (signed short *)buffer;
          for (int yi = 0; yi < HEIGHT; yi++)
          { // compute the bounding box
            for (int xi = 0; xi < WIDTH; xi++)
            {
              if (image_buffer[yi][xi] == 0)
                continue;
              // I would like to copy the value from image over to
              // the buffer. At some good location...
              int newx = px + xi;
              int newy = py + yi;
              int idx = newy * xmax + newx;
              if (newx < 0 || newx >= xmax || newy < 0 || newy >= ymax)
                continue;
              if (image_buffer[yi][xi] == 0)
                continue;
              if (newx < boundingBox[0])
                boundingBox[0] = newx;
              if (newy < boundingBox[1])
                boundingBox[1] = newy;
              if (newx >= boundingBox[2])
                boundingBox[2] = newx;
              if (newy >= boundingBox[3])
                boundingBox[3] = newy;
            }
          }
        }
        else if (bitsAllocated == 8)
        {
          unsigned char *bvals = (unsigned char *)buffer;
          for (int yi = 0; yi < HEIGHT; yi++)
          { // compute the bounding box
            for (int xi = 0; xi < WIDTH; xi++)
            {
              if (image_buffer[yi][xi] == 0)
                continue;
              // I would like to copy the value from image over to
              // the buffer. At some good location...
              int newx = px + xi;
              int newy = py + yi;
              int idx = newy * xmax + newx;
              if (newx < 0 || newx >= xmax || newy < 0 || newy >= ymax)
                continue;
              if (image_buffer[yi][xi] == 0)
                continue;
              if (newx < boundingBox[0])
                boundingBox[0] = newx;
              if (newy < boundingBox[1])
                boundingBox[1] = newy;
              if (newx >= boundingBox[2])
                boundingBox[2] = newx;
              if (newy >= boundingBox[3])
                boundingBox[3] = newy;
            }
          }
        }
        else
        {
          fprintf(stderr, "Warning: Found bitsAllocation that we don't support %d\n", bitsAllocated);
          fflush(stderr);
          exit(-1);
        }

        // draw the background
        if (fabs(color_background_color[3]) > 1e-6)
        {
          // check if we should draw the background at all - might be
          // that the color is transparent and we don't have to do anything here
          // fprintf(stdout, "  Add a background %f %f %f %f for this text line.\n", color_background_color[0], color_background_color[1],
          //        color_background_color[2], color_background_color[3]);
          // we need to make the boundingBox from above larger by the color_background_size fields
          float rnx1 = (1.0f * rand()) / (1.0f * RAND_MAX);
          float rnx2 = (1.0f * rand()) / (1.0f * RAND_MAX);
          float rny1 = (1.0f * rand()) / (1.0f * RAND_MAX);
          float rny2 = (1.0f * rand()) / (1.0f * RAND_MAX);
          std::vector<int> backgroundBoundingBox = {
              (int)std::round(boundingBox[0] - (color_background_size[0] + rnx1 * (color_background_size[1] - color_background_size[0]))),
              (int)std::round(boundingBox[1] - (color_background_size[2] + rny1 * (color_background_size[3] - color_background_size[2]))),
              (int)std::round(boundingBox[2] + (color_background_size[0] + rnx2 * (color_background_size[1] - color_background_size[0]))),
              (int)std::round(boundingBox[3] + (color_background_size[2] + rny2 * (color_background_size[3] - color_background_size[2])))};
          // fprintf(stdout, "%d %d %d %d -> %d %d %d %d\n", boundingBox[0], boundingBox[1], boundingBox[2], boundingBox[3], backgroundBoundingBox[0],
          //        backgroundBoundingBox[1], backgroundBoundingBox[2], backgroundBoundingBox[3]);
          if (bitsAllocated == 16)
          {
            signed short *bvals = (signed short *)buffer;
            for (int yi = 0; yi < ymax; yi++)
            {
              for (int xi = 0; xi < xmax; xi++)
              {
                if (xi < backgroundBoundingBox[0] || xi > backgroundBoundingBox[2] || yi < backgroundBoundingBox[1] || yi > backgroundBoundingBox[3])
                  continue;
                int newx = xi;
                int newy = yi;
                int idx = newy * xmax + newx;
                if (newx < 0 || newx >= xmax || newy < 0 || newy >= ymax)
                  continue;
                // for the background color 255 means white (so max value), 0 mean black so min value
                bvals[idx] = (signed short)std::max(
                    0.0f, std::min(current_image_max_value, current_image_min_value + ((1.0f * color_background_color[0]) / (1.0f * 255) *
                                                                                       (current_image_max_value - current_image_min_value))));
              }
            }
          }
          else if (bitsAllocated == 8)
          {
            unsigned char *bvals = (unsigned char *)buffer;
            for (int yi = 0; yi < ymax; yi++)
            {
              for (int xi = 0; xi < xmax; xi++)
              {
                if (xi < backgroundBoundingBox[0] || xi > backgroundBoundingBox[2] || yi < backgroundBoundingBox[1] || yi > backgroundBoundingBox[3])
                  continue;
                int newx = xi;
                int newy = yi;
                int idx = newy * xmax + newx;
                if (newx < 0 || newx >= xmax || newy < 0 || newy >= ymax)
                  continue;
                // for the background color 255 means white (so max value), 0 mean black so min value
                bvals[idx] = (signed short)std::max(
                    0.0f, std::min(current_image_max_value, current_image_min_value + ((1.0f * color_background_color[0]) / (1.0f * 255) *
                                                                                       (current_image_max_value - current_image_min_value))));
              }
            }
          }
          else
          {
            fprintf(stderr, "Warning: bitAllocation unsupported %d.\n", bitsAllocated);
          }
        }

        // draw the text
        if (!leaveWithoutText)
        {
          // in order to scale the color by the vary_percent value we can reduce the alpha_blend value by the percentage relative to zero
          // vary_percent is ment to have a uniform distribution between the max and (max-vary_percent)
          float reduce_by = ((double) std::rand() / (RAND_MAX)) * (vary_percent/100.0f);
          if (bitsAllocated == 16)
          {
            signed short *bvals = (signed short *)buffer;
            for (int yi = 0; yi < HEIGHT; yi++)
            {
              for (int xi = 0; xi < WIDTH; xi++)
              {
                if (image_buffer[yi][xi] == 0)
                  continue;
                // I would like to copy the value from image over to
                // the buffer. At some good location...
                int newx = px + xi;
                int newy = py + yi;
                int idx = newy * xmax + newx;
                if (newx < 0 || newx >= xmax || newy < 0 || newy >= ymax)
                  continue;
                if (image_buffer[yi][xi] == 0)
                  continue;

                // instead of blending we need to use a fixed overlay color
                // we have image information between current_image_min_value and current_image_max_value
                // we need to scale the image_buffer by those values.
                float f = 0;
                // float v = (f * bvals[idx]) + ((1.0 - f) * ((1.0 * image_buffer[yi][xi]) / 512.0 * current_image_max_value));
                float v = (1.0f * image_buffer[yi][xi] / 255.0)-reduce_by; // 0 to 1 for color, could be inverted if we have a white background
                // clamp to 0 to 1
                v = std::max(0.0f, std::min(1.0f, v));
                float w = 1.0f * bvals[idx] / current_image_max_value;
                float alpha_blend = (v + w * (1.0f - v));
                // should be random value now not exceeding (vary_percent/100)
                if (color_pen_color[0] == 0)
                { // instead of white on black do now black on white
                  alpha_blend = 1.0f - v;
                }

                // fprintf(stdout, "%d %d: %d\n", xi, yi, bvals[idx]);
                bvals[idx] = (signed short)std::max(
                    0.0f, std::min(current_image_max_value, current_image_min_value + (alpha_blend) * (current_image_max_value - current_image_min_value)));
                // fprintf(stdout, "%d %d: %d\n", xi, yi, bvals[idx]);
              }
            }
          }
          else if (bitsAllocated == 8)
          {
            unsigned char *bvals = (unsigned char *)buffer;
            for (int yi = 0; yi < HEIGHT; yi++)
            {
              for (int xi = 0; xi < WIDTH; xi++)
              {
                if (image_buffer[yi][xi] == 0)
                  continue;
                // I would like to copy the value from image over to
                // the buffer. At some good location...
                int newx = px + xi;
                int newy = py + yi;
                int idx = newy * xmax + newx;
                if (newx < 0 || newx >= xmax || newy < 0 || newy >= ymax)
                  continue;
                if (image_buffer[yi][xi] == 0)
                  continue;

                // instead of blending we need to use a fixed overlay color
                // we have image information between current_image_min_value and current_image_max_value
                // we need to scale the image_buffer by those values.
                float f = 0;
                // float v = (f * bvals[idx]) + ((1.0 - f) * ((1.0 * image_buffer[yi][xi]) / 512.0 * current_image_max_value));
                float v = 1.0f * image_buffer[yi][xi] / 255.0; // 0 to 1 for color, could be inverted if we have a white background
                float w = 1.0f * bvals[idx] / current_image_max_value;
                float alpha_blend = (v + w * (1.0f - v));
                if (color_pen_color[0] == 0)
                { // instead of white on black do now black on white
                  alpha_blend = 1.0f - v;
                }

                // fprintf(stdout, "%d %d: %d\n", xi, yi, bvals[idx]);
                bvals[idx] = (signed short)std::max(
                    0.0f, std::min(current_image_max_value, current_image_min_value + (alpha_blend) * (current_image_max_value - current_image_min_value)));
                // fprintf(stdout, "%d %d: %d\n", xi, yi, bvals[idx]);
              }
            }
          }
        }
        // we have a bounding box now for the text on this picture
        if (boundingBox[0] == INT_MAX)
        {
          // makefprintf(stderr, "Warning: No bounding box found [%d].\n", i);
          continue;
        }
        if (verbose)
          fprintf(stdout, "  bounding box: %d %d %d %d\n", boundingBox[0], boundingBox[1],
                  boundingBox[2], boundingBox[3]);
        // bbox.insert(files[pickImageIdx].c_str()
        json bbox = json::object();
        std::ostringstream o;
        std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
        for (int a = 0; a < text2print.size(); a++)
        {
          o << conv.to_bytes(text2print[a]);
          // sprintf(str_buf, "%s%n", str_buf, text2print[a].c_str());
          //  bbox.name += text2print[a].c_str();
        }
        // bbox["text"] = json::parse(text2print);
        bbox["name"] = leaveWithoutText ? std::string("") : std::string(o.str());
        bbox["xmin"] = boundingBox[0];
        bbox["ymin"] = boundingBox[1];
        bbox["xmax"] = boundingBox[2];
        bbox["ymax"] = boundingBox[3];
        bbox["x"] = (int)(boundingBox[0] + std::round((boundingBox[2] - boundingBox[0]) / 2)); // center of bounding box
        bbox["y"] = (int)(boundingBox[1] + std::round((boundingBox[3] - boundingBox[1]) / 2));
        bbox["width"] = (int)(std::round((boundingBox[2] - boundingBox[0]))); // why is this half the distance?
        bbox["height"] = (int)(std::round((boundingBox[3] - boundingBox[1])));
        bbox["imagewidth"] = xmax; // of the image
        bbox["imageheight"] = ymax;
        bbox["class"] = leaveWithoutText ? "background" : "text";
        bbox["filename_source"] = boost::filesystem::path(files[pickImageIdx]).filename().string();
        bbox["filename"] = boost::filesystem::path(outputfilename).filename().string();
        bbox["draw_background_box"] = (fabs(color_background_color[3]) > 1e-6);

        // we can also get bounding boxes that don't have an area (height or width == 0 due to rounding)
        if (bbox["width"] < 1 || bbox["height"] < 1) // in pixel correct?
          continue;
        bboxes[bboxes.size()] = bbox;

        // boundingBoxes.insert(std::pair<std::string, std::map<std::string, std::string>>(
        //    (files[pickImageIdx] + bb_count).c_str(), bbox));
      }
    }
    // In rare cases none of the bounding boxes ever get written. We would still produce an output image,
    // so we need at least one empty mention of that image in the boundingBoxes file.
    if (bboxes.size() == 0)
    {
      fprintf(stderr, "Warning: empty bounding box array for image %d.\n", i);
      json bbox = json::object();
      bbox["name"] = std::string("");
      bbox["xmin"] = NAN;
      bbox["ymin"] = NAN;
      bbox["xmax"] = NAN;
      bbox["ymax"] = NAN;
      bbox["x"] = NAN;
      bbox["y"] = NAN;
      bbox["width"] = 0;
      bbox["height"] = 0;
      bbox["imagewidth"] = xmax; // of the image
      bbox["imageheight"] = ymax;
      bbox["filename_source"] = boost::filesystem::path(files[pickImageIdx]).filename().string();
      bbox["filename"] = boost::filesystem::path(outputfilename).filename().string();
      bbox["class"] = "background";
      bboxes[bboxes.size()] = bbox;
    }

    // store the bboxes as a txt file in the same directory (with_text)
    snprintf(outputfilename, 1024 - 1, "%s/with_text/%s%08d.txt", output.c_str(), batch_string.c_str(), i);
    // open this file and save all the bboxes with some number for the label
    std::ofstream myfile;
    myfile.open (outputfilename);
    for (int bidx = 0; bidx < bboxes.size(); bidx++) {
      if (bboxes[bidx]["class"] != "background") {
        int label = 1;
        float x_01 = std::int64_t(bboxes[bidx]["x"]) / (xmax-1.0);
        float y_01 = std::int64_t(bboxes[bidx]["y"]) / (ymax-1.0);
        float w_01 = std::int64_t(bboxes[bidx]["width"]) / (xmax-1.0);
        float h_01 = std::int64_t(bboxes[bidx]["height"]) / (ymax-1.0);
        myfile << label << " " << x_01 << " " << y_01 << " " << w_01 <<  " " << h_01 << "\n";
      }
    }
    myfile.close();

    // store all the bounding boxes of the current target image
    // we could pick the same file here twice, that would overwrite the
    // older value and we end up with missing bounding boxes
    if (boundingBoxes.contains(files[pickImageIdx]))
    {
      int cc = 1;
      std::string unique_name;
      do
      {
        unique_name = files[pickImageIdx] + "_" + std::to_string(cc);
        cc++;
      } while (boundingBoxes.contains(unique_name));

      boundingBoxes[unique_name] = bboxes;
    }
    else
    {
      boundingBoxes[files[pickImageIdx]] = bboxes;
    }
    bboxCounter++;

    // now we can add the bitmap to the original data and write again
    // change_image.SetBuffer(buffer);
    // gdcm::DataElement pixeldata = change_image.GetDataElement();
    gdcm::DataElement pixeldata(gdcm::Tag(0x7fe0, 0x0010));
    pixeldata.SetByteValue(buffer, len);

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

    if (target_type == "dcm")
    {
      snprintf(outputfilename, 1024 - 1, "%s/with_text/%s%08d.dcm", output.c_str(), batch_string.c_str(), i);
      gdcm::ImageWriter writer;
      writer.SetImage(*im);
      writer.SetFile(reader.GetFile());
      // snprintf(outputfilename, 1024 - 1, "%s/with_text/%08d.dcm", output.c_str(), i);
      writer.SetFileName(outputfilename);
      if (!writer.Write())
      {
        return 1;
      }
    }
    else
    {
      snprintf(outputfilename, 1024 - 1, "%s/with_text/%s%08d.png", output.c_str(), batch_string.c_str(), i);
      rgb16_image_t img(xmax, ymax);
      rgb16_pixel_t rgb(65535, 0, 0);
      // we should copy the values over now --- from the buffer? Or from the DICOM Image
      // fill_pixels(view(img), red);
      // stretch the intensities from 0 to max for png (0...65535)
      float pmin = current_image_min_value;
      float pmax = current_image_max_value;
      auto v = view(img);
      auto it = v.begin();
      if (bitsAllocated == 16)
      {
        signed short *bvals = (signed short *)buffer;
        while (it != v.end())
        {
          //++hist[*it];
          float pixel_val = ((1.0 * bvals[0]) - pmin) / (pmax - pmin);
          *it = rgb16_pixel_t{(short unsigned int)(pixel_val * 65535), (short unsigned int)(pixel_val * 65535), (short unsigned int)(pixel_val * 65535)};
          bvals++;
          it++;
        }
        write_view(outputfilename, const_view(img), png_tag{});
      }
      else if (bitsAllocated == 8)
      {
        rgb8_image_t img8(xmax, ymax);
        auto v = view(img8);
        auto it = v.begin();
        unsigned char *bvals = (unsigned char *)buffer;
        while (it != v.end())
        {
          //++hist[*it];
          float pixel_val = ((1.0 * bvals[0]) - pmin) / (pmax - pmin);
          // fprintf(stdout, "Pixel value: %f %d\n", pixel_val, (unsigned char)(pixel_val*255));
          *it = rgb8_pixel_t{(unsigned char)(pixel_val * 255), (unsigned char)(pixel_val * 255), (unsigned char)(pixel_val * 255)};
          bvals++;
          it++;
        }
        write_view(outputfilename, const_view(img8), png_tag{});
      }
    }
    delete[] buffer;
  }
  FT_Done_FreeType(library);

  // safe the boundingBoxes dataset now as well as a json
  // std::u32string res = boundingBoxes.dump(4);
  std::locale::global(std::locale(""));
  std::ofstream out(output + "/boundingBoxes.json", std::ofstream::out | std::ofstream::trunc);
  //  std::locale utf8_locale(std::locale(), new utf8cvt<false>);
  //  out.imbue(utf8_locale);
  out << std::setw(4) << boundingBoxes << std::endl;
  out.close();

  // create an annotations folder for the output path
  // only if we are exporting png we should do this:
  if (target_type == "png")
  {
    dn = output + "/annotations/";
    if (!(stat(dn.c_str(), &buf) == 0))
    {
      mkdir(dn.c_str(), 0777);
    }
    exportBoundingBoxesAsXML(boundingBoxes, dn, output + "/with_text/");
  }

  // we should also write a new style dataset.yml file
  std::ofstream out2(output + "/dataset.yaml", std::ofstream::out | std::ofstream::trunc);
  out2 << std::setw(4) << "path: " << output << std::endl;
  out2 << "train: " << "with_text/" << std::endl;
  out2 << "# No split has been performed yet, copy some of the with_text image/txt pairs to the val folder" << std::endl;
  out2 << "#val: " << "val/" << std::endl;
  out2 << "nc: 1" << std::endl;
  out2 << "names: [ 'foreground' ]" << std::endl;
  out2.close();




  // test writing png
  // rgb8_image_t img(512, 512);
  // rgb8_pixel_t red(255, 0, 0);
  // fill_pixels(view(img), red);
  // write_view("redsquare.png", const_view(img), png_tag{});

  return 0;
}

/* EOF */
