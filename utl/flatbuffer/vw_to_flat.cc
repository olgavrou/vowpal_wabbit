// Copyright (c) by respective owners including Yahoo!, Microsoft, and
// individual contributors. All rights reserved. Released under a BSD (revised)
// license as described in the file LICENSE.

#include <sys/timeb.h>
#include <fstream>
#include <vector>

#include "vw_to_flat.h"
#include "options.h"
#include "parse_args.h"
#include "parse_regressor.h"
#include "accumulate.h"
#include "best_constant.h"
#include "vw_exception.h"
#include "options_boost_po.h"

void write_buffer_to_file(std::ofstream& outfile, flatbuffers::FlatBufferBuilder& builder,
    flatbuffers::Offset<VW::parsers::flatbuffer::ExampleRoot>& root)
{
  builder.FinishSizePrefixed(root);
  uint8_t* buf = builder.GetBufferPointer();
  int size = builder.GetSize();
  outfile.write(reinterpret_cast<char*>(buf), size);
}

void to_flat::write_collection_to_file(bool is_multiline, std::ofstream& outfile)
{
  flatbuffers::Offset<VW::parsers::flatbuffer::ExampleCollection> egcollection;
  if (is_multiline)
  {
    egcollection =
        VW::parsers::flatbuffer::CreateExampleCollectionDirect(_builder, nullptr, &_multi_example_collection, true);
    _multi_example_collection.clear();
  }
  else
  {
    egcollection =
        VW::parsers::flatbuffer::CreateExampleCollectionDirect(_builder, &_example_collection, nullptr, false);
    _example_collection.clear();
  }
  auto root = CreateExampleRoot(_builder, VW::parsers::flatbuffer::ExampleType_ExampleCollection, egcollection.Union());
  write_buffer_to_file(outfile, _builder, root);
}

void to_flat::write_to_file(bool collection, bool is_multiline, MultiExampleBuilder& multi_ex_builder,
    ExampleBuilder& ex_builder, std::ofstream& outfile)
{
  if (collection)
  {
    if (is_multiline)
    {
      auto multi_ex = multi_ex_builder.to_flat_example(_builder);
      _multi_example_collection.push_back(multi_ex);
      _multi_ex_index = 0;
    }
    else
    {
      auto ex = ex_builder.to_flat_example(_builder);
      _example_collection.push_back(ex);
    }
    _collection_count++;
    if (_collection_count >= collection_size)
    {
      write_collection_to_file(is_multiline, outfile);
      _builder.Clear();
      _collection_count = 0;
    }
  }
  else
  {
    if (is_multiline)
    {
      auto multi_ex = multi_ex_builder.to_flat_example(_builder);
      _multi_ex_index = 0;

      auto root = VW::parsers::flatbuffer::CreateExampleRoot(
          _builder, VW::parsers::flatbuffer::ExampleType_MultiExample, multi_ex.Union());
      write_buffer_to_file(outfile, _builder, root);
    }
    else
    {
      auto ex = ex_builder.to_flat_example(_builder);

      auto root = VW::parsers::flatbuffer::CreateExampleRoot(
          _builder, VW::parsers::flatbuffer::ExampleType_Example, ex.Union());
      write_buffer_to_file(outfile, _builder, root);
    }
  }
}

void to_flat::create_simple_label(example* v, ExampleBuilder& ex_builder)
{
  ex_builder.label =
      VW::parsers::flatbuffer::CreateSimpleLabel(_builder, v->l.simple.label, v->l.simple.weight).Union();
  ex_builder.label_type = VW::parsers::flatbuffer::Label_SimpleLabel;
}

void to_flat::create_cb_label(example* v, ExampleBuilder& ex_builder)
{
  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::CB_class>> costs;
  for (auto const& cost : v->l.cb.costs)
  {
    costs.push_back(VW::parsers::flatbuffer::CreateCB_class(
        _builder, cost.cost, cost.action, cost.probability, cost.partial_prediction));
  }
  ex_builder.label = VW::parsers::flatbuffer::CreateCBLabelDirect(_builder, v->l.cb.weight, &costs).Union();
  ex_builder.label_type = VW::parsers::flatbuffer::Label_CBLabel;
}

