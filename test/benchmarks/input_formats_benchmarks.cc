#ifndef STATIC_LINK_VW
#define BOOST_TEST_DYN_LINK
#endif

#include <benchmark/benchmark.h>

#include "parser/flatbuffer/parse_example_flatbuffer.h"
#include "parser/flatbuffer/generated/example_generated.h"

#include <vector>
#include <sstream>
#include <string>
#include <fstream>
#include <memory>
#include <array>
#include <cstdio>
#include <random>
#include <unordered_map>

#include "cache.h"
#include "parser.h"
#include "io/io_adapter.h"
#include "vw.h"
#include "../../utl/flatbuffer/vw_to_flat.h"

auto get_x_numerical_fts = [](int feature_size) {
  std::stringstream ss;
  ss << "1:1:0.5 |";
  for (size_t i = 0; i < feature_size; i++) { ss << " " << std::to_string(i) + ":4.36352"; }
  std::string s = ss.str();
  return s;
};

auto get_x_string_fts = [](int feature_size) {
  std::stringstream ss;
  ss << "1:1:0.5 | ";
  for (size_t i = 0; i < feature_size; i++) { ss << "bigfeaturename" + std::to_string(i) + ":10 "; }
  std::string s = ss.str();
  return s;
};

auto get_x_string_fts_no_label = [](int feature_size, size_t action_index = 0) {
  std::stringstream ss;
  ss << " | ";
  for (size_t j = 0; j < feature_size; j++) { ss << std::to_string(action_index) + "_" + std::to_string(j) << +" "; }
  ss << std::endl;

  return ss.str();
};

auto get_x_string_fts_multi_ex = [](int feature_size, size_t actions, bool shared, bool label, size_t start_index = 0) {
  size_t action_start = 0;
  std::stringstream ss;
  if (shared) { ss << "shared | s_1 s_2 s_3 s_4" << std::endl; }
  if (label)
  {
    ss << "0:1.0:0.5 | ";
    for (size_t j = 0; j < feature_size; j++) { ss << "0_" + std::to_string(j) << +" "; }
    ss << std::endl;
    action_start++;
  }
  for (size_t i = action_start; i < actions; i++)
  {
    ss << " | ";
    for (size_t j = start_index; j < start_index + feature_size; j++)
    { ss << std::to_string(i) + "_" + std::to_string(j) << +" "; }
    ss << std::endl;
  }
  return ss.str();
};

void parse_fts_str(flatbuffers::FlatBufferBuilder& builder, std::string feature_s,
    std::vector<VW::parsers::flatbuffer::FeatureNum>* num_fts = nullptr,
    std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::FeatureStr>>* str_fts = nullptr)
{
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
    if (num_fts) { num_fts->push_back(VW::parsers::flatbuffer::FeatureNum(std::stoi(name), std::stof(value))); }
    else if (str_fts)
    {
      auto flats = builder.CreateSharedString(name);
      str_fts->push_back(VW::parsers::flatbuffer::CreateFeatureStr(builder, flats, std::stof(value)));
    }
  }
}

flatbuffers::Offset<VW::parsers::flatbuffer::ExampleRoot> pre_hashed_sample_example_flatbuffer(
    flatbuffers::FlatBufferBuilder& builder, std::string feature_s)
{
  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::Namespace>> namespaces;
  std::vector<VW::parsers::flatbuffer::FeatureNum> fts;

  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::CB_class>> costs;
  costs.push_back(VW::parsers::flatbuffer::CreateCB_class(builder, 1, 1, 0.5, 0));
  auto label = VW::parsers::flatbuffer::CreateCBLabelDirect(builder, 1, &costs).Union();
  auto labeltype = VW::parsers::flatbuffer::Label_CBLabel;

  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::Example>> examplecollection;

  parse_fts_str(builder, feature_s, &fts);
  namespaces.push_back(VW::parsers::flatbuffer::CreateNamespaceDirect(builder, nullptr, 0, &fts, nullptr));
  auto example = VW::parsers::flatbuffer::CreateExampleDirect(builder, &namespaces, labeltype, label);
  return VW::parsers::flatbuffer::CreateExampleRoot(
      builder, VW::parsers::flatbuffer::ExampleType_Example, example.Union());
}

