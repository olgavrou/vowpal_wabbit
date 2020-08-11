// Copyright (c) by respective owners including Yahoo!, Microsoft, and
// individual contributors. All rights reserved. Released under a BSD (revised)
// license as described in the file LICENSE.

#include <fstream>
#include <iostream>
#include <cfloat>

#include "../../global_data.h"
#include "../../constant.h"
#include "../../cb.h"
#include "../../action_score.h"
#include "../../best_constant.h"
#include "parse_example_flatbuffer.h"

namespace VW
{
namespace parsers
{
namespace flatbuffer
{
int flatbuffer_to_examples(vw* all, v_array<example*>& examples)
{
  return static_cast<int>(all->flat_converter->parse_examples_wio(all, examples));
}

parser::parser(uint8_t* buffer_pointer) : _flatbuffer_pointer(buffer_pointer), _example_index(0), _c_hash(0)
{
  _data = VW::parsers::flatbuffer::GetSizePrefixedExampleRoot(_flatbuffer_pointer);  // Shift as a separate call
  _initialized = true;
}

const VW::parsers::flatbuffer::ExampleRoot* parser::data() { return _data; }

void parser::process_collection_item(vw* all, v_array<example*>& examples)
{
  const auto ex = _data->example_obj_as_ExampleCollection()->examples()->Get(_example_index);
  parse_example(all, examples[0], ex);
  // read from active collection
  _example_index++;
  if (_example_index == _data->example_obj_as_ExampleCollection()->examples()->Length())
  {
    _example_index = 0;
    _active_collection = false;
  }
}

void parser::parse(uint8_t* buffer_pointer)
{
  _flatbuffer_pointer = buffer_pointer;
  _data = VW::parsers::flatbuffer::GetSizePrefixedExampleRoot(_flatbuffer_pointer);
}

bool parser::parse_examples_wio(vw* all, v_array<example*>& examples)
{
  // for now this will read the entire file and deserialize the
  // examples collection that will then be accessed via the parse_examples method
  if (_active_collection && _example_index < _data->example_obj_as_ExampleCollection()->examples()->Length())
  {
    process_collection_item(all, examples);
    return true;
  }
  else
  {
    char* line = nullptr;
    auto len = all->p->input->buf_read(line, sizeof(uint32_t));
    if (len < sizeof(uint32_t))
    {
      return false;
    }

    _object_size = flatbuffers::ReadScalar<flatbuffers::uoffset_t>(line);
    // read one object, object size defined by the read prefix
    all->p->input->buf_read(line, _object_size);
    _flatbuffer_pointer = reinterpret_cast<uint8_t*>(line);

    _data = VW::parsers::flatbuffer::GetExampleRoot(_flatbuffer_pointer);

    switch (_data->example_obj_type())
    {
      case VW::parsers::flatbuffer::ExampleType_Example:
      {
        const auto example = _data->example_obj_as_Example();
        parse_example(all, examples[0], example);
        return true;
      }
      break;
      case VW::parsers::flatbuffer::ExampleType_ExampleCollection:
      {
        _active_collection = true;
        process_collection_item(all, examples);
        return true;
      }
      break;

      default:
        break;
    }

    return false;
  }
}

bool parser::parse_examples(vw* all, v_array<example*>& examples)
{
  // for now this will read the entire file and deserialize the
  // examples collection that will then be accessed via the parse_examples method
  if (_active_collection && _example_index < _data->example_obj_as_ExampleCollection()->examples()->Length())
  {
    process_collection_item(all, examples);
    return true;
  }
  else
  {
    // char* line = nullptr;
    // auto len = all->p->input->buf_read(line, sizeof(uint32_t));
    // if (len < sizeof(uint32_t))
    // {
    //   return false;
    // }

    // _object_size = flatbuffers::ReadScalar<flatbuffers::uoffset_t>(line);
    // // read one object, object size defined by the read prefix
    // all->p->input->buf_read(line, _object_size);
    // auto _flatbuffer_pointer = reinterpret_cast<uint8_t*>(line);

    // auto _data = VW::parsers::flatbuffer::GetExampleRoot(_flatbuffer_pointer);

    switch (_data->example_obj_type())
    {
      case VW::parsers::flatbuffer::ExampleType_Example:
      {
        const auto example = _data->example_obj_as_Example();
        parse_example(all, examples[0], example);
        return true;
      }
      break;
      case VW::parsers::flatbuffer::ExampleType_ExampleCollection:
      {
        _active_collection = true;
        process_collection_item(all, examples);
        return true;
      }
      break;

      default:
        break;
    }

    return false;
  }
}

void parser::parse_example(vw* all, example* ae, const Example* eg)
{
  all->p->lp.default_label(&ae->l);
  parse_flat_label(all->sd, ae, eg);

  if (flatbuffers::IsFieldPresent(eg, Example::VT_TAG))
  {
    ae->tag = v_init<char>();
    VW::string_view tag(eg->tag()->c_str());
    push_many(ae->tag, tag.begin(), tag.size());
  }

  for (const auto& ns : *(eg->namespaces())) { parse_namespaces(all, ae, ns); }
}

bool parser::is_initialized() const { return _initialized; }

void parser::parse_namespaces(vw* all, example* ae, const Namespace* ns)
{
  namespace_index temp_index;
  if (flatbuffers::IsFieldPresent(ns, Namespace::VT_NAME))
  {
    temp_index = (uint8_t)ns->name()->c_str()[0];
    _c_hash = all->p->hasher(ns->name()->c_str(), ns->name()->Length(), all->hash_seed);
  }
  else
  {
    temp_index = ns->hash();
  }
  ae->indices.push_back(temp_index);

  auto& fs = ae->feature_space[temp_index];

  for (const auto& feature : *(ns->features())) { parse_features(all, fs, feature); }
}

void parser::parse_features(vw* all, features& fs, const Feature* feature)
{
  if (flatbuffers::IsFieldPresent(feature, Feature::VT_NAME))
  {
    uint64_t word_hash = (all->p->hasher(reinterpret_cast<const char*>(feature->name()->Data()), feature->name()->Length(), _c_hash) & all->parse_mask);
    // uint64_t word_hash = (all->p->hasher(feature->name()->c_str(), feature->name()->Length(), _c_hash) & all->parse_mask);
    fs.push_back(feature->value(), word_hash);
  }
  else
  {
    fs.push_back(feature->value(), feature->hash());
  }
}

void parser::parse_flat_label(shared_data* sd, example* ae, const Example* eg)
{
  Label label_type = eg->label_type();

  switch (label_type)
  {
    case Label_SimpleLabel:
    {
      const SimpleLabel* simple_label = static_cast<const SimpleLabel*>(eg->label());
      parse_simple_label(sd, &(ae->l), simple_label);
      break;
    }
    case Label_CBLabel:
    {
      const CBLabel* cb_label = static_cast<const CBLabel*>(eg->label());
      parse_cb_label(&(ae->l), cb_label);
      break;
    }
    case Label_CCBLabel:
    {
      const CCBLabel* ccb_label = static_cast<const CCBLabel*>(eg->label());
      parse_ccb_label(&(ae->l), ccb_label);
      break;
    }
    case Label_CB_EVAL_Label:
    {
      auto cb_eval_label = static_cast<const CB_EVAL_Label*>(eg->label());
      parse_cb_eval_label(&(ae->l), cb_eval_label);
      break;
    }
    case Label_CS_Label:
    {
      auto cs_label = static_cast<const CS_Label*>(eg->label());
      parse_cs_label(&(ae->l), cs_label);
      break;
    }
    case Label_MultiClass:
    {
      auto mc_label = static_cast<const MultiClass*>(eg->label());
      parse_mc_label(sd, &(ae->l), mc_label);
      break;
    }
    case Label_MultiLabel:
    {
      auto multi_label = static_cast<const MultiLabel*>(eg->label());
      parse_multi_label(&(ae->l), multi_label);
      break;
    }
    case Label_Slates_Label:
    {
      auto slates_label = static_cast<const Slates_Label*>(eg->label());
      parse_slates_label(&(ae->l), slates_label);
      break;
    }
    case Label_no_label:
      // auto label = static_cast<const no_label*>(eg->label());
      parse_no_label();
      break;
    default:
      THROW("Label type in Flatbuffer not understood");
  }
}

}  // namespace flatbuffer
}  // namespace parsers
}  // namespace VW