void to_flat::create_ccb_label(example* v, ExampleBuilder& ex_builder)
{
  auto weight = v->l.conditional_contextual_bandit.weight;
  auto e_type = v->l.conditional_contextual_bandit.type;

  if (e_type == CCB::example_type::shared)
  {
    auto type = VW::parsers::flatbuffer::CCB_Slates_example_type_shared;
    ex_builder.label = VW::parsers::flatbuffer::CreateCCBLabelDirect(_builder, type, 0, nullptr, weight).Union();
    ex_builder.label_type = VW::parsers::flatbuffer::Label_CCBLabel;
  }
  else if (e_type == CCB::example_type::action)
  {
    auto type = VW::parsers::flatbuffer::CCB_Slates_example_type_action;
    ex_builder.label = VW::parsers::flatbuffer::CreateCCBLabelDirect(_builder, type, 0, nullptr, weight).Union();
    ex_builder.label_type = VW::parsers::flatbuffer::Label_CCBLabel;
  }
  else if (e_type == CCB::example_type::slot)
  {
    auto type = VW::parsers::flatbuffer::CCB_Slates_example_type_slot;
    std::vector<uint32_t> explicit_included_actions;
    std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::action_score>> action_scores;
    if (v->l.conditional_contextual_bandit.outcome != nullptr)
    {
      for (const auto& probability : v->l.conditional_contextual_bandit.outcome->probabilities)
      {
        auto action = probability.action;
        auto score = probability.score;
        action_scores.push_back(VW::parsers::flatbuffer::Createaction_score(_builder, action, score));
      }
      auto cost = v->l.conditional_contextual_bandit.outcome->cost;
      auto outcome = VW::parsers::flatbuffer::CreateCCB_outcomeDirect(_builder, cost, &action_scores);
      ex_builder.label = VW::parsers::flatbuffer::CreateCCBLabelDirect(_builder, type, outcome, nullptr).Union();
      ex_builder.label_type = VW::parsers::flatbuffer::Label_CCBLabel;
    }
    else if (&(v->l.conditional_contextual_bandit.explicit_included_actions) != nullptr)
    {
      for (auto const& action : v->l.conditional_contextual_bandit.explicit_included_actions)
      { explicit_included_actions.push_back(action); }
      ex_builder.label =
          VW::parsers::flatbuffer::CreateCCBLabelDirect(_builder, type, 0, &explicit_included_actions).Union();
      ex_builder.label_type = VW::parsers::flatbuffer::Label_CCBLabel;
    }
    else
    {
      ex_builder.label = VW::parsers::flatbuffer::CreateCCBLabelDirect(_builder, type, 0, nullptr, weight).Union();
      ex_builder.label_type = VW::parsers::flatbuffer::Label_CCBLabel;
    }
  }
  else
  {
    auto type = VW::parsers::flatbuffer::CCB_Slates_example_type_unset;
    ex_builder.label = VW::parsers::flatbuffer::CreateCCBLabelDirect(_builder, type, 0, nullptr, weight).Union();
    ex_builder.label_type = VW::parsers::flatbuffer::Label_CCBLabel;
  }
}

void to_flat::create_cb_eval_label(example* v, ExampleBuilder& ex_builder)
{
  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::CB_class>> costs;
  for (const auto& cost : v->l.cb_eval.event.costs)
  {
    costs.push_back(VW::parsers::flatbuffer::CreateCB_class(
        _builder, cost.cost, cost.action, cost.probability, cost.partial_prediction));
  }
  auto sub_label = CreateCBLabelDirect(_builder, v->l.cb_eval.event.weight, &costs);
  ex_builder.label = VW::parsers::flatbuffer::CreateCB_EVAL_Label(_builder, v->l.cb_eval.action, sub_label).Union();
  ex_builder.label_type = VW::parsers::flatbuffer::Label_CB_EVAL_Label;
}

