#include <vector>
#include "generated/example_generated.h"

#include <sstream>
#include <fstream>
#include <string>
#include <iostream>

using namespace VW::parsers::flatbuffer;

enum class object_type
{
  example = 0,
  example_collection
};

struct feature
{
  std::string name;
  float value;
};

struct simple_example
{
  float label;
  float weight;
  std::string ns;
  std::vector<feature> features;
};

simple_example parse_line_simple(const std::string& line)
{
  std::string fn, label_weight, fv;
  std::istringstream iss(line);

  iss >> label_weight >> fn;

  auto label = std::stof(label_weight.substr(0, label_weight.find(":")));
  float weight = 1;
  if (label_weight.find(":") != std::string::npos)
  { weight = std::stof(label_weight.substr(label_weight.find(":") + 1)); }

  std::string ns;
  if (fn != "|") { ns = fn.substr(fn.find("|") + 1); }

  std::vector<feature> features;
  while (iss >> fv)
  {
    auto name = fv.substr(0, fv.find(":"));
    float value = 1;
    if (fv.find(":") != std::string::npos) { value = std::stof(fv.substr(fv.find(":") + 1)); }
    features.emplace_back(feature{name, value});
  }

  return {label, weight, ns, features};
}

auto create_example(flatbuffers::FlatBufferBuilder& builder, simple_example& simple_example)
{
  std::vector<flatbuffers::Offset<Namespace>> namespaces;
  std::vector<flatbuffers::Offset<Feature>> fts;

  auto label = CreateSimpleLabel(builder, simple_example.label, simple_example.weight).Union();

  for (auto& feature : simple_example.features)
  { fts.push_back(CreateFeatureDirect(builder, feature.name.c_str(), feature.value)); }
  namespaces.push_back(CreateNamespaceDirect(builder, simple_example.ns.c_str(), 0, &fts));
  return CreateExampleDirect(
      builder, &namespaces, Label_SimpleLabel, label, "");
}

flatbuffers::Offset<ExampleRoot> create_example_obj(
    flatbuffers::FlatBufferBuilder& builder, simple_example& simple_example)
{
  auto ex = create_example(builder, simple_example);
  return CreateExampleRoot(builder, ExampleType_Example, ex.Union());
}

auto create_example_collection_obj(flatbuffers::FlatBufferBuilder& builder, simple_example& simple_example)
{
  // collection with only one example
  std::vector<flatbuffers::Offset<Example>> examplecollection;
  auto ex = create_example(builder, simple_example);
  examplecollection.push_back(std::move(ex));
  auto ex_collection = CreateExampleCollectionDirect(builder, &examplecollection);

  return CreateExampleRoot(
      builder, ExampleType_ExampleCollection, ex_collection.Union());
}

void print_example(const Example* example)
{
  auto namespaces_size = example->namespaces()->Length();
  for (size_t i = 0; i < namespaces_size; i++)
  {
    auto ns = example->namespaces()->Get(i);
    auto features_size = ns->features()->Length();
    std::cout << "namespace: " << ns->name()->c_str() << std::endl;
    for (size_t j = 0; j < features_size; j++)
    {
      auto ft = ns->features()->Get(j);
      std::cout << "feature name: " << ft->name()->c_str() << " feature value: " << ft->value() << std::endl;
    }
  }
}