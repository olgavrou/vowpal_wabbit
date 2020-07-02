This directory contains simple examples to demonstrate how to create flatbuffer files using either cpp or python.

The demos read a simple text file containing simple examples of the format:

"0 |f price:.23 sqft:.25 age:.05"

The data1.txt file was used in developing these examples.

The only thing that needs to be set while building these demos is the environmental variable `FB_EXAMPLE_SCHEMA` which needs to point to the `example.fbs` flatbuffer schema file currently located under `vowpalwabbit/parser/flatbuffer/schema`. It is important to build first before executing the python demo, as cmake will generate the cpp and python files needed to create/consume flatbuffer files using the `example.fbs` schema. These files will be generated under a directory called `generated` under the current dir.

## Prerequisites
flatbuffers needs to be installed

## Build

build:

 - mkdir build
 - cd build
 - cmake ..
 - make

## Running the cpp demo

Run the cpp demo from the `build` dir:
 - ./create-fb-data.out <path_to_input_file> <path_to_output_file>

This will take the input file containing the txt data and create a flatbuffer file with the data in flatbuffer format.

## Running the python demo

Run the python demo (python3 only) from the `build` dir:
 - python3 ../create_fb_data.py --from-file <path_to_input_file> --to-file <path_to_output_file>

This will behave the same way as the cpp example.

## Inspect the flatbuffer file

You can inspect the flatbuffer file easily by transforming it into json:

- flatc --json --raw-binary <path_to_example.fbs> -- <path_to_fb_file>

This will create a json file `<fb_file_name>.json` which you can easily inspect