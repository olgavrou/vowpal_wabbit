import sys
import argparse

sys.path.append("..")

import generated.VW.parsers.flatbuffer.Example as e
import generated.VW.parsers.flatbuffer.ExampleCollection as ec
import generated.VW.parsers.flatbuffer.ExampleRoot as er
import generated.VW.parsers.flatbuffer.ExampleType as et

def print_example(example):
    ns = example.Namespaces(0)
    print(f"namespace: {ns.Name()}")
    for j in range(ns.FeaturesLength()):
        feature = ns.Features(j)
        print(f"feature name: {feature.Name()}, feature value: {feature.Value()}")

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

            root_example = er.ExampleRoot.GetRootAsExampleRoot(buf, 0)
            tentative_object = root_example.ExampleObj()
            
            if root_example.ExampleObjType() == et.ExampleType().Example:
                example = e.Example()
                example.Init(tentative_object.Bytes, tentative_object.Pos)
                # examples currently have only one namespace
                print_example(example)
            elif root_example.ExampleObjType() == et.ExampleType().ExampleCollection:
                example_coll = ec.ExampleCollection()
                example_coll.Init(tentative_object.Bytes, tentative_object.Pos)
                coll_size = example_coll.ExamplesLength()
                for i in range(coll_size):
                    ex = example_coll.Examples(i)
                    print_example(example_coll.Examples(i))

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
