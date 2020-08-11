#ifndef STATIC_LINK_VW
#define BOOST_TEST_DYN_LINK
#endif

#include <benchmark/benchmark.h>

#include "parser/flatbuffer/parse_example_flatbuffer.h"
#include "parser/flatbuffer/generated/example_generated.h"

// #include <boost/test/unit_test.hpp>
// #include <boost/test/test_tools.hpp>

// #include "test_common.h"

#include <vector>
#include <sstream>
#include <string>
#include <fstream>
#include <memory>
#include <array>
#include <cstdio>

#include "cache.h"
#include "parser.h"
#include "io/io_adapter.h"
#include "vw.h"
#include "../../utl/flatbuffer/vw_to_flat.h"

flatbuffers::Offset<VW::parsers::flatbuffer::ExampleRoot> pre_hashed_sample_example_flatbuffer(
    flatbuffers::FlatBufferBuilder& builder, std::string& feature_s)
{
  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::Namespace>> namespaces;
  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::Feature>> fts;

  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::CB_class>> costs;
  costs.push_back(VW::parsers::flatbuffer::CreateCB_class(builder, 1, 1, 0.5));
  auto label = VW::parsers::flatbuffer::CreateCBLabelDirect(builder, 1, &costs).Union();
  auto labeltype = VW::parsers::flatbuffer::Label_CBLabel;

  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::Example>> examplecollection;

  std::string ps = "|";
  std::string space = " ";
  size_t pipe = feature_s.find("|");
  feature_s.erase(0, pipe + ps.length() + space.length());

  size_t pos = 0;
  std::string delimiter(" ");
  while ((pos = feature_s.find(delimiter)) != std::string::npos)
  {
    auto s = feature_s.substr(0, pos);
    feature_s.erase(0, pos + delimiter.length());
    // split at :
    size_t pos_colon = s.find(":");
    auto name = s.substr(0, pos_colon);
    auto value = s.substr(pos_colon + 1);
    fts.push_back(VW::parsers::flatbuffer::CreateFeatureDirect(builder, nullptr, std::stof(value), std::stoi(name)));
  }
  namespaces.push_back(VW::parsers::flatbuffer::CreateNamespaceDirect(builder, nullptr, constant_namespace, &fts));
  auto example = VW::parsers::flatbuffer::CreateExampleDirect(builder, &namespaces, labeltype, label);
  return VW::parsers::flatbuffer::CreateExampleRoot(
      builder, VW::parsers::flatbuffer::ExampleType_Example, example.Union());
}

flatbuffers::Offset<VW::parsers::flatbuffer::ExampleRoot> sample_example_flatbuffer(
    flatbuffers::FlatBufferBuilder& builder, std::string& feature_s)
{
  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::Namespace>> namespaces;
  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::Feature>> fts;

  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::CB_class>> costs;
  costs.push_back(VW::parsers::flatbuffer::CreateCB_class(builder, 1, 1, 0.5));
  auto label = VW::parsers::flatbuffer::CreateCBLabelDirect(builder, 1, &costs).Union();
  auto labeltype = VW::parsers::flatbuffer::Label_CBLabel;

  std::string ps = "|";
  std::string space = " ";
  size_t pipe = feature_s.find("|");
  feature_s.erase(0, pipe + ps.length() + space.length());

  size_t pos = 0;
  std::string delimiter(" ");
  while ((pos = feature_s.find(delimiter)) != std::string::npos)
  {
    auto s = feature_s.substr(0, pos);
    feature_s.erase(0, pos + delimiter.length());
    // split at :
    size_t pos_colon = s.find(":");
    auto name = s.substr(0, pos_colon);
    auto value = s.substr(pos_colon + 1);

    fts.push_back(VW::parsers::flatbuffer::CreateFeatureDirect(builder, name.c_str(), std::stof(value)));
  }
  namespaces.push_back(VW::parsers::flatbuffer::CreateNamespaceDirect(builder, nullptr, constant_namespace, &fts));
  auto example = VW::parsers::flatbuffer::CreateExampleDirect(builder, &namespaces, labeltype, label);
  return VW::parsers::flatbuffer::CreateExampleRoot(
      builder, VW::parsers::flatbuffer::ExampleType_Example, example.Union());
}

