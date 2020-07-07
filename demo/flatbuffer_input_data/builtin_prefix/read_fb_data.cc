#include "convert_to_fb.h"

void deserialize_and_print(const std::string& from)
{
  std::ifstream data_file(from, std::ios::binary | std::ios::in);
  size_t prefix_len = sizeof(uint32_t);
  uint32_t prefix;

  while (!data_file.fail())
  {
    // read the prefix from the file
    data_file.read((char*)&prefix, prefix_len);
    if (data_file.fail())
    {
      std::cout << "Done reading file" << std::endl;
      break;
    }

    std::vector<char> fb_obj(prefix);
    // read one object, object size defined by the read prefix
    data_file.read(&fb_obj[0], prefix);

    auto flatbuffer_pointer = reinterpret_cast<u_int8_t*>(&fb_obj[0]);

    auto data = GetExampleRoot(flatbuffer_pointer);
    switch (data->example_obj_type())
    {
      case ExampleType_Example:
      {
        const auto example = data->example_obj_as_Example();
        print_example(example);
      }
      break;
      case ExampleType_ExampleCollection:
      {
        const auto example_coll = data->example_obj_as_ExampleCollection();
        const auto example_size = example_coll->examples()->Length();
        for (size_t i = 0; i < example_size; i++)
        {
          const auto example = example_coll->examples()->Get(i);
          print_example(example);
        }
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