void to_flat::create_mc_label(VW::named_labels* ldict, example* v, ExampleBuilder& ex_builder)
{
  if (ldict)
  {
    if (ldict->get(v->l.multi.label).empty())
      ex_builder.label = VW::parsers::flatbuffer::CreateMultiClass(_builder, 0, 0U, v->l.multi.weight).Union();
    else
    {
      VW::string_view named_label = ldict->get(v->l.multi.label);
      ex_builder.label = VW::parsers::flatbuffer::CreateMultiClass(
          _builder, _builder.CreateString(std::string(named_label.begin(), named_label.end())), 0U, v->l.multi.weight)
                             .Union();
    }
  }
  else
  {
    ex_builder.label =
        VW::parsers::flatbuffer::CreateMultiClass(_builder, 0, v->l.multi.label, v->l.multi.weight).Union();
  }

  ex_builder.label_type = VW::parsers::flatbuffer::Label_MultiClass;
}

void to_flat::create_multi_label(example* v, ExampleBuilder& ex_builder)
{
  std::vector<uint32_t> labels;
  for (auto const l : v->l.multilabels.label_v) { labels.push_back(l); }

  ex_builder.label = VW::parsers::flatbuffer::CreateMultiLabelDirect(_builder, &labels).Union();
  ex_builder.label_type = VW::parsers::flatbuffer::Label_MultiLabel;
}

void to_flat::create_slates_label(example* v, ExampleBuilder& ex_builder)
{
  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::action_score>> action_scores;
  float weight = v->l.slates.weight;
  auto e_type = v->l.slates.type;
  ex_builder.label_type = VW::parsers::flatbuffer::Label_Slates_Label;

  if (e_type == 1)
  {  // shared type
    auto type = VW::parsers::flatbuffer::CCB_Slates_example_type_shared;
    ex_builder.label = VW::parsers::flatbuffer::CreateSlates_LabelDirect(
        _builder, type, weight, v->l.slates.labeled, v->l.slates.cost, 0U, nullptr)
                           .Union();
  }
  else if (e_type == 2)
  {  // action type
    auto type = VW::parsers::flatbuffer::CCB_Slates_example_type_action;
    ex_builder.label = VW::parsers::flatbuffer::CreateSlates_LabelDirect(
        _builder, type, weight, false, 0.0, v->l.slates.slot_id, nullptr)
                           .Union();
  }
  else if (e_type == 3)
  {  // slot type
    auto type = VW::parsers::flatbuffer::CCB_Slates_example_type_slot;
    for (auto const& as : v->l.slates.probabilities)
    { action_scores.push_back(VW::parsers::flatbuffer::Createaction_score(_builder, as.action, as.score)); }
    ex_builder.label = VW::parsers::flatbuffer::CreateSlates_LabelDirect(
        _builder, type, weight, v->l.slates.labeled, 0.0, 0U, &action_scores)
                           .Union();
  }
  else
  {
    ex_builder.label = VW::parsers::flatbuffer::CreateSlates_LabelDirect(
        _builder, VW::parsers::flatbuffer::CCB_Slates_example_type_unset, weight, false, 0.0, 0U, nullptr)
                           .Union();
  }
}

void to_flat::create_cs_label(example* v, ExampleBuilder& ex_builder)
{
  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::wclass>> costs;
  for (auto const& wc : v->l.cs.costs)
  {
    costs.push_back(
        VW::parsers::flatbuffer::Createwclass(_builder, wc.x, wc.partial_prediction, wc.wap_value, wc.class_index));
  }
  ex_builder.label = VW::parsers::flatbuffer::CreateCS_LabelDirect(_builder, &costs).Union();
  ex_builder.label_type = VW::parsers::flatbuffer::Label_CS_Label;
}