void create_input_files_in_all_formats(const std::string& es, std::string& prefix, size_t collection_size = 0)
{
  std::string filename = ".temp_test_" + prefix;
  std::string txt_file = filename + ".txt";
  std::string fb_file = filename + "_" + std::to_string(collection_size) + ".fb";
  std::string cache_file = txt_file + ".cache";
  std::remove(txt_file.c_str());
  std::remove(fb_file.c_str());
  std::remove(cache_file.c_str());
  size_t examples = 10000;
  // just create the txt file
  std::ofstream tf(txt_file);
  for (size_t i = 0; i < examples; i++) { tf << es << std::endl; }
  tf.close();

  // transform to cache
  auto vw = VW::initialize("--cb 2 --quiet --cache -d " + txt_file);
  for (size_t i = 0; i < examples; i++)
  {
    auto ex = VW::read_example(*vw, es);
    vw->p->output->flush();
    VW::finish_example(*vw, *ex);
  }
  reset_source(*vw, vw->num_bits);
  VW::finish(*vw);

  // transform to fb
  vw = VW::initialize(txt_file + " --cb 2 --quiet");

  to_flat converter;
  converter.output_flatbuffer_name = fb_file;
  converter.collection_size = collection_size;
  converter.collection = true;
  VW::start_parser(*vw);
  converter.convert_txt_to_flat(*vw);
  VW::end_parser(*vw);

  std::string fb_file_no_hash = filename + "_no_hash.fb";
  flatbuffers::FlatBufferBuilder builder;
  std::string example_string = es;
  auto ex = sample_example_flatbuffer(builder, example_string);
  builder.FinishSizePrefixed(ex);

  uint8_t* buf = builder.GetBufferPointer();
  int size = builder.GetSize();
  std::ofstream outfile;
  outfile.open(fb_file_no_hash, std::ios::binary | std::ios::out);
  for (size_t i = 0; i < examples; i++) { outfile.write(reinterpret_cast<char*>(buf), size); }
}

template <class... ExtraArgs>
static void test_flatbuffer(benchmark::State& state, ExtraArgs&&... extra_args)
{
  std::string res[sizeof...(extra_args)] = {extra_args...};
  auto example_string = res[0];
  auto prefix = res[1];
  auto collection_size = std::stoi(res[2]);

  std::string fb_file = ".temp_test_" + prefix + "_" + std::to_string(collection_size) + ".fb";
  if (sizeof...(extra_args) == 4)
  {
    fb_file = ".temp_test_" + prefix + "_no_hash.fb";
  }

  // create_input_files_in_all_formats(example_string, prefix, collection_size);

  for (auto _ : state)
  {
    auto all = VW::initialize(
        "--no_stdin --cb 2 --flatbuffer --quiet -d " + fb_file, nullptr, false, nullptr, nullptr);

    auto examples = v_init<example*>();
    examples.push_back(&VW::get_unused_example(all));
    while (all->p->reader(all, examples)) { VW::empty_example(*all, *examples[0]); }
    examples.delete_v();
    benchmark::ClobberMemory();
  }
}

template <class... ExtraArgs>
static void test_text(benchmark::State& state, ExtraArgs&&... extra_args)
{
  std::string res[sizeof...(extra_args)] = {extra_args...};
  auto example_string = res[0];
  auto prefix = res[1];
  // create_input_files_in_all_formats(example_string, prefix);

  for (auto _ : state)
  {
    auto all =
        VW::initialize("--no_stdin --cb 2 --quiet -d .temp_test_" + prefix + ".txt", nullptr, false, nullptr, nullptr);

    auto examples = v_init<example*>();
    examples.push_back(&VW::get_unused_example(all));
    while (all->p->reader(all, examples)) { VW::empty_example(*all, *examples[0]); }
    examples.delete_v();
    benchmark::ClobberMemory();
  }
}

template <class... ExtraArgs>
static void test_cache(benchmark::State& state, ExtraArgs&&... extra_args)
{
  std::string res[sizeof...(extra_args)] = {extra_args...};
  auto example_string = res[0];
  auto prefix = res[1];
  // create_input_files_in_all_formats(example_string, prefix);

  for (auto _ : state)
  {
    auto all = VW::initialize(
        "--no_stdin --cb 2 --quiet --cache_file .temp_test_" + prefix + ".txt.cache", nullptr, false, nullptr, nullptr);

    auto examples = v_init<example*>();
    examples.push_back(&VW::get_unused_example(all));
    while (all->p->reader(all, examples)) { VW::empty_example(*all, *examples[0]); }
    examples.delete_v();
    benchmark::ClobberMemory();
  }
}

