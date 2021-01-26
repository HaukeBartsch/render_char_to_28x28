### Render a text into a pixel area

This program uses the freetype library and gdcm to render a character in a given font into a series of DICOM images.

## Build the program

The build can be done in a docker container that contains all the required libraries:
```
docker build -t render_char_to_28x28 build
```
Once the container is build you can run the executable with:

```
docker run --rm -it \
    -v /input/data/folder:/data \
    -v /output/data/folder:/output \
    render_char_to_28x28 /bin/bash -c \
       "renderText -d /data -o /output/ -c forwardModel.json -t 1"
```