flatbuffers::Offset<VW::parsers::flatbuffer::Example> sample_example_flatbuffer(
    flatbuffers::FlatBufferBuilder& builder, std::string feature_s)
{
  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::Namespace>> namespaces;
  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::FeatureStr>> fts_str;

  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::CB_class>> costs;
  costs.push_back(VW::parsers::flatbuffer::CreateCB_class(builder, 1, 1, 0.5, 0));
  auto label = VW::parsers::flatbuffer::CreateCBLabelDirect(builder, 1, &costs).Union();
  auto labeltype = VW::parsers::flatbuffer::Label_CBLabel;

  parse_fts_str(builder, feature_s, nullptr, &fts_str);
  namespaces.push_back(VW::parsers::flatbuffer::CreateNamespaceDirect(builder, nullptr, 0, nullptr, &fts_str));
  return VW::parsers::flatbuffer::CreateExampleDirect(builder, &namespaces, labeltype, label);
}

std::vector<char> example_collections_flatbuffer(
    const std::string& fs, size_t examples_size, size_t collection_size = 0)
{
  bool collection = false;
  if (collection_size > 0) { collection = true; }
  std::vector<char> final_data;
  flatbuffers::FlatBufferBuilder builder;

  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::Example>> examplecollection;
  size_t collection_count = 0;

  for (size_t i = 0; i < examples_size; i++)
  {
    auto flat_example = sample_example_flatbuffer(builder, fs);
    if (collection)
    {
      examplecollection.push_back(flat_example);
      collection_count++;
      if (collection_count >= collection_size)
      {
        auto egcollection = VW::parsers::flatbuffer::CreateExampleCollectionDirect(builder, &examplecollection);
        auto root =
            CreateExampleRoot(builder, VW::parsers::flatbuffer::ExampleType_ExampleCollection, egcollection.Union());
        builder.FinishSizePrefixed(root);

        uint8_t* buf = builder.GetBufferPointer();
        auto size = builder.GetSize();
        std::copy(reinterpret_cast<char*>(buf), reinterpret_cast<char*>(buf + size), std::back_inserter(final_data));
        builder.Clear();
        examplecollection.clear();
        collection_count = 0;
      }
    }
    else
    {
      auto root = VW::parsers::flatbuffer::CreateExampleRoot(
          builder, VW::parsers::flatbuffer::ExampleType_Example, flat_example.Union());
      builder.FinishSizePrefixed(root);

      uint8_t* buf = builder.GetBufferPointer();
      auto size = builder.GetSize();
      std::copy(reinterpret_cast<char*>(buf), reinterpret_cast<char*>(buf + size), std::back_inserter(final_data));
      builder.Clear();
    }
  }
  if (examplecollection.size() > 0)
  {
    auto egcollection = VW::parsers::flatbuffer::CreateExampleCollectionDirect(builder, &examplecollection);
    auto root =
        CreateExampleRoot(builder, VW::parsers::flatbuffer::ExampleType_ExampleCollection, egcollection.Union());
    builder.FinishSizePrefixed(root);

    uint8_t* buf = builder.GetBufferPointer();
    auto size = builder.GetSize();
    std::copy(reinterpret_cast<char*>(buf), reinterpret_cast<char*>(buf + size), std::back_inserter(final_data));
    builder.Clear();
    examplecollection.clear();
    collection_count = 0;
  }
  return final_data;
}

void call_flat_converter(vw* vw, const std::string& outfile, size_t collection_size = 0)
{
  to_flat converter;
  converter.output_flatbuffer_name = outfile;
  converter.collection_size = collection_size;
  if (collection_size > 0) { converter.collection = true; }

  VW::start_parser(*vw);
  converter.convert_txt_to_flat(*vw);
  VW::end_parser(*vw);
}