template <class... ExtraArgs>
static void bench_text(benchmark::State& state, ExtraArgs&&... extra_args)
{
  std::string res[sizeof...(extra_args)] = {extra_args...};
  auto example_string = res[0];

  auto es = const_cast<char*>(example_string.c_str());
  auto vw = VW::initialize("--cb 2 --quiet");
  auto examples = v_init<example*>();
  examples.push_back(&VW::get_unused_example(vw));
  for (auto _ : state)
  {
    for (size_t i = 0; i < 20000; i++)
    {
      VW::read_line(*vw, examples[0], es);
      VW::empty_example(*vw, *examples[0]);
      benchmark::ClobberMemory();
    }
  }
  examples.delete_v();
}

std::shared_ptr<std::vector<char>> get_pre_hashed_fb_buffer(const std::string& es)
{
  std::string out_name = ".out.fb";
  auto vw = VW::initialize("--cb 2 --quiet");
  auto buffer = std::make_shared<std::vector<char>>();

  io_buf reader_view_of_buffer;
  reader_view_of_buffer.add_file(VW::io::create_buffer_view(es.data(), es.size()));
  vw->p->input = &reader_view_of_buffer;

  to_flat converter;
  converter.output_flatbuffer_name = out_name;
  VW::start_parser(*vw);
  converter.convert_txt_to_flat(*vw);
  VW::end_parser(*vw);

  std::ifstream infile;
  infile.open(out_name, std::ios::binary | std::ios::in);
  infile.seekg(0, std::ios::end);
  int length = infile.tellg();
  infile.seekg(0, std::ios::beg);
  buffer->resize(length);
  infile.read(buffer->data(), length);
  infile.close();

  return buffer;
}

template <class... ExtraArgs>
static void bench_fb(benchmark::State& state, ExtraArgs&&... extra_args)
{
  std::string res[sizeof...(extra_args)] = {extra_args...};
  auto example_string = res[0];

  auto vw = VW::initialize("--cb 2 --quiet");

  flatbuffers::FlatBufferBuilder builder;
  auto ex = sample_example_flatbuffer(builder, example_string);
  builder.FinishSizePrefixed(ex);

  uint8_t* buf = builder.GetBufferPointer();
  int size = builder.GetSize();
  std::vector<char> buffer(buf, buf + size);

  auto examples = v_init<example*>();
  vw->flat_converter = std::unique_ptr<VW::parsers::flatbuffer::parser>(new VW::parsers::flatbuffer::parser);

  examples.push_back(&VW::get_unused_example(vw));

  for (auto _ : state)
  {
    for (size_t i = 0; i < 20000; i++)
    {
      vw->flat_converter->parse(buf);
      vw->flat_converter->parse_examples(vw, examples);
      VW::empty_example(*vw, *examples[0]);
      benchmark::ClobberMemory();
    }
  }
  examples.delete_v();
}

template <class... ExtraArgs>
static void bench_fb_pre_hashed(benchmark::State& state, ExtraArgs&&... extra_args)
{
  std::string res[sizeof...(extra_args)] = {extra_args...};
  auto example_string = res[0];
  bool string_fts = false;
  if (res[1] == "true") { string_fts = true; }

  auto vw = VW::initialize("--cb 2 --quiet");
  std::shared_ptr<std::vector<char>> buffer;
  if (string_fts) { buffer = get_pre_hashed_fb_buffer(example_string); }
  else
  {
    flatbuffers::FlatBufferBuilder builder;
    auto ex = pre_hashed_sample_example_flatbuffer(builder, example_string);
    builder.FinishSizePrefixed(ex);

    uint8_t* buf = builder.GetBufferPointer();
    int size = builder.GetSize();
    buffer = std::make_shared<std::vector<char>>(reinterpret_cast<char*>(buf), reinterpret_cast<char*>(buf + size));
  }

  auto examples = v_init<example*>();
  vw->flat_converter = std::unique_ptr<VW::parsers::flatbuffer::parser>(new VW::parsers::flatbuffer::parser);

  examples.push_back(&VW::get_unused_example(vw));

  for (auto _ : state)
  {
    for (size_t i = 0; i < 20000; i++)
    {
      vw->flat_converter->parse(reinterpret_cast<uint8_t*>(buffer->data()));
      vw->flat_converter->parse_examples(vw, examples);
      VW::empty_example(*vw, *examples[0]);
      benchmark::ClobberMemory();
    }
  }
  examples.delete_v();
}

