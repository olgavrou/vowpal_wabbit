import flatbuffers
from flatbuffers import util
import os
import sys
import argparse
from itertools import chain

sys.path.append("..")
sys.path.append("../..")

from generated.VW.parsers.flatbuffer.Label import *
from generated.VW.parsers.flatbuffer.Namespace import *
from generated.VW.parsers.flatbuffer.Feature import *
from generated.VW.parsers.flatbuffer.Example import *
from generated.VW.parsers.flatbuffer.AnotherExample import *
from generated.VW.parsers.flatbuffer.FinalExample import *
from generated.VW.parsers.flatbuffer.ExampleType import *
from generated.VW.parsers.flatbuffer.SimpleLabel import *

with open('data1.fb', 'rb') as data_file:
    while True:
        # read first 32 bits holding the object size
        buf_ojb_size = data_file.read(4)
        if len(buf_ojb_size) <= 0:
            print("Done reading file")
            break
        osiz = int.from_bytes(buf_ojb_size, byteorder='little')

        # read the rest of the object to be deserialized
        buf = data_file.read(osiz)
        buf = bytearray(buf)
        fe = FinalExample.GetRootAsFinalExample(buf, 0)
        et = fe.Example()
        if fe.ExampleType() == ExampleType().Example:
            e = Example()
            e.Init(et.Bytes, et.Pos)
            # examples currently have only one namespace
            ns = e.Namespaces(0)
            print(f"namespace: {ns.Name()}")
            for j in range(ns.FeaturesLength()):
                f = ns.Features(j)
                print(f"feature name: {f.Name()}, feature value: {f.Value()}")
        elif fe.ExampleType() == ExampleType().AnotherExample:
            e = AnotherExample()
            e.Init(et.Bytes, et.Pos)
            name = e.Name()
            value = e.Value()
            print(f"example name: {slot}, example value: {value}")