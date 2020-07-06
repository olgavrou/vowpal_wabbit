import flatbuffers
import os
import sys
import argparse

sys.path.append("..")
sys.path.append("../..")

import generated.VW.parsers.flatbuffer.Example as e
import  generated.VW.parsers.flatbuffer.AnotherExample as ae
import generated.VW.parsers.flatbuffer.FinalExample as fe
import generated.VW.parsers.flatbuffer.ExampleType as et
import generated.VW.parsers.flatbuffer.Label as lbl
import generated.VW.parsers.flatbuffer.Namespace as ns
import generated.VW.parsers.flatbuffer.Feature as ft
import generated.VW.parsers.flatbuffer.SimpleLabel as sl


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
        with open(to_file, "w+b") as out_f:
            builder = flatbuffers.Builder(1024)

            line_count = 0
            for line in data_file:
                line_count += 1
                label, weight, name_space, fts = parse_simple(line)
                # per line
                # create a label
                sl.SimpleLabelStart(builder)
                sl.SimpleLabelAddLabel(builder, label)
                sl.SimpleLabelAddWeight(builder, weight)
                fb_label = sl.SimpleLabelEnd(builder)
                # careful not to create nested builds with builder
                # create feature vector objects
                feature_objs = []
                for name, value in fts:
                    # create name string before starting to create Feature
                    fb_name = builder.CreateString(name)
                    ft.FeatureStart(builder)
                    ft.FeatureAddName(builder, fb_name)
                    ft.FeatureAddValue(builder, value)
                    feature_objs.append(ft.FeatureEnd(builder))
                # create vector of features
                fb_fts_vector = create_flatbuffer_vector(
                    feature_objs, builder, ns.NamespaceStartFeaturesVector
                )

                fb_ns = builder.CreateString(name_space)
                # create a namespace and put in the features
                ns.NamespaceStart(builder)
                ns.NamespaceAddName(builder, fb_ns)
                ns.NamespaceAddFeatures(builder, fb_fts_vector)
                ns_obj = ns.NamespaceEnd(builder)
                # create an example with the ns and label
                # create all nested objects first
                empty_tag = builder.CreateString("")
                e.ExampleStartNamespacesVector(builder, 1)
                builder.PrependUOffsetTRelative(ns_obj)
                fb_namespaces = builder.EndVector(1)

                e.ExampleStart(builder)
                e.ExampleAddNamespaces(builder, fb_namespaces)
                e.ExampleAddLabel(builder, fb_label)
                e.ExampleAddLabelType(builder, lbl.Label().SimpleLabel)
                e.ExampleAddTag(builder, empty_tag)
                example = e.ExampleEnd(builder)

                fe.FinalExampleStart(builder)
                fe.FinalExampleAddExampleType(builder, et.ExampleType.Example)
                fe.FinalExampleAddExample(builder, example)
                final_example = fe.FinalExampleEnd(builder)

                # finish building and store to file
                builder.FinishSizePrefixed(final_example)
                buf = builder.Output()
                print(f"Writing buf of size {len(buf)}")
                out_f.write(buf)
            
            # dummy another example
            name = builder.CreateString("example name")
            ae.AnotherExampleStart(builder)
            ae.AnotherExampleAddName(builder, name)
            ae.AnotherExampleAddValue(builder, 1.)
            example = ae.AnotherExampleEnd(builder)

            fe.FinalExampleStart(builder)
            fe.FinalExampleAddExampleType(builder, et.ExampleType.AnotherExample)
            fe.FinalExampleAddExample(builder, example)
            final_example = fe.FinalExampleEnd(builder)
            # finish building and store to file
            builder.FinishSizePrefixed(final_example)
            buf = builder.Output()
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
