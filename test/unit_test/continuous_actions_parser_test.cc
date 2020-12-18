#ifndef STATIC_LINK_VW
#  define BOOST_TEST_DYN_LINK
#endif

#include <boost/test/unit_test.hpp>
#include <boost/test/test_tools.hpp>

#include "test_common.h"

#include <vector>
#include "cb_continuous_label.h"
#include "parser.h"
#include <memory>

void parse_label(label_parser& lp, parser* p, VW::string_view label, VW::cb_continuous::continuous_label& l)
{
  tokenize(' ', label, p->words);
  lp.default_label(&l);
  reduction_features red_fts;
  lp.parse_label(p, nullptr, &l, p->words, red_fts);
}

BOOST_AUTO_TEST_CASE(continuous_actions_parse_label)
{
  auto lp = VW::cb_continuous::the_label_parser;
  parser p{8 /*ring_size*/, false /*strict parse*/};

  {
    auto label = scoped_calloc_or_throw<VW::cb_continuous::continuous_label>();
    parse_label(lp, &p, "ca 185.121:0.657567:6.20426e-05", *label);
    BOOST_CHECK_CLOSE(label->costs[0].pdf_value, 6.20426e-05, FLOAT_TOL);
    BOOST_CHECK_CLOSE(label->costs[0].cost, 0.657567, FLOAT_TOL);
    BOOST_CHECK_CLOSE(label->costs[0].action, 185.121, FLOAT_TOL);

    lp.delete_label(label.get());
  }
}

BOOST_AUTO_TEST_CASE(continuous_actions_parse_label_and_pdf)
{
  auto lp = VW::cb_continuous::the_label_parser;
  parser p{8 /*ring_size*/, false /*strict parse*/};

  {
    auto label = scoped_calloc_or_throw<VW::cb_continuous::continuous_label>();
    parse_label(lp, &p, "ca 185.121:0.657567:6.20426e-05 pdf 1:2:3 4:5:6", *label);
    BOOST_CHECK_CLOSE(label->costs[0].pdf_value, 6.20426e-05, FLOAT_TOL);
    BOOST_CHECK_CLOSE(label->costs[0].cost, 0.657567, FLOAT_TOL);
    BOOST_CHECK_CLOSE(label->costs[0].action, 185.121, FLOAT_TOL);

    lp.delete_label(label.get());
  }
}

BOOST_AUTO_TEST_CASE(continuous_actions_parse_only_pdf)
{
  auto lp = VW::cb_continuous::the_label_parser;
  parser p{8 /*ring_size*/, false /*strict parse*/};

  {
    auto label = scoped_calloc_or_throw<VW::cb_continuous::continuous_label>();
    parse_label(lp, &p, "ca pdf 1:2:3 4:5:6", *label);
    BOOST_CHECK_EQUAL(label->costs.size(), 0);

    lp.delete_label(label.get());
  }
}

BOOST_AUTO_TEST_CASE(continuous_actions_parse_label_and_chosen_action)
{
  auto lp = VW::cb_continuous::the_label_parser;
  parser p{8 /*ring_size*/, false /*strict parse*/};

  {
    auto label = scoped_calloc_or_throw<VW::cb_continuous::continuous_label>();
    parse_label(lp, &p, "ca 185.121:0.657567:6.20426e-05 chosen_action 185.121", *label);
    BOOST_CHECK_CLOSE(label->costs[0].pdf_value, 6.20426e-05, FLOAT_TOL);
    BOOST_CHECK_CLOSE(label->costs[0].cost, 0.657567, FLOAT_TOL);
    BOOST_CHECK_CLOSE(label->costs[0].action, 185.121, FLOAT_TOL);

    lp.delete_label(label.get());
  }
}

BOOST_AUTO_TEST_CASE(continuous_actions_chosen_action_only)
{
  auto lp = VW::cb_continuous::the_label_parser;
  parser p{8 /*ring_size*/, false /*strict parse*/};

  {
    auto label = scoped_calloc_or_throw<VW::cb_continuous::continuous_label>();
    // parse_label(lp, &p, "ca 185.121:0.657567:6.20426e-05 pdf 1:2:3 4:5:6", *label);
    // parse_label(lp, &p, "ca pdf 1:2:3 4:5:6", *label);
    parse_label(lp, &p, "ca chosen_action 185.121", *label);
    // TODO combinations of the above (chosen action and then pdf, pdf and then chosen action) and make sure that all combinations are OK and none cause problems
    // parse_label(lp, &p, "ca ", *label);
    BOOST_CHECK_EQUAL(label->costs.size(), 0);

    lp.delete_label(label.get());
  }
}

BOOST_AUTO_TEST_CASE(continuous_actions_parse_no_label)
{
  auto lp = VW::cb_continuous::the_label_parser;
  parser p{8 /*ring_size*/, false /*strict parse*/};

  {
    auto label = scoped_calloc_or_throw<VW::cb_continuous::continuous_label>();
    parse_label(lp, &p, "", *label);
    BOOST_CHECK_EQUAL(label->costs.size(), 0);

    lp.delete_label(label.get());
  }
}

BOOST_AUTO_TEST_CASE(continus_actions_check_label_for_prefix)
{
  auto lp = VW::cb_continuous::the_label_parser;
  parser p{8 /*ring_size*/, false /*strict parse*/};

  {
    auto label = scoped_calloc_or_throw<VW::cb_continuous::continuous_label>();
    BOOST_REQUIRE_THROW(parse_label(lp, &p, "185.121:0.657567:6.20426e-05", *label), VW::vw_exception);
    lp.delete_label(label.get());
  }
}