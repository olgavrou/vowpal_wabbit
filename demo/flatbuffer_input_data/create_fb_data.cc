#include <vector>
#include "generated/example_generated.h"

#include <sstream>
#include <fstream>
#include <string>
#include <iostream>

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
  {
    weight = std::stof(label_weight.substr(label_weight.find(":") + 1));
  }

  std::string ns;
  if (fn != "|")
  {
    ns = fn.substr(fn.find("|") + 1);
  }

  std::vector<feature> features;
  while (iss >> fv)
  {
    auto name = fv.substr(0, fv.find(":"));
    float value = 1;
    if (fv.find(":") != std::string::npos)
    {
      value = std::stof(fv.substr(fv.find(":") + 1));
    }
    features.emplace_back(feature{name, value});
  }

  return {label, weight, ns, features};
}

void convert(const std::string& from, const std::string& to)
{
  std::ifstream data_file(from);
  std::string line;
  flatbuffers::FlatBufferBuilder builder;
  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::Example>> examplecollection;

  size_t line_count = 0;
  while (std::getline(data_file, line))
  {
    line_count++;
    auto simple_example = parse_line_simple(line);
    std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::Namespace>> namespaces;
    std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::Feature>> fts;

    auto label =
        VW::parsers::flatbuffer::CreateSimpleLabel(builder, simple_example.label, simple_example.weight).Union();

    for (auto& feature : simple_example.features)
    {
      fts.push_back(VW::parsers::flatbuffer::CreateFeatureDirect(builder, feature.name.c_str(), feature.value));
    }
    namespaces.push_back(VW::parsers::flatbuffer::CreateNamespaceDirect(builder, simple_example.ns.c_str(), 0, &fts));
    examplecollection.push_back(VW::parsers::flatbuffer::CreateExampleDirect(
        builder, &namespaces, VW::parsers::flatbuffer::Label_SimpleLabel, label, ""));
  }

  auto fb_collection = VW::parsers::flatbuffer::CreateExampleCollectionDirect(builder, &examplecollection);
  builder.Finish(fb_collection);

  uint8_t* buf = builder.GetBufferPointer();
  int size = builder.GetSize();

  std::ofstream outfile;
  outfile.open(to, std::ios::binary | std::ios::out);

  outfile.write(reinterpret_cast<char*>(buf), size);
  std::cout << "Done converting [" << line_count << "] lines from text file [" << from << "] to flatbuffer file [" << to
            << "]" << std::endl;
}

int main(int argc, char* argv[])
{
  if (argc < 3)
  {
    throw std::invalid_argument(
        "You need to provide two file names as arugments, the file to transform and the output file");
  }

  convert(argv[1], argv[2]);
  return 0;
}