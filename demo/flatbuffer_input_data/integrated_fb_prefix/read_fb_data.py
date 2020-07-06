import flatbuffers
from flatbuffers import util
import os
import sys
import argparse

sys.path.append("..")
sys.path.append("../..")

import generated.VW.parsers.flatbuffer.Example as e
import  generated.VW.parsers.flatbuffer.AnotherExample as ae
import generated.VW.parsers.flatbuffer.FinalExample as fe
import generated.VW.parsers.flatbuffer.ExampleType as et

def deserialize_and_print(from_file):
    with open(from_file, 'rb') as data_file:
        while True:
            # read first 32 bits holding the object size
            prefix = data_file.read(4)
            if len(prefix) <= 0:
                print("Done reading file")
                break
            prefix_obj_size = int.from_bytes(prefix, byteorder='little')

            # read the rest of the object to be deserialized
            buf = data_file.read(prefix_obj_size)
            buf = bytearray(buf)
            final_example = fe.FinalExample.GetRootAsFinalExample(buf, 0)
            tentative_example = final_example.Example()
            if final_example.ExampleType() == et.ExampleType().Example:
                example = e.Example()
                example.Init(tentative_example.Bytes, tentative_example.Pos)
                # examples currently have only one namespace
                ns = example.Namespaces(0)
                print(f"namespace: {ns.Name()}")
                for j in range(ns.FeaturesLength()):
                    feature = ns.Features(j)
                    print(f"feature name: {feature.Name()}, feature value: {feature.Value()}")
            elif final_example.ExampleType() == et.ExampleType().AnotherExample:
                example = ae.AnotherExample()
                example.Init(tentative_example.Bytes, tentative_example.Pos)
                print(f"example name: {example.Name()}, example value: {example.Value()}")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--from-file",
        help="flatbuffer file to deserialize",
        required=True,
    )

    args = parser.parse_args()
    deserialize_and_print(args.from_file)


if __name__ == "__main__":
    main()
