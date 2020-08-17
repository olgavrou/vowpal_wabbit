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
#include "example.h"
#include "cb.h"

std::string to_flat::get_label_string(label_type_t label_type)
{
  switch (label_type)
  {
    case label_type_t::nolabel:
      return std::string("No label");
    case label_type_t::cb:
      return std::string("Contextual Bandit Label");
    case label_type_t::ccb:
      return std::string("Conditional Contextual Bandit Label");
    case label_type_t::multi:
      return std::string("Multilabel Label");
    case label_type_t::mc:
      return std::string("Multiclass Label");
    case label_type_t::cs:
      return std::string("Cost Sensitive Label");
    case label_type_t::cb_eval:
      return std::string("Contextual Bandit Eval Label");
    case label_type_t::slates:
      return std::string("Slates Label");
    case label_type_t::simple:
      return std::string("Simple Label");
    default:
      THROW("Unknown label type");
      break;
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
        _builder, cost.action, cost.cost, cost.probability, cost.partial_prediction));
  }
  ex_builder.label = VW::parsers::flatbuffer::CreateCBLabelDirect(_builder, v->l.cb.weight, &costs).Union();
  ex_builder.label_type = VW::parsers::flatbuffer::Label_CBLabel;
}

void to_flat::create_cb_label_multi_ex(example* v, MultiExampleBuilder& ex_builder, uint32_t multi_ex_index)
{
  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::CB_class>> costs;
  for (auto const& cost : v->l.cb.costs)
  {
    auto action = cost.action;
    if (!CB::ec_is_example_header(*v))
    {
      // this example has a label, need to not which example it is
      action = multi_ex_index;
    }
    costs.push_back(VW::parsers::flatbuffer::CreateCB_class(
        _builder, action, cost.cost, cost.probability, cost.partial_prediction));
  }
  if (CB::ec_is_example_header(*v))
  {
    ex_builder.shared = VW::parsers::flatbuffer::CreateCBLabelDirect(_builder, v->l.cb.weight, &costs).Union();
    ex_builder.shared_type = VW::parsers::flatbuffer::Label_CBLabel;
  }
  else if (v->l.cb.costs.size())
  {
    ex_builder.label = VW::parsers::flatbuffer::CreateCBLabelDirect(_builder, v->l.cb.weight, &costs).Union();
    ex_builder.label_type = VW::parsers::flatbuffer::Label_CBLabel;
  }
}

void to_flat::create_ccb_label(example* v, ExampleBuilder& ex_builder)
{
  auto weight = v->l.conditional_contextual_bandit.weight;
  auto e_type = v->l.conditional_contextual_bandit.type;
  ex_builder.label_type = VW::parsers::flatbuffer::Label_CCBLabel;

  if (e_type == 1)
  {
    auto type = VW::parsers::flatbuffer::CCB_Slates_example_type_shared;
    ex_builder.label = VW::parsers::flatbuffer::CreateCCBLabelDirect(_builder, type, 0, nullptr, weight).Union();
  }
  else if (e_type == 2)
  {
    auto type = VW::parsers::flatbuffer::CCB_Slates_example_type_action;
    ex_builder.label = VW::parsers::flatbuffer::CreateCCBLabelDirect(_builder, type, 0, nullptr, weight).Union();
  }
  else if (e_type == 3)
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
    }
    else if (&(v->l.conditional_contextual_bandit.explicit_included_actions) != nullptr)
    {
      for (auto const& action : v->l.conditional_contextual_bandit.explicit_included_actions)
      { explicit_included_actions.push_back(action); }
      ex_builder.label =
          VW::parsers::flatbuffer::CreateCCBLabelDirect(_builder, type, 0, &explicit_included_actions).Union();
    }
    else
    {
      ex_builder.label = VW::parsers::flatbuffer::CreateCCBLabelDirect(_builder, type, 0, nullptr, weight).Union();
    }
  }
  else
  {
    auto type = VW::parsers::flatbuffer::CCB_Slates_example_type_unset;
    ex_builder.label = VW::parsers::flatbuffer::CreateCCBLabelDirect(_builder, type, 0, nullptr, weight).Union();
  }
}