std::shared_ptr<std::vector<char>> get_cache_buffer(const std::string& es)
{
  auto vw = VW::initialize("--cb 2 --quiet");
  auto buffer = std::make_shared<std::vector<char>>();
  io_buf writer_view_of_buffer;
  writer_view_of_buffer.add_file(VW::io::create_vector_writer(buffer));
  vw->p->output = &writer_view_of_buffer;
  vw->p->write_cache = true;
  auto ae = &VW::get_unused_example(vw);

  VW::read_line(*vw, ae, const_cast<char*>(es.c_str()));

  if (vw->p->write_cache)
  {
    vw->p->lp.cache_label(&ae->l, *(vw->p->output));
    cache_features(*(vw->p->output), ae, vw->parse_mask);
  }
  vw->p->output->flush();
  VW::finish_example(*vw, *ae);

  return buffer;
}

template <class... ExtraArgs>
static void bench_cache_io_buf(benchmark::State& state, ExtraArgs&&... extra_args)
{
  std::string res[sizeof...(extra_args)] = {extra_args...};
  auto example_string = res[0];

  auto buffer = get_cache_buffer(example_string);
  auto vw = VW::initialize("--cb 2 --quiet");

  auto examples = v_init<example*>();
  examples.push_back(&VW::get_unused_example(vw));

  io_buf reader_view_of_buffer;
  vw->p->input = &reader_view_of_buffer;

  for (auto _ : state)
  {
    for (size_t i = 0; i < 20000; i++)
    {
      reader_view_of_buffer.add_file(VW::io::create_buffer_view(buffer->data(), buffer->size()));
      read_cached_features(vw, examples);
      VW::empty_example(*vw, *examples[0]);
      benchmark::ClobberMemory();
    }
  }
  examples.delete_v();
}

template <class... ExtraArgs>
static void bench_cache_io_buf_collections(benchmark::State& state, ExtraArgs&&... extra_args)
{
  std::string res[sizeof...(extra_args)] = {extra_args...};
  auto example_string = res[0];
  auto examples_size = std::stoi(res[1]);

  auto buffer = get_cache_buffer(example_string);
  auto vw = VW::initialize("--cb 2 --quiet");
  io_buf reader_view_of_buffer;
  vw->p->input = &reader_view_of_buffer;

  auto examples = v_init<example*>();
  examples.push_back(&VW::get_unused_example(vw));

  for (auto _ : state)
  {
    for (size_t i = 0; i < examples_size; i++)
    { reader_view_of_buffer.add_file(VW::io::create_buffer_view(buffer->data(), buffer->size())); }
    while (read_cached_features(vw, examples)) { VW::empty_example(*vw, *examples[0]); }
    benchmark::ClobberMemory();
  }
  examples.delete_v();
}

template <class... ExtraArgs>
static void bench_fb_io_buf(benchmark::State& state, ExtraArgs&&... extra_args)
{
  std::string res[sizeof...(extra_args)] = {extra_args...};
  auto example_string = res[0];

  auto vw = VW::initialize("--cb 2 --flatbuffer --quiet");
  flatbuffers::FlatBufferBuilder builder;
  auto ex = sample_example_flatbuffer(builder, example_string);
  builder.FinishSizePrefixed(ex);

  uint8_t* buf = builder.GetBufferPointer();
  int size = builder.GetSize();
  std::vector<char> buffer(buf, buf + size);

  io_buf reader_view_of_buffer;
  vw->p->input = &reader_view_of_buffer;

  auto examples = v_init<example*>();
  examples.push_back(&VW::get_unused_example(vw));

  for (auto _ : state)
  {
    for (size_t i = 0; i < 20000; i++)
    {
      reader_view_of_buffer.add_file(VW::io::create_buffer_view(buffer.data(), buffer.size()));
      vw->flat_converter->parse_examples_wio(vw, examples);
      VW::empty_example(*vw, *examples[0]);
      benchmark::ClobberMemory();
    }
  }
  examples.delete_v();
}

