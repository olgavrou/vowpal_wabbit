#include "convert_to_fb.h"

void convert(const std::string& from, const std::string& to)
{
  std::ifstream data_file(from);
  std::string line;

  std::ofstream outfile;
  outfile.open(to, std::ios::binary | std::ios::out);

  std::ofstream index_file;
  index_file.open(".index_file.txt");

  flatbuffers::FlatBufferBuilder builder;

  size_t line_count = 0;
  while (std::getline(data_file, line))
  {
    line_count++;
    auto simple_example = parse_line_simple(line);
    auto root_ex_obj = create_example_obj(builder, simple_example);

    builder.Finish(root_ex_obj);
    uint8_t* buf = builder.GetBufferPointer();
    size_t size = builder.GetSize();
    outfile.write(reinterpret_cast<char*>(buf), size);
    index_file << size << " " << static_cast<int>(object_type::example) << std::endl;

    auto root_coll_obj = create_example_collection_obj(builder, simple_example);

    builder.Finish(root_coll_obj);
    buf = builder.GetBufferPointer();
    size = builder.GetSize();
    outfile.write(reinterpret_cast<char*>(buf), size);
    index_file << size << " " << static_cast<int>(object_type::example_collection) << std::endl;
  }

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