std::shared_ptr<std::vector<char>> get_pre_hashed_fb_buffer(const std::string& es)
{
  std::string out_name = ".out.fb";
  auto vw = VW::initialize("--cb 2 --quiet");
  auto buffer = std::make_shared<std::vector<char>>();

  io_buf reader_view_of_buffer;
  reader_view_of_buffer.add_file(VW::io::create_buffer_view(es.data(), es.size()));
  vw->p->input = &reader_view_of_buffer;

  call_flat_converter(vw, out_name);

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

void create_input_files_in_all_formats(const std::string& filename, bool make_text, const std::string& es,
    std::string prefix, std::vector<size_t> collection_sizes)
{
  std::string txt_file = filename + ".txt";
  std::string cache_file = txt_file + ".cache";
  size_t examples = 5000;
  std::string example_string = es;
  std::remove(cache_file.c_str());
  if (make_text) { std::remove(txt_file.c_str()); }

  bool multi_ex = (prefix.find("multi") != std::string::npos);
  std::string type = "--cb 2";
  if (multi_ex) { type = "--cb_adf"; }

  if (make_text)
  {
    std::ofstream tf(txt_file);
    for (size_t i = 0; i < examples; i++) { tf << es; }
    tf.close();
  }
  // transform to cache
  auto vw = VW::initialize(type + " --quiet --cache -d " + txt_file);
  std::ifstream tf(txt_file);
  std::string line;
  while (std::getline(tf, line, '\n'))
  {
    auto ex = VW::read_example(*vw, line);
    vw->p->output->flush();
    VW::finish_example(*vw, *ex);
  }
  tf.close();
  reset_source(*vw, vw->num_bits);
  VW::finish(*vw);

  // transform to fb pre-hashed
  for (auto collection_size : collection_sizes)
  {
    std::string fb_file = filename + "_" + std::to_string(collection_size) + ".fb";
    std::remove(fb_file.c_str());
    vw = VW::initialize(txt_file + " " + type + " --quiet");
    call_flat_converter(vw, fb_file, collection_size);
  }

  if (!multi_ex)
  {
    for (auto collection_size : collection_sizes)
    {
      std::string fb_file_no_hash = filename + "_" + std::to_string(collection_size) + "_no_hash.fb";
      std::remove(fb_file_no_hash.c_str());
      // transform to fb no hash
      auto buf = example_collections_flatbuffer(example_string, examples, collection_size);
      std::ofstream outfile;
      outfile.open(fb_file_no_hash, std::ios::binary | std::ios::out);
      outfile.write(buf.data(), buf.size());
      outfile.close();
    }
  }
}

void create_input_files_in_all_formats_multiex(
    size_t feature_size, std::string prefix, size_t overlap, std::vector<size_t> collection_sizes)
{
  prefix = prefix + "multi_ex";
  std::string filename = ".temp_test_" + prefix;
  std::string txt_file = filename + ".txt";
  std::ofstream tf(txt_file);

  size_t unique_actions = 500;
  std::vector<std::string> actions;

  for (size_t i = 0; i < unique_actions; i++)
  {
    // start creating new feature strings
    actions.emplace_back(get_x_string_fts_no_label(feature_size, i));
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib200(0, 50);
  std::uniform_int_distribution<> distribRest(51, unique_actions - 1);
  size_t multi_examples = 5000;
  std::vector<std::set<int>> indexes(multi_examples);  // 10k examples, each example has 15 actions
  std::map<int, size_t> keeper;
  size_t actions_per_example = 15;

  for (size_t i = 0; i < multi_examples; i++)
  {
    size_t iteration = 0;
    while (indexes[i].size() < actions_per_example)
    {
      int index;
      if (iteration % 2)
      {
        index = distrib200(gen);
        keeper[index]++;
        indexes[i].insert(index);
      }
      else
      {
        index = distribRest(gen);
        keeper[index]++;
        indexes[i].insert(index);
      }
      iteration++;
    }
    for (int index : indexes[i]) { tf << actions[index]; }
    tf << std::endl;
  }

  // for (const auto& k : keeper) { std::cout << k.first << ":" << k.second << std::endl; }

  create_input_files_in_all_formats(filename, false, "", prefix, collection_sizes);
  tf.close();
}

class Setup
{
  Setup()
  {
    std::string s20 = "20";
    std::string s40 = "40";
    std::string s60 = "60";
    std::string s120 = "120";

    create_input_files_in_all_formats_multiex(20, s20, 5, {0, 500, 1000});
    create_input_files_in_all_formats_multiex(40, s40, 5, {0, 500, 1000});
    create_input_files_in_all_formats_multiex(60, s60, 5, {0, 500, 1000});
    create_input_files_in_all_formats_multiex(120, s120, 5, {0, 500, 1000});

    // create_input_files_in_all_formats(".temp_test_" + s20, true, get_x_string_fts(20), s20, {0, 500, 1000});
    // create_input_files_in_all_formats(".temp_test_" + s40, true, get_x_string_fts(40), s40, {0, 500, 1000});
    // create_input_files_in_all_formats(".temp_test_" + s60, true, get_x_string_fts(60), s60, {0, 500, 1000});
    // create_input_files_in_all_formats(".temp_test_" + s120, true, get_x_string_fts(120), s120, {0, 500, 1000});
  }

public:
  static void perform_setup() { static Setup setup; }
};

template <class... ExtraArgs>
static void test_flatbuffer(benchmark::State& state, ExtraArgs&&... extra_args)
{
  std::string res[sizeof...(extra_args)] = {extra_args...};
  auto prefix = res[0];
  auto collection_size = std::stoi(res[1]);
  std::string fb_file = ".temp_test_" + prefix + "_" + std::to_string(collection_size) + ".fb";
  if (sizeof...(extra_args) == 3)
  { fb_file = ".temp_test_" + prefix + "_" + std::to_string(collection_size) + "_no_hash.fb"; }
  // Setup::perform_setup();

  std::fstream filestream;
  filestream.open(fb_file);
  if (!filestream.is_open()) { throw std::logic_error("Can not open file: " + fb_file); }
  filestream.close();

  for (auto _ : state)
  {
    auto all =
        VW::initialize("--no_stdin --cb_adf --flatbuffer --quiet -d " + fb_file, nullptr, false, nullptr, nullptr);

    auto examples = v_init<example*>();
    examples.push_back(&VW::get_unused_example(all));
    while (all->p->reader(all, examples)) { VW::empty_example(*all, *examples[0]); }
    VW::finish_example(*all, *examples[0]);
    examples.delete_v();
    benchmark::ClobberMemory();
    VW::finish(*all);
  }
}

template <class... ExtraArgs>
static void test_text(benchmark::State& state, ExtraArgs&&... extra_args)
{
  std::string res[sizeof...(extra_args)] = {extra_args...};
  auto prefix = res[0];
  // Setup::perform_setup();
  std::string text_file = ".temp_test_" + prefix + ".txt";

  for (auto _ : state)
  {
    auto all = VW::initialize("--no_stdin --cb_adf --quiet -d " + text_file, nullptr, false, nullptr, nullptr);

    auto examples = v_init<example*>();
    examples.push_back(&VW::get_unused_example(all));
    while (all->p->reader(all, examples)) { VW::empty_example(*all, *examples[0]); }
    VW::finish_example(*all, *examples[0]);
    examples.delete_v();
    benchmark::ClobberMemory();
    VW::finish(*all);
  }
}

template <class... ExtraArgs>
static void test_cache(benchmark::State& state, ExtraArgs&&... extra_args)
{
  std::string res[sizeof...(extra_args)] = {extra_args...};
  auto prefix = res[0];
  // Setup::perform_setup();
  std::string cache_file = ".temp_test_" + prefix + ".txt.cache";

  for (auto _ : state)
  {
    auto all =
        VW::initialize("--no_stdin --cb_adf --quiet --cache_file " + cache_file, nullptr, false, nullptr, nullptr);

    auto examples = v_init<example*>();
    examples.push_back(&VW::get_unused_example(all));
    while (all->p->reader(all, examples)) { VW::empty_example(*all, *examples[0]); }
    VW::finish_example(*all, *examples[0]);
    examples.delete_v();
    benchmark::ClobberMemory();
    VW::finish(*all);
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

template <class... ExtraArgs>
static void bench_fb_no_hash(benchmark::State& state, ExtraArgs&&... extra_args)
{
  std::string res[sizeof...(extra_args)] = {extra_args...};
  auto example_string = res[0];

  auto vw = VW::initialize("--cb 2 --quiet");

  flatbuffers::FlatBufferBuilder builder;
  auto expl = sample_example_flatbuffer(builder, example_string);
  auto ex =
      VW::parsers::flatbuffer::CreateExampleRoot(builder, VW::parsers::flatbuffer::ExampleType_Example, expl.Union());
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
      vw->flat_converter->parse_examples(vw, examples, buf);
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
      vw->flat_converter->parse_examples(vw, examples, reinterpret_cast<uint8_t*>(buffer->data()));
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
static void bench_fb_no_hash_io_buf(benchmark::State& state, ExtraArgs&&... extra_args)
{
  std::string res[sizeof...(extra_args)] = {extra_args...};
  auto example_string = res[0];

  auto vw = VW::initialize("--cb 2 --flatbuffer --quiet");
  flatbuffers::FlatBufferBuilder builder;
  auto expl = sample_example_flatbuffer(builder, example_string);
  auto ex =
      VW::parsers::flatbuffer::CreateExampleRoot(builder, VW::parsers::flatbuffer::ExampleType_Example, expl.Union());
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
      vw->flat_converter->parse_examples(vw, examples);
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
      vw->flat_converter->parse_examples(vw, examples);
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

// BENCHMARK_CAPTURE(test_cache, 20_fts, "20");
BENCHMARK_CAPTURE(test_cache, 20_fts_multi_ex, "20multi_ex");
// BENCHMARK_CAPTURE(test_text, 20_fts, "20");
BENCHMARK_CAPTURE(test_text, 20_fts_multi_ex, "20multi_ex");
// BENCHMARK_CAPTURE(test_flatbuffer, 20_fts_no_hash, "20", "0", "nohash");
// BENCHMARK_CAPTURE(test_flatbuffer, 20_fts_no_hash_500, "20", "500", "nohash");
// BENCHMARK_CAPTURE(test_flatbuffer, 20_fts_no_hash_1000, "20", "1000", "nohash");
// BENCHMARK_CAPTURE(test_flatbuffer, 20_fts_pre_hashed, "20", "0");
// BENCHMARK_CAPTURE(test_flatbuffer, 20_fts_pre_hashed_500, "20", "500");
// BENCHMARK_CAPTURE(test_flatbuffer, 20_fts_pre_hashed_1000, "20", "1000");
BENCHMARK_CAPTURE(test_flatbuffer, 20_fts_pre_hashed_multi_ex, "20multi_ex", "0");
BENCHMARK_CAPTURE(test_flatbuffer, 20_fts_pre_hashed_multi_ex_500, "20multi_ex", "500");
BENCHMARK_CAPTURE(test_flatbuffer, 20_fts_pre_hashed_multi_ex_1000, "20multi_ex", "1000");

// BENCHMARK_CAPTURE(test_cache, 40_fts, "40");
BENCHMARK_CAPTURE(test_cache, 40_fts_multi_ex, "40multi_ex");
// BENCHMARK_CAPTURE(test_text, 40_fts, "40");
BENCHMARK_CAPTURE(test_text, 40_fts_multi_ex, "40multi_ex");
// BENCHMARK_CAPTURE(test_flatbuffer, 40_fts_no_hash, "40", "0", "nohash");
// BENCHMARK_CAPTURE(test_flatbuffer, 40_fts_no_hash_500, "40", "500", "nohash");
// BENCHMARK_CAPTURE(test_flatbuffer, 40_fts_no_hash_1000, "40", "1000", "nohash");
// BENCHMARK_CAPTURE(test_flatbuffer, 40_fts_pre_hashed, "40", "0");
// BENCHMARK_CAPTURE(test_flatbuffer, 40_fts_pre_hashed_500, "40", "500");
// BENCHMARK_CAPTURE(test_flatbuffer, 40_fts_pre_hashed_1000, "40", "1000");
BENCHMARK_CAPTURE(test_flatbuffer, 40_fts_pre_hashed_multi_ex, "40multi_ex", "0");
BENCHMARK_CAPTURE(test_flatbuffer, 40_fts_pre_hashed_multi_ex_500, "40multi_ex", "500");
BENCHMARK_CAPTURE(test_flatbuffer, 40_fts_pre_hashed_multi_ex_1000, "40multi_ex", "1000");

// BENCHMARK_CAPTURE(test_cache, 60_fts, "60");
BENCHMARK_CAPTURE(test_cache, 60_fts_multi_ex, "60multi_ex");
// BENCHMARK_CAPTURE(test_text, 60_fts, "60");
BENCHMARK_CAPTURE(test_text, 60_fts_multi_ex, "60multi_ex");
// BENCHMARK_CAPTURE(test_flatbuffer, 60_fts_no_hash, "60", "0", "nohash");
// BENCHMARK_CAPTURE(test_flatbuffer, 60_fts_no_hash_500, "60", "500", "nohash");
// BENCHMARK_CAPTURE(test_flatbuffer, 60_fts_no_hash_1000, "60", "1000", "nohash");
// BENCHMARK_CAPTURE(test_flatbuffer, 60_fts_pre_hashed, "60", "0");
// BENCHMARK_CAPTURE(test_flatbuffer, 60_fts_pre_hashed_500, "60", "500");
// BENCHMARK_CAPTURE(test_flatbuffer, 60_fts_pre_hashed_1000, "60", "1000");
BENCHMARK_CAPTURE(test_flatbuffer, 60_fts_pre_hashed_multi_ex, "60multi_ex", "0");
BENCHMARK_CAPTURE(test_flatbuffer, 60_fts_pre_hashed_multi_ex_500, "60multi_ex", "500");
BENCHMARK_CAPTURE(test_flatbuffer, 60_fts_pre_hashed_multi_ex_1000, "60multi_ex", "1000");

// BENCHMARK_CAPTURE(test_cache, 120_fts, "120");
BENCHMARK_CAPTURE(test_cache, 120_fts_multi_ex, "120multi_ex");
// BENCHMARK_CAPTURE(test_text, 120_fts, "120");
BENCHMARK_CAPTURE(test_text, 120_fts_multi_ex, "120multi_ex");
// BENCHMARK_CAPTURE(test_flatbuffer, 120_fts_no_hash, "120", "0", "nohash");
// BENCHMARK_CAPTURE(test_flatbuffer, 120_fts_no_hash_500, "120", "500", "nohash");
// BENCHMARK_CAPTURE(test_flatbuffer, 120_fts_no_hash_1000, "120", "1000", "nohash");
// BENCHMARK_CAPTURE(test_flatbuffer, 120_fts_pre_hashed, "120", "0");
// BENCHMARK_CAPTURE(test_flatbuffer, 120_fts_pre_hashed_500, "120", "500");
// BENCHMARK_CAPTURE(test_flatbuffer, 120_fts_pre_hashed_1000, "120", "1000");
BENCHMARK_CAPTURE(test_flatbuffer, 120_fts_pre_hashed_multi_ex, "120multi_ex", "0");
BENCHMARK_CAPTURE(test_flatbuffer, 120_fts_pre_hashed_multi_ex_500, "120multi_ex", "500");
BENCHMARK_CAPTURE(test_flatbuffer, 120_fts_pre_hashed_multi_ex_1000, "120multi_ex", "1000");

// BENCHMARK_CAPTURE(bench_text, 20_string_fts, get_x_string_fts(20));
// BENCHMARK_CAPTURE(bench_fb_no_hash, 20_string_fts, get_x_string_fts(20));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed, 20_string_fts, get_x_string_fts(20), "true");
// BENCHMARK_CAPTURE(bench_cache_io_buf, 20_string_fts, get_x_string_fts(20));
// BENCHMARK_CAPTURE(bench_text_io_buf, 20_string_fts, get_x_string_fts(20));
// BENCHMARK_CAPTURE(bench_fb_no_hash_io_buf, 20_string_fts, get_x_string_fts(20));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed_io_buf, 20_string_fts, get_x_string_fts(20), "true");

// BENCHMARK_CAPTURE(bench_text, 40_string_fts, get_x_string_fts(40));
// BENCHMARK_CAPTURE(bench_fb_no_hash, 40_string_fts, get_x_string_fts(40));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed, 40_string_fts, get_x_string_fts(40), "true");
// BENCHMARK_CAPTURE(bench_cache_io_buf, 40_string_fts, get_x_string_fts(40));
// BENCHMARK_CAPTURE(bench_text_io_buf, 40_string_fts, get_x_string_fts(40));
// BENCHMARK_CAPTURE(bench_fb_no_hash_io_buf, 40_string_fts, get_x_string_fts(40));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed_io_buf, 40_string_fts, get_x_string_fts(40), "true");

// BENCHMARK_CAPTURE(bench_text, 60_string_fts, get_x_string_fts(60));
// BENCHMARK_CAPTURE(bench_fb_no_hash, 60_string_fts, get_x_string_fts(60));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed, 60_string_fts, get_x_string_fts(60), "true");
// BENCHMARK_CAPTURE(bench_cache_io_buf, 60_string_fts, get_x_string_fts(60));
// BENCHMARK_CAPTURE(bench_text_io_buf, 60_string_fts, get_x_string_fts(60));
// BENCHMARK_CAPTURE(bench_fb_no_hash_io_buf, 60_string_fts, get_x_string_fts(60));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed_io_buf, 60_string_fts, get_x_string_fts(60), "true");

// BENCHMARK_CAPTURE(bench_text, 120_string_fts, get_x_string_fts(120));
// BENCHMARK_CAPTURE(bench_fb_no_hash, 120_string_fts, get_x_string_fts(120));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed, 120_string_fts, get_x_string_fts(120), "true");
// BENCHMARK_CAPTURE(bench_cache_io_buf, 120_string_fts, get_x_string_fts(120));
// BENCHMARK_CAPTURE(bench_text_io_buf, 120_string_fts, get_x_string_fts(120));
// BENCHMARK_CAPTURE(bench_fb_no_hash_io_buf, 120_string_fts, get_x_string_fts(120));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed_io_buf, 120_string_fts, get_x_string_fts(120), "true");

// BENCHMARK_CAPTURE(bench_text, 20_num_fts, get_x_numerical_fts(20));
// BENCHMARK_CAPTURE(bench_fb_no_hash, 20_num_fts, get_x_numerical_fts(20));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed, 20_num_fts, get_x_numerical_fts(20), "false");
// BENCHMARK_CAPTURE(bench_cache_io_buf, 20_num_fts, get_x_numerical_fts(20));
// BENCHMARK_CAPTURE(bench_text_io_buf, 20_num_fts, get_x_numerical_fts(20));
// BENCHMARK_CAPTURE(bench_fb_no_hash_io_buf, 20_num_fts, get_x_numerical_fts(20));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed_io_buf, 20_num_fts, get_x_numerical_fts(20), "false");

// BENCHMARK_CAPTURE(bench_text, 40_num_fts, get_x_numerical_fts(40));
// BENCHMARK_CAPTURE(bench_fb_no_hash, 40_num_fts, get_x_numerical_fts(40));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed, 40_num_fts, get_x_numerical_fts(40), "false");
// BENCHMARK_CAPTURE(bench_cache_io_buf, 40_num_fts, get_x_numerical_fts(40));
// BENCHMARK_CAPTURE(bench_text_io_buf, 40_num_fts, get_x_numerical_fts(40));
// BENCHMARK_CAPTURE(bench_fb_no_hash_io_buf, 40_num_fts, get_x_numerical_fts(40));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed_io_buf, 40_num_fts, get_x_numerical_fts(40), "false");

// BENCHMARK_CAPTURE(bench_text, 60_num_fts, get_x_numerical_fts(60));
// BENCHMARK_CAPTURE(bench_fb_no_hash, 60_num_fts, get_x_numerical_fts(60));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed, 60_num_fts, get_x_numerical_fts(60), "false");
// BENCHMARK_CAPTURE(bench_cache_io_buf, 60_num_fts, get_x_numerical_fts(60));
// BENCHMARK_CAPTURE(bench_text_io_buf, 60_num_fts, get_x_numerical_fts(60));
// BENCHMARK_CAPTURE(bench_fb_no_hash_io_buf, 60_num_fts, get_x_numerical_fts(60));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed_io_buf, 60_num_fts, get_x_numerical_fts(60), "false");

// BENCHMARK_CAPTURE(bench_text, 120_num_fts, get_x_numerical_fts(120));
// BENCHMARK_CAPTURE(bench_fb_no_hash, 120_num_fts, get_x_numerical_fts(120));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed, 120_num_fts, get_x_numerical_fts(120), "false");
// BENCHMARK_CAPTURE(bench_cache_io_buf, 120_num_fts, get_x_numerical_fts(120));
// BENCHMARK_CAPTURE(bench_text_io_buf, 120_num_fts, get_x_numerical_fts(120));
// BENCHMARK_CAPTURE(bench_fb_no_hash_io_buf, 120_num_fts, get_x_numerical_fts(120));
// BENCHMARK_CAPTURE(bench_fb_pre_hashed_io_buf, 120_num_fts, get_x_numerical_fts(120), "false");

// // BENCHMARK(cache_io);
// BENCHMARK(text_buf_reader);
// BENCHMARK(flatbuffer_buf_reader);
// Run the benchmark
BENCHMARK_MAIN();