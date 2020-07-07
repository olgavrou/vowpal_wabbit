import flatbuffers
import os
import sys

sys.path.append("..")
sys.path.append("../..")

import generated.VW.parsers.flatbuffer.Example as e
import generated.VW.parsers.flatbuffer.ExampleCollection as ec
import generated.VW.parsers.flatbuffer.ExampleRoot as rt
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


def create_example(builder, simple_example):
    label, weight, name_space, fts = simple_example
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
    return e.ExampleEnd(builder)


def create_example_obj(builder, simple_example):
    example = create_example(builder, simple_example)

    rt.ExampleRootStart(builder)
    rt.ExampleRootAddExampleObjType(builder, et.ExampleType.Example)
    rt.ExampleRootAddExampleObj(builder, example)
    return rt.ExampleRootEnd(builder)


def create_example_collection_obj(builder, simple_example):
    # create example collection with one example
    example = create_example(builder, simple_example)
    ex_colls = [example]

    examples = create_flatbuffer_vector(
        ex_colls, builder, ec.ExampleCollectionStartExamplesVector
    )
    ec.ExampleCollectionStart(builder)
    ec.ExampleCollectionAddExamples(builder, examples)
    fb_collection = ec.ExampleCollectionEnd(builder)

    rt.ExampleRootStart(builder)
    rt.ExampleRootAddExampleObjType(builder, et.ExampleType.ExampleCollection)
    rt.ExampleRootAddExampleObj(builder, fb_collection)
    return rt.ExampleRootEnd(builder)