void to_flat::create_cb_eval_label(example* v, ExampleBuilder& ex_builder)
{
  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::CB_class>> costs;
  for (const auto& cost : v->l.cb_eval.event.costs)
  {
    costs.push_back(VW::parsers::flatbuffer::CreateCB_class(
        _builder, cost.action, cost.cost, cost.probability, cost.partial_prediction));
  }
  auto sub_label = CreateCBLabelDirect(_builder, v->l.cb_eval.event.weight, &costs);
  ex_builder.label = VW::parsers::flatbuffer::CreateCB_EVAL_Label(_builder, v->l.cb_eval.action, sub_label).Union();
  ex_builder.label_type = VW::parsers::flatbuffer::Label_CB_EVAL_Label;
}

void to_flat::create_mc_label(example* v, ExampleBuilder& ex_builder)
{
  ex_builder.label = VW::parsers::flatbuffer::CreateMultiClass(_builder, v->l.multi.label, v->l.multi.weight).Union();
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

  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::Example>> example_collection;
  std::vector<flatbuffers::Offset<VW::parsers::flatbuffer::MultiExample>> multi_example_collection;
  size_t collection_count = 0;
  example* ae = all.p->ready_parsed_examples.pop();
  int examples = 0;
  uint32_t multi_ex_index = 0;

  MultiExampleBuilder multi_ex_builder;
  ExampleBuilder ex_builder;

  while (ae != nullptr && !ae->end_pass)
  {
    // Create Label for current example
    switch (all.label_type)
    {
      case label_type_t::nolabel:
        to_flat::create_no_label(ae, ex_builder);
        break;
      case label_type_t::cb:
      {
        if (all.l->is_multiline) { to_flat::create_cb_label_multi_ex(ae, multi_ex_builder, multi_ex_index); }
        else
        {
          to_flat::create_cb_label(ae, ex_builder);
        }
        break;
      }
      case label_type_t::ccb:
        to_flat::create_ccb_label(ae, ex_builder);
        break;
      case label_type_t::multi:
        to_flat::create_multi_label(ae, ex_builder);
        break;
      case label_type_t::mc:
        to_flat::create_mc_label(ae, ex_builder);
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

      std::vector<VW::parsers::flatbuffer::FeatureNum> fts;

      for (features::iterator& f : ae->feature_space[ns])
      { fts.push_back(VW::parsers::flatbuffer::FeatureNum(f.index(), f.value())); }
      namespaces.push_back(VW::parsers::flatbuffer::CreateNamespaceDirect(_builder, nullptr, ns, &fts, nullptr));
    }
    std::string tag(ae->tag.begin(), ae->tag.size());
    // auto flat_namespaces =
    // _builder.CreateVector<flatbuffers::Offset<VW::parsers::flatbuffer::Namespace>>(namespaces);

    if (all.l->is_multiline)
    {
      // TODO share the namespaces
      multi_ex_builder.namespaces.insert(multi_ex_builder.namespaces.end(), namespaces.begin(), namespaces.end());
      multi_ex_index++;
      if (!example_is_newline(*ae))
      {
        examples++;
        ae = all.p->ready_parsed_examples.pop();
        continue;
      }
    }
    else
    {
      ex_builder.namespaces.insert(ex_builder.namespaces.end(), namespaces.begin(), namespaces.end());
      ex_builder.tag = _builder.CreateString(tag);
    }
    if (collection)
    {
      if (all.l->is_multiline)
      {
        auto multi_ex = VW::parsers::flatbuffer::CreateMultiExampleDirect(_builder, &multi_ex_builder.namespaces,
            multi_ex_builder.shared_type, multi_ex_builder.shared, multi_ex_builder.label_type, multi_ex_builder.label);

        multi_example_collection.push_back(multi_ex);
        multi_ex_index = 0;

        multi_ex_builder.namespaces.clear();
        multi_ex_builder.shared_type = VW::parsers::flatbuffer::Label_NONE;
        multi_ex_builder.shared = 0;
        multi_ex_builder.label_type = VW::parsers::flatbuffer::Label_NONE;
        multi_ex_builder.label = 0;
      }
      else
      {
        auto ex = VW::parsers::flatbuffer::CreateExampleDirect(
            _builder, &ex_builder.namespaces, ex_builder.label_type, ex_builder.label);
        example_collection.push_back(ex);
        ex_builder.namespaces.clear();

        ex_builder.label_type = VW::parsers::flatbuffer::Label_NONE;
        ex_builder.label = 0;
      }
      collection_count++;
      if (collection_count >= collection_size)
      {
        if (all.l->is_multiline)
        {
          auto egcollection =
              VW::parsers::flatbuffer::CreateMultiExampleCollectionDirect(_builder, &multi_example_collection);
          auto root = CreateExampleRoot(
              _builder, VW::parsers::flatbuffer::ExampleType_MultiExampleCollection, egcollection.Union());
          _builder.FinishSizePrefixed(root);
        }
        else
        {
          auto egcollection = VW::parsers::flatbuffer::CreateExampleCollectionDirect(_builder, &example_collection);
          auto root =
              CreateExampleRoot(_builder, VW::parsers::flatbuffer::ExampleType_ExampleCollection, egcollection.Union());
          _builder.FinishSizePrefixed(root);
        }
        uint8_t* buf = _builder.GetBufferPointer();
        int size = _builder.GetSize();
        outfile.write(reinterpret_cast<char*>(buf), size);
        _builder.Clear();
        example_collection.clear();
        multi_example_collection.clear();
        collection_count = 0;
      }
    }
    else
    {
      if (all.l->is_multiline)
      {
        auto multi_ex = VW::parsers::flatbuffer::CreateMultiExampleDirect(_builder, &multi_ex_builder.namespaces,
            multi_ex_builder.shared_type, multi_ex_builder.shared, multi_ex_builder.label_type, multi_ex_builder.label);

        multi_ex_index = 0;

        multi_ex_builder.namespaces.clear();
        multi_ex_builder.shared_type = VW::parsers::flatbuffer::Label_NONE;
        multi_ex_builder.shared = flatbuffers::Offset<void>();
        multi_ex_builder.label_type = VW::parsers::flatbuffer::Label_NONE;
        multi_ex_builder.label = flatbuffers::Offset<void>();

        auto root = VW::parsers::flatbuffer::CreateExampleRoot(
            _builder, VW::parsers::flatbuffer::ExampleType_MultiExample, multi_ex.Union());
        _builder.FinishSizePrefixed(root);
      }
      else
      {
        auto ex = VW::parsers::flatbuffer::CreateExampleDirect(
            _builder, &ex_builder.namespaces, ex_builder.label_type, ex_builder.label);
        ex_builder.namespaces.clear();

        ex_builder.label_type = VW::parsers::flatbuffer::Label_NONE;
        ex_builder.label = 0;
        auto root = VW::parsers::flatbuffer::CreateExampleRoot(
            _builder, VW::parsers::flatbuffer::ExampleType_Example, ex.Union());
        _builder.FinishSizePrefixed(root);
      }
      uint8_t* buf = _builder.GetBufferPointer();
      int size = _builder.GetSize();
      outfile.write(reinterpret_cast<char*>(buf), size);
      _builder.Clear();
    }

    examples++;
    ae = all.p->ready_parsed_examples.pop();
  }

  if (collection && collection_count > 0)
  {
    auto egcollection = VW::parsers::flatbuffer::CreateExampleCollectionDirect(_builder, &example_collection);
    auto root = VW::parsers::flatbuffer::CreateExampleRoot(
        _builder, VW::parsers::flatbuffer::ExampleType_ExampleCollection, egcollection.Union());
    _builder.FinishSizePrefixed(root);

    uint8_t* buf = _builder.GetBufferPointer();
    int size = _builder.GetSize();
    outfile.write(reinterpret_cast<char*>(buf), size);
  }

  // write collection again

  // all.trace_message << "\nLabel type " << get_label_string(all.label_type) << std::endl;
  // all.trace_message << "Converted " << examples << " examples" << std::endl;

  // all.trace_message << "Flatbuffer " << output_flatbuffer_name << " created" << std::endl;
}
