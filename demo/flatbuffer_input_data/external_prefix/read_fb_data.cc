#include "convert_to_fb.h"

void deserialize_only_example_collections_and_print(const std::string& from)
{
  std::ifstream data_file(from, std::ios::binary | std::ios::in);
  std::ifstream index_file(".index_file.txt");

  size_t obj_size;
  int type;
  object_type obj_type;

  while (index_file >> obj_size >> type)
  {
    obj_type = static_cast<object_type>(type);
    if (obj_type == object_type::example)
    {
      // skip
      data_file.ignore(obj_size);
      continue;
    }

    std::vector<char> fb_obj(obj_size);
    data_file.read(&fb_obj[0], obj_size);

    auto flatbuffer_pointer = reinterpret_cast<u_int8_t*>(&fb_obj[0]);

    auto data = GetExampleRoot(flatbuffer_pointer);

    const auto example_coll = data->example_obj_as_ExampleCollection();
    const auto example_size = example_coll->examples()->Length();
    for (size_t i = 0; i < example_size; i++)
    {
      const auto example = example_coll->examples()->Get(i);
      print_example(example);
    }
  }
  std::cout << "Done reading file" << std::endl;
}

void deserialize_and_print(const std::string& from)
{
  std::ifstream data_file(from, std::ios::binary | std::ios::in);
  std::ifstream index_file(".index_file.txt");

  size_t obj_size;
  int type;
  object_type obj_type;

  while (index_file >> obj_size >> type)
  {
    obj_type = static_cast<object_type>(type);

    std::vector<char> fb_obj(obj_size);
    data_file.read(&fb_obj[0], obj_size);

    auto flatbuffer_pointer = reinterpret_cast<u_int8_t*>(&fb_obj[0]);

    auto data = GetExampleRoot(flatbuffer_pointer);
    switch (obj_type)
    {
      case object_type::example:
      {
        const auto example = data->example_obj_as_Example();
        print_example(example);
        auto namespaces_size = example->namespaces()->Length();
      }
      break;
      case object_type::example_collection:
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
  std::cout << "Done reading file" << std::endl;
}

int main(int argc, char* argv[])
{
  if (argc < 3)
  {
    throw std::invalid_argument(
        "First argument \"0\" for deserializing all objects or \"1\" for deserializing only collections. Second "
        "argument the flatbuffer file to be deserialized");
  }

  if (std::strcmp(argv[1], "0") == 0) { deserialize_and_print(argv[2]); }
  else
  {
    deserialize_only_example_collections_and_print(argv[2]);
  }

  return 0;
}