template <class... ExtraArgs>
static void bench_fb_pre_hashed_io_buf(benchmark::State& state, ExtraArgs&&... extra_args)
{
  std::string res[sizeof...(extra_args)] = {extra_args...};
  auto example_string = res[0];
  bool string_fts = false;
  if (res[1] == "true") { string_fts = true; }

  auto vw = VW::initialize("--cb 2 --flatbuffer --quiet");
  std::shared_ptr<std::vector<char>> buffer;
  if (string_fts) { buffer = get_pre_hashed_fb_buffer(example_string); }
  else
  {
    flatbuffers::FlatBufferBuilder builder;
    auto ex = pre_hashed_sample_example_flatbuffer(builder, example_string);
    builder.FinishSizePrefixed(ex);

    uint8_t* buf = builder.GetBufferPointer();
    int size = builder.GetSize();
    buffer = std::make_shared<std::vector<char>>(reinterpret_cast<char*>(buf), reinterpret_cast<char*>(buf + size));
  }

  io_buf reader_view_of_buffer;
  vw->p->input = &reader_view_of_buffer;

  auto examples = v_init<example*>();
  examples.push_back(&VW::get_unused_example(vw));

  for (auto _ : state)
  {
    for (size_t i = 0; i < 20000; i++)
    {
      reader_view_of_buffer.add_file(VW::io::create_buffer_view(buffer->data(), buffer->size()));
      vw->flat_converter->parse_examples_wio(vw, examples);
      VW::empty_example(*vw, *examples[0]);
      benchmark::ClobberMemory();
    }
  }
  examples.delete_v();
}

template <class... ExtraArgs>
static void bench_text_io_buf(benchmark::State& state, ExtraArgs&&... extra_args)
{
  std::string res[sizeof...(extra_args)] = {extra_args...};
  auto example_string = res[0];

  auto vw = VW::initialize("--cb 2 --quiet");
  auto examples = v_init<example*>();
  examples.push_back(&VW::get_unused_example(vw));

  io_buf reader_view_of_buffer;
  vw->p->input = &reader_view_of_buffer;

  for (auto _ : state)
  {
    for (size_t i = 0; i < 20000; i++)
    {
      reader_view_of_buffer.add_file(VW::io::create_buffer_view(example_string.data(), example_string.size()));
      vw->p->reader(vw, examples);
      VW::empty_example(*vw, *examples[0]);
      benchmark::ClobberMemory();
    }
  }
  examples.delete_v();
}

// need to benchmark reader only

static void text_buf_reader(benchmark::State& state)
{
  std::string es(
      "1:1:0.5 | 3:4.36352 4:4.36352 5:4.36352 6:4.36352 7:4.36352 8:4.36352 9:4.36352 10:4.36352 11:4.36352 "
      "12:4.36352 13:4.36352 14:4.36352 15:4.36352 16:4.36352 17:4.36352 18:4.36352 19:4.36352 20:4.36352 21:4.36352 "
      "22:4.36352 23:4.36352 24:4.36352 25:4.36352 26:4.36352 27:4.36352 28:4.36352 29:4.36352 30:4.36352 31:4.36352 "
      "32:4.36352 33:4.36352 34:4.36352 35:4.36352 36:4.36352 37:4.36352 38:4.36352 39:4.36352 40:4.36352 41:4.36352 "
      "42:4.36352 43:4.36352 44:4.36352 45:4.36352 46:4.36352 47:4.36352 48:4.36352 49:4.36352 50:4.36352 "
      "51:4.36352");
  io_buf reader_view_of_buffer;
  for (auto _ : state)
  {
    for (size_t i = 0; i < 20000; i++)
    {
      reader_view_of_buffer.add_file(VW::io::create_buffer_view(es.data(), es.size()));
      char* line = nullptr;
      size_t num_chars_initial = reader_view_of_buffer.readto(line, '\n');
      if (num_chars_initial < 1) std::cout << "ERROR ERROR" << std::endl;
      benchmark::ClobberMemory();
    }
  }
}

