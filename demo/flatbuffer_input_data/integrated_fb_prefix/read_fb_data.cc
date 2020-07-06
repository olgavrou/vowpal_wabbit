#include <vector>
#include "generated/example_generated.h"

#include <sstream>
#include <fstream>
#include <string>
#include <iostream>

void deserialize_and_print(const std::string& from)
{
  std::ifstream data_file(from, std::ios::binary | std::ios::in);
  size_t prefix_len = sizeof(uint32_t);
  uint32_t prefix;

  while (!data_file.fail())
  {
    data_file.read((char*)&prefix, prefix_len);
    if (data_file.fail())
    {
      std::cout << "Done reading file" << std::endl;
      break;
    }

    std::vector<char> fb_obj(prefix);
    data_file.read(&fb_obj[0], prefix);

    auto flatbuffer_pointer = reinterpret_cast<u_int8_t*>(&fb_obj[0]);

    auto data = VW::parsers::flatbuffer::GetFinalExample(flatbuffer_pointer);
    switch (data->example_type())
    {
      case VW::parsers::flatbuffer::ExampleType_Example:
      {
        const auto example = data->example_as_Example();
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
      break;
      case VW::parsers::flatbuffer::ExampleType_AnotherExample:
      {
        const auto example = data->example_as_AnotherExample();
        std::cout << "another example name: " << example->name()->c_str() << " value: " << example->value()
                  << std::endl;
      }
      break;

      default:
        break;
    }
  }
}

int main(int argc, char* argv[])
{
  if (argc < 2) { throw std::invalid_argument("You need to provide a flatbuffer file to be deserialized"); }

  deserialize_and_print(argv[1]);
  return 0;
}