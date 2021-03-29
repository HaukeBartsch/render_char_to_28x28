### Render a text into a pixel area

This program uses the freetype library and gdcm to render lines of characters in a given font into a series of DICOM images.

![Example](images/example.png)

The output generates two folders with numbered files. One folder contains copies of input images and the second folder contains the same images with random text placed on them. The location of each line of text placed on each image is stored in a file called boundingBoxes.json.

In order to control the generation of images a json control file is used. The file mimics some usual placements of text on medical images. This includes text in the 4 corners of the image and some short text in the middle. The file also specifies different fonts that should be used as well as font sizes.

```{json}
{
  "model": "text in medical images",
  "description": "Describe a sequence that will be sufficient to generate an image with burned in information.",
  "logic": {
    "image_contrasts": [
      {
        "name": "bone",
        "min": [
          -0.2,
          0.2
        ],
        "max": [
          0.8,
          1.2
        ]
      }
    ],
    "font": [
      {
        "name": "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "sizes": [
          6,
          8,
          11,
          12,
          15
        ],
        "face_indexes": [
          0
        ]
      },
      {
        "name": "/root/render_char_to_28x28/data/Menlo.ttc",
        "sizes": [
          6,
          8,
          11,
          12,
          15
        ],
        "face_indexes": [
          0
        ]
      }
    ],
    "placement": {
      "top-left": {
        "x": [
          0.01,
          0.02
        ],
        "y": [
          0.01,
          0.02
        ],
        "repeat-spacing": [
          1,
          2
        ],
        "how-many": [
          0,
          6
        ],
        "lengths": [
          2,
          20
        ],
        "types": [
          "Date",
          "Name",
          "SeriesDescription"
        ]
      },
      "top-right": {
        "x": [
          -0.3,
          -0.28
        ],
        "y": [
          0.01,
          0.02
        ],
        "repeat-spacing": [
          1,
          2
        ],
        "how-many": [
          0,
          6
        ],
        "lengths": [
          2,
          20
        ],
        "types": [
          "Date",
          "Name",
          "SeriesDescription"
        ]
      },
      "bottom-left": {
        "x": [
          0.02,
          0.03
        ],
        "y": [
          -0.06,
          -0.04
        ],
        "repeat-spacing": [
          1,
          2
        ],
        "how-many": [
          0,
          6
        ],
        "lengths": [
          2,
          20
        ],
        "types": [
          "Date",
          "Name",
          "SeriesDescription"
        ]
      },
      "bottom-right": {
        "x": [
          -0.3,
          -0.2
        ],
        "y": [
          -0.06,
          -0.04
        ],
        "repeat-spacing": [
          1,
          2
        ],
        "how-many": [
          0,
          6
        ],
        "lengths": [
          2,
          20
        ],
        "types": [
          "Date",
          "Name",
          "SeriesDescription"
        ]
      },
      "somewhere": {
        "x": [
          0.2,
          0.8
        ],
        "y": [
          0.2,
          0.8
        ],
        "repeat-spacing": [
          1,
          2
        ],
        "how-many": [
          1,
          2
        ],
        "lengths": [
          1,
          4
        ],
        "types": [
          "Date",
          "Name",
          "SeriesDescription"
        ]
      }
    },
    "types": [
      {
        "name": "Name",
        "generator": "[A-Z][a-z,_-]+"
      },
      {
        "name": "Date",
        "generator": "[0-9][0-9][-/][0-9][0-9][-/][0-9][0-9][0-9][0-9]"
      },
      {
        "name": "SeriesDescription",
        "generator": "[A-Z][a-z]+[ ,.-]+"
      }
    ]
  }
}
```
### Limitations

The input files are assumed to be grayscale 16bit or 8bit (DICOM, MONOCHROME2). The output can be either DICOM or png (16-bit, grayscale stored as color). The "types" specification in the file above is not yet implemented - only random text is supported currently.


## Build the program

The build can be done in a docker container that contains all the required libraries:
```
docker build -t render_char_to_28x28 build
```
Once the container is build, run the executable by providing a folder with DICOM data and an output folder. Both can be mounted to your machine with:

```
docker run --rm -it \
    -v /input/data/folder:/data \
    -v /output/data/folder:/output \
    render_char_to_28x28 /bin/bash -c \
  "/root/render_char_to_28x28/renderText -d /data -s png -o /output -c forwardModel.json -t 1"
```
The above will generate one pair of images in png format (16-bit color).

Additionally to the information in the forwardModel.json for text placement some command line options can be used to specify for example how many image pairs should be generated.

```
root@xxxx:~/render_char_to_28x28# ./renderText 
Create deep learning data from collection of DICOM files by adding text
annotations.
USAGE: renderText [options]

Options:
  --help            Print this help message.
  --dicom, -d       Directory with DICOM files.
  --output, -o      Output directory.
  --fontfile, -f    Name of the font to be used. Also specific in the config
                    file.
  --targetnum, -t   Number of image pairs to be created. Images are chosen at
                    random from the DICOM folder.
  --config, -c      Configuration file for text generation.
  --export_type, -e File format for output ("dcm" by default, or "png"). Only if
                    the png option is used the PASCAL VOC annotation folder with
                    xml files will be created.
  --random_seed, -s Specify random seed for text placement (default random).
  --batch, -b       A string prepended to the numbered files to allow for
                    repeated creation of samples without overwrite.
  --verbose, -v     Verbose output.
  --multiclass, -m  Provide examples for text ("text") and for non-text
                    ("background") objects. Default is that only bounding boxes
                    for actual text are generated.

Examples:
  ./renderText -d data/LIDC-IDRI-0009 -c forwardModel.json \
               -o /tmp/bla -e png -t 100 -v -m
  ./renderText --help
```

Some control over the text and placement on the DICOM images is part of the forwardModel.json control file. Most of the options listed in the control file are now implemented.

Tip: Convert the bounding box json file to a csv by:
```
jq -r '[.[][]] | (map(keys) | add | unique) as $cols | map(. as $row | $cols | map($row[.])) as $rows | $cols, $rows[] | @csv' boundingBoxes.json > boundingBoxes.csv
```

## Output format

In case that the png format is selected as the output format the program will assume that the user also wants the bounding box information in a separate folder annotations/ in the PASCAL VOC XML format. This format consists of a separate xml file for each image file with the information on the annotations placed on the image.

# Convert png files to DICOM

The render_char_to_28x28 program requires DICOM files as a source of the underlying image information. Some of the sources for such data contain only PNG file format coded files. Usually this is done because DICOM meta data can contain sensitive information and the developers of the image resources don't want to or don't know how to anonymize that kind of information. The png162dcm program which is part of this repository can be used to convert such PNG files into DICOM files with a minimal header suitable for render_char_to_28x28.

```
Convert png images (16bit) to DICOM with a random header.
USAGE: png162dcm [options]

Options:
  --help        Print this help message.
  --input, -i   Input PNG file.
  --output, -o  Output directory, if not specified the input folder will be used
                instead.

Examples:
  ./png162dcm -i data/test.png -o /tmp/
  ./png162dcm --help
```