static void flatbuffer_buf_reader(benchmark::State& state)
{
  flatbuffers::FlatBufferBuilder builder;
  std::string es(
      "1:1:0.5 | 3:4.36352 4:4.36352 5:4.36352 6:4.36352 7:4.36352 8:4.36352 9:4.36352 10:4.36352 11:4.36352 "
      "12:4.36352 13:4.36352 14:4.36352 15:4.36352 16:4.36352 17:4.36352 18:4.36352 19:4.36352 20:4.36352 21:4.36352 "
      "22:4.36352 23:4.36352 24:4.36352 25:4.36352 26:4.36352 27:4.36352 28:4.36352 29:4.36352 30:4.36352 31:4.36352 "
      "32:4.36352 33:4.36352 34:4.36352 35:4.36352 36:4.36352 37:4.36352 38:4.36352 39:4.36352 40:4.36352 41:4.36352 "
      "42:4.36352 43:4.36352 44:4.36352 45:4.36352 46:4.36352 47:4.36352 48:4.36352 49:4.36352 50:4.36352 "
      "51:4.36352");
  auto ex = pre_hashed_sample_example_flatbuffer(builder, es);
  builder.FinishSizePrefixed(ex);

  uint8_t* buf = builder.GetBufferPointer();
  int size = builder.GetSize();
  std::vector<char> buffer(buf, buf + size);

  io_buf reader_view_of_buffer;
  for (auto _ : state)
  {
    for (size_t i = 0; i < 20000; i++)
    {
      reader_view_of_buffer.add_file(VW::io::create_buffer_view(buffer.data(), buffer.size()));
      char* line = nullptr;
      auto len = reader_view_of_buffer.buf_read(line, sizeof(uint32_t));
      if (len < sizeof(uint32_t)) { std::cout << "ERROR ERROR" << std::endl; }

      auto _object_size = flatbuffers::ReadScalar<flatbuffers::uoffset_t>(line);
      // read one object, object size defined by the read prefix
      reader_view_of_buffer.buf_read(line, _object_size);
      benchmark::ClobberMemory();
    }
  }
}

static void cache_io(benchmark::State& state)
{
  std::string es(
      "1:1:0.5 | 3:4.36352 4:4.36352 5:4.36352 6:4.36352 7:4.36352 8:4.36352 9:4.36352 10:4.36352 11:4.36352 "
      "12:4.36352 13:4.36352 14:4.36352 15:4.36352 16:4.36352 17:4.36352 18:4.36352 19:4.36352 20:4.36352 21:4.36352 "
      "22:4.36352 23:4.36352 24:4.36352 25:4.36352 26:4.36352 27:4.36352 28:4.36352 29:4.36352 30:4.36352 31:4.36352 "
      "32:4.36352 33:4.36352 34:4.36352 35:4.36352 36:4.36352 37:4.36352 38:4.36352 39:4.36352 40:4.36352 41:4.36352 "
      "42:4.36352 43:4.36352 44:4.36352 45:4.36352 46:4.36352 47:4.36352 48:4.36352 49:4.36352 50:4.36352 "
      "51:4.36352");

  auto buffer = get_cache_buffer(es);
  auto vw = VW::initialize("--cb 2 --quiet");

  auto examples = v_init<example*>();
  examples.push_back(&VW::get_unused_example(vw));
  io_buf reader_view_of_buffer;
  vw->p->input = &reader_view_of_buffer;

  for (auto _ : state)
  {
    for (size_t i = 0; i < 20000; i++)
    {
      reader_view_of_buffer.add_file(VW::io::create_buffer_view(buffer->data(), buffer->size()));
      read_cached_features(vw, examples);
      VW::empty_example(*vw, *examples[0]);
      benchmark::ClobberMemory();
    }
  }
}

auto get_x_numerical_fts = [](int feature_size) {
  std::stringstream ss;
  ss << "1:1:0.5 |";
  for (size_t i = 0; i < feature_size; i++) { ss << " " << std::to_string(i) + ":4.36352"; }
  std::string s = ss.str();
  return s;
};

auto get_x_string_fts = [](int feature_size) {
  std::stringstream ss;
  ss << "1:1:0.5 |";
  for (size_t i = 0; i < feature_size; i++) { ss << " bigfeaturename" + std::to_string(i) + ":10"; }
  std::string s = ss.str();
  return s;
};

// BENCHMARK_CAPTURE(test_cache, 20_string_fts, get_x_string_fts(20), "20");
// BENCHMARK_CAPTURE(test_text, 20_string_fts, get_x_string_fts(20), "20");
// BENCHMARK_CAPTURE(test_flatbuffer, 20_string_fts, get_x_string_fts(20), "20", "0");
// BENCHMARK_CAPTURE(test_flatbuffer, 20_string_fts_500, get_x_string_fts(20), "20", "500");
// BENCHMARK_CAPTURE(test_flatbuffer, 20_string_fts_1000, get_x_string_fts(20), "20", "1000");
// BENCHMARK_CAPTURE(test_flatbuffer, 20_string_fts_2000, get_x_string_fts(20), "20", "2000");

