import argparse
import flatbuffers

import os
import sys
sys.path.append("..")
sys.path.append("../..")

import convert_to_fb as tofb

def convert(from_file, to_file):
    with open(from_file, "r") as data_file:
        with open(to_file, "w+b") as out_f:
            builder = flatbuffers.Builder(1024)

            line_count = 0
            for line in data_file:
                line_count += 1
                simple_example = tofb.parse_simple(line)
                root_ex_obj = tofb.create_example_obj(builder, simple_example)
                
                # finish building and store to file
                builder.FinishSizePrefixed(root_ex_obj)
                buf = builder.Output()
                print(f"Writing buf of size {len(buf)}")
                out_f.write(buf)

                # create collection out of example
                root_coll_obj = tofb.create_example_collection_obj(builder, simple_example)
                # finish building and store to file
                builder.FinishSizePrefixed(root_coll_obj)
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
