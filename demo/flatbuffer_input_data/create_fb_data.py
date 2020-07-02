import flatbuffers
import os
import sys
import argparse
from itertools import chain

sys.path.append("..")
sys.path.append("../..")

from generated.VW.parsers.flatbuffer.Label import *
from generated.VW.parsers.flatbuffer.Namespace import *
from generated.VW.parsers.flatbuffer.Feature import *
from generated.VW.parsers.flatbuffer.ExampleCollection import *
from generated.VW.parsers.flatbuffer.Example import *
from generated.VW.parsers.flatbuffer.SimpleLabel import *

builder = flatbuffers.Builder(1024)
ex_colls = []


def create_flatbuffer_vector(original_list, builder, _vector_type_):
    _vector_type_(builder, len(original_list))
    for item in reversed(original_list):
        builder.PrependUOffsetTRelative(item)
    return builder.EndVector(len(original_list))


def parse_simple(line):
    line = line.strip()
    default_value = [1]

    label_weight, pipe_ns, *features = line.split(" ")
    # if : missing use default value for weight
    label, weight = (label_weight.split(":") + default_value)[:2]
    ns = pipe_ns.split("|")[1] if pipe_ns != "|" else ""

    fts = []
    for feature in features:
        # if : missing use default value for feature value
        name, value = (feature.split(":") + default_value)[:2]
        fts.append((name, float(value)))
    return float(label), float(weight), ns, fts


def convert(from_file, to_file):
    with open(from_file, "r") as data_file:
        line_count = 0
        for line in data_file:
            line_count += 1
            label, weight, ns, fts = parse_simple(line)
            # per line
            # create a label
            SimpleLabelStart(builder)
            SimpleLabelAddLabel(builder, label)
            SimpleLabelAddWeight(builder, weight)
            fb_label = SimpleLabelEnd(builder)
            # careful not to create nested builds with builder
            # create feature vector objects
            feature_objs = []
            for name, value in fts:
                # create name string before starting to create Feature
                fb_name = builder.CreateString(name)
                FeatureStart(builder)
                FeatureAddName(builder, fb_name)
                FeatureAddValue(builder, value)
                feature_objs.append(FeatureEnd(builder))
            # create vector of features
            fb_fts_vector = create_flatbuffer_vector(
                feature_objs, builder, NamespaceStartFeaturesVector
            )

            fb_ns = builder.CreateString(ns)
            # create a namespace and put in the features
            NamespaceStart(builder)
            NamespaceAddName(builder, fb_ns)
            NamespaceAddFeatures(builder, fb_fts_vector)
            ns_obj = NamespaceEnd(builder)
            # create an example with the ns and label
            # create all nested objects first
            empty_tag = builder.CreateString("")
            ExampleStartNamespacesVector(builder, 1)
            builder.PrependUOffsetTRelative(ns_obj)
            fb_namespaces = builder.EndVector(1)

            ExampleStart(builder)
            ExampleAddNamespaces(builder, fb_namespaces)
            ExampleAddLabel(builder, fb_label)
            ExampleAddLabelType(builder, Label().SimpleLabel)
            ExampleAddTag(builder, empty_tag)
            # append to ex_colls list
            ex_colls.append(ExampleEnd(builder))

        # create example collection
        fb_examples = create_flatbuffer_vector(
            ex_colls, builder, ExampleCollectionStartExamplesVector
        )
        ExampleCollectionStart(builder)
        ExampleCollectionAddExamples(builder, fb_examples)
        fb_ex_collection = ExampleCollectionEnd(builder)

        # finish building and store to file
        builder.Finish(fb_ex_collection)
        buf = builder.Output()

        with open(to_file, "w+b") as out_f:
            out_f.write(buf)
    print(
        f"Done converting [{line_count}] lines from text file [{from_file}] to flatbuffer file [{to_file}]"
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--from-file",
        help="path to text file with simple examples in vw format",
        required=True,
    )
    parser.add_argument(
        "--to-file", help="path to flatbuffer output file", required=True
    )

    args = parser.parse_args()
    convert(args.from_file, args.to_file)


if __name__ == "__main__":
    main()