// BENCHMARK_CAPTURE(test_cache, 40_string_fts, get_x_string_fts(40), "40");
// BENCHMARK_CAPTURE(test_text, 40_string_fts, get_x_string_fts(40), "40");
// BENCHMARK_CAPTURE(test_flatbuffer, 40_string_fts, get_x_string_fts(40), "40", "0");
// BENCHMARK_CAPTURE(test_flatbuffer, 40_string_fts_500, get_x_string_fts(40), "40", "500");
// BENCHMARK_CAPTURE(test_flatbuffer, 40_string_fts_1000, get_x_string_fts(40), "40", "1000");
// BENCHMARK_CAPTURE(test_flatbuffer, 40_string_fts_2000, get_x_string_fts(40), "40", "2000");

BENCHMARK_CAPTURE(test_cache, 60_string_fts, get_x_string_fts(60), "60");
BENCHMARK_CAPTURE(test_text, 60_string_fts, get_x_string_fts(60), "60");
BENCHMARK_CAPTURE(test_flatbuffer, 60_string_fts_no_hash, get_x_string_fts(60), "60", "0", "nohash");
BENCHMARK_CAPTURE(test_flatbuffer, 60_string_fts, get_x_string_fts(60), "60", "0");
BENCHMARK_CAPTURE(test_flatbuffer, 60_string_fts_500, get_x_string_fts(60), "60", "500");
BENCHMARK_CAPTURE(test_flatbuffer, 60_string_fts_1000, get_x_string_fts(60), "60", "1000");
BENCHMARK_CAPTURE(test_flatbuffer, 60_string_fts_2000, get_x_string_fts(60), "60", "2000");

BENCHMARK_CAPTURE(test_cache, 120_string_fts, get_x_string_fts(120), "120");
BENCHMARK_CAPTURE(test_text, 120_string_fts, get_x_string_fts(120), "120");
BENCHMARK_CAPTURE(test_flatbuffer, 120_string_fts_no_hash, get_x_string_fts(120), "120", "0", "nohash");
BENCHMARK_CAPTURE(test_flatbuffer, 120_string_fts, get_x_string_fts(120), "120", "0");
BENCHMARK_CAPTURE(test_flatbuffer, 120_string_fts_500, get_x_string_fts(120), "120", "500");
BENCHMARK_CAPTURE(test_flatbuffer, 120_string_fts_1000, get_x_string_fts(120), "120", "1000");
BENCHMARK_CAPTURE(test_flatbuffer, 120_string_fts_2000, get_x_string_fts(120), "120", "2000");

// BENCHMARK_CAPTURE(bench_text, 20_string_fts, get_x_string_fts(20));
// BENCHMARK_CAPTURE(bench_fb, 20_string_fts, get_x_string_fts(20));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed, 20_string_fts, get_x_string_fts(20), "true");
// BENCHMARK_CAPTURE(bench_cache_io_buf, 20_string_fts, get_x_string_fts(20));
// BENCHMARK_CAPTURE(bench_text_io_buf, 20_string_fts, get_x_string_fts(20));
// BENCHMARK_CAPTURE(bench_fb_io_buf, 20_string_fts, get_x_string_fts(20));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed_io_buf, 20_string_fts, get_x_string_fts(20), "true");

// BENCHMARK_CAPTURE(bench_text, 40_string_fts, get_x_string_fts(40));
// BENCHMARK_CAPTURE(bench_fb, 40_string_fts, get_x_string_fts(40));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed, 40_string_fts, get_x_string_fts(40), "true");
// BENCHMARK_CAPTURE(bench_cache_io_buf, 40_string_fts, get_x_string_fts(40));
// BENCHMARK_CAPTURE(bench_text_io_buf, 40_string_fts, get_x_string_fts(40));
// BENCHMARK_CAPTURE(bench_fb_io_buf, 40_string_fts, get_x_string_fts(40));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed_io_buf, 40_string_fts, get_x_string_fts(40), "true");

// BENCHMARK_CAPTURE(bench_text, 60_string_fts, get_x_string_fts(60));
// BENCHMARK_CAPTURE(bench_fb, 60_string_fts, get_x_string_fts(60));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed, 60_string_fts, get_x_string_fts(60), "true");
// BENCHMARK_CAPTURE(bench_cache_io_buf, 60_string_fts, get_x_string_fts(60));
// BENCHMARK_CAPTURE(bench_text_io_buf, 60_string_fts, get_x_string_fts(60));
// BENCHMARK_CAPTURE(bench_fb_io_buf, 60_string_fts, get_x_string_fts(60));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed_io_buf, 60_string_fts, get_x_string_fts(60), "true");