void to_flat::create_no_label(example* v, ExampleBuilder& ex_builder)
{
  ex_builder.label = VW::parsers::flatbuffer::Createno_label(_builder, (uint8_t)'\000').Union();
}

void to_flat::convert_txt_to_flat(vw& all)
{
  std::ofstream outfile;
  if (output_flatbuffer_name.empty()) { output_flatbuffer_name = all.data_filename + ".fb"; }
  outfile.open(output_flatbuffer_name, std::ios::binary | std::ios::out);

  MultiExampleBuilder multi_ex_builder;
  ExampleBuilder ex_builder;

  example* ae = all.example_parser->ready_parsed_examples.pop();

  while (ae != nullptr && !ae->end_pass)
  {
    // Create Label for current example
    flatbuffers::Offset<void> label;
    VW::parsers::flatbuffer::Label label_type = VW::parsers::flatbuffer::Label_NONE;
    switch (all.label_type)
    {
      case label_type_t::nolabel:
        to_flat::create_no_label(ae, ex_builder);
        break;
      case label_type_t::cb:
        to_flat::create_cb_label(ae, ex_builder);
        break;
      case label_type_t::ccb:
        to_flat::create_ccb_label(ae, ex_builder);
        break;
      case label_type_t::multi:
        to_flat::create_multi_label(ae, ex_builder);
        break;
      case label_type_t::mc:
        to_flat::create_mc_label(all.sd->ldict, ae, ex_builder);
        break;
      case label_type_t::cs:
        to_flat::create_cs_label(ae, ex_builder);
        break;
      case label_type_t::cb_eval:
        to_flat::create_cb_eval_label(ae, ex_builder);
        break;
      case label_type_t::slates:
        to_flat::create_slates_label(ae, ex_builder);
        break;
      case label_type_t::simple:
        to_flat::create_simple_label(ae, ex_builder);
        break;
      default:
        THROW("Unknown label type");
        break;
    }

    uint64_t multiplier = (uint64_t)all.wpp << all.weights.stride_shift();
    if (multiplier != 1)
    {
      for (features& fs : *ae)
      {
        for (auto& j : fs.indicies) { j /= multiplier; }
      }
    }
    std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::Namespace>> namespaces;
    for (const namespace_index& ns : ae->indices)
    {
      // Skip over constant namespace as that will be assigned while reading flatbuffer again
      if (ns == 128) { continue; }

      std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::Feature>> fts;

      for (features::iterator& f : ae->feature_space[ns])
      { fts.push_back(VW::parsers::flatbuffer::CreateFeatureDirect(_builder, nullptr, f.value(), f.index())); }
      namespaces.push_back(VW::parsers::flatbuffer::CreateNamespaceDirect(_builder, nullptr, ns, &fts));
    }
    std::string tag(ae->tag.begin(), ae->tag.size());

    if (all.l->is_multiline)
    {
      if (!example_is_newline(*ae))
      {
        ex_builder.namespaces.insert(ex_builder.namespaces.end(), namespaces.begin(), namespaces.end());
        ex_builder.tag = _builder.CreateString(tag);
        multi_ex_builder.examples.push_back(ex_builder);
        ex_builder.clear();
        _multi_ex_index++;
        _examples++;
        ae = all.example_parser->ready_parsed_examples.pop();
        continue;
      }
    }
    else
    {
      ex_builder.namespaces.insert(ex_builder.namespaces.end(), namespaces.begin(), namespaces.end());
      ex_builder.tag = _builder.CreateString(tag);
      _examples++;
    }

    write_to_file(collection, all.l->is_multiline, multi_ex_builder, ex_builder, outfile);

    ae = all.example_parser->ready_parsed_examples.pop();
  }

  if (collection && _collection_count > 0)
  {
    // left over examples that did not fit in collection
    write_collection_to_file(all.l->is_multiline, outfile);
  }

  all.trace_message << "Converted " << _examples << " examples" << std::endl;
  all.trace_message << "Flatbuffer " << output_flatbuffer_name << " created" << std::endl;
}