// BENCHMARK_CAPTURE(bench_text, 120_string_fts, get_x_string_fts(120));
// BENCHMARK_CAPTURE(bench_fb, 120_string_fts, get_x_string_fts(120));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed, 120_string_fts, get_x_string_fts(120), "true");
// BENCHMARK_CAPTURE(bench_cache_io_buf, 120_string_fts, get_x_string_fts(120));
// BENCHMARK_CAPTURE(bench_text_io_buf, 120_string_fts, get_x_string_fts(120));
// BENCHMARK_CAPTURE(bench_fb_io_buf, 120_string_fts, get_x_string_fts(120));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed_io_buf, 120_string_fts, get_x_string_fts(120), "true");

// BENCHMARK_CAPTURE(bench_text, 20fts, get_x_numerical_fts(20));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed, 20fts, get_x_numerical_fts(20), false);
// BENCHMARK_CAPTURE(bench_cache_io_buf, 20fts, get_x_numerical_fts(20));
// BENCHMARK_CAPTURE(bench_text_io_buf, 20fts, get_x_numerical_fts(20));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed_io_buf, 20fts, get_x_numerical_fts(20));

// BENCHMARK_CAPTURE(bench_text, 40fts, get_x_numerical_fts(40));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed, 40fts, get_x_numerical_fts(40), false);
// BENCHMARK_CAPTURE(bench_cache_io_buf, 40fts, get_x_numerical_fts(40));
// BENCHMARK_CAPTURE(bench_text_io_buf, 40fts, get_x_numerical_fts(40));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed_io_buf, 40fts, get_x_numerical_fts(40));

// BENCHMARK_CAPTURE(bench_text, 60fts, get_x_numerical_fts(60));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed, 60fts, get_x_numerical_fts(60), false);
// BENCHMARK_CAPTURE(bench_cache_io_buf, 60fts, get_x_numerical_fts(60));
// BENCHMARK_CAPTURE(bench_text_io_buf, 60fts, get_x_numerical_fts(60));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed_io_buf, 60fts, get_x_numerical_fts(60));

// BENCHMARK_CAPTURE(bench_text, 120fts, get_x_numerical_fts(120));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed, 120fts, get_x_numerical_fts(120), false);
// BENCHMARK_CAPTURE(bench_cache_io_buf, 120fts, get_x_numerical_fts(120));
// BENCHMARK_CAPTURE(bench_text_io_buf, 120fts, get_x_numerical_fts(120));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed_io_buf, 120fts, get_x_numerical_fts(120));

// BENCHMARK(bench_text);
// BENCHMARK(bench_cache_io_buf);
// BENCHMARK(bench_text_io_buf);
// BENCHMARK(bench_fb_pre_hashed_io_buf);
// // BENCHMARK(cache_io);
// BENCHMARK(text_buf_reader);
// BENCHMARK(flatbuffer_buf_reader);
// Run the benchmark
BENCHMARK_MAIN();

// for (auto& f : examples[0]->feature_space)
// {
//   for (size_t i = 0; i < f.indicies.size(); i++) { std::cout << f.indicies[i] << ":" << f.values[i] << std::endl; }
// }

// std::string example_string(
//     "1:1:0.5 | 3:4.36352 4:4.36352 5:4.36352 6:4.36352 7:4.36352 8:4.36352 9:4.36352 10:4.36352 11:4.36352 "
//     "12:4.36352 13:4.36352 14:4.36352 15:4.36352 16:4.36352 17:4.36352 18:4.36352 19:4.36352 20:4.36352 21:4.36352
//     " "22:4.36352 23:4.36352 24:4.36352 25:4.36352 26:4.36352 27:4.36352 28:4.36352 29:4.36352 30:4.36352
//     31:4.36352 " "32:4.36352 33:4.36352 34:4.36352 35:4.36352 36:4.36352 37:4.36352 38:4.36352 39:4.36352
//     40:4.36352 41:4.36352 " "42:4.36352 43:4.36352 44:4.36352 45:4.36352 46:4.36352 47:4.36352 48:4.36352
//     49:4.36352 50:4.36352 51:4.36352");

// BENCHMARK_CAPTURE(bench_fb_pre_hashed, bench_fb_pre_hashed, example_string);