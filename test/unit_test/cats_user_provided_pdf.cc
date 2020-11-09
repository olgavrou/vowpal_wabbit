#ifndef STATIC_LINK_VW
#  define BOOST_TEST_DYN_LINK
#endif

#include <boost/test/unit_test.hpp>
#include <boost/test/test_tools.hpp>

#include "test_common.h"

#include <vector>

BOOST_AUTO_TEST_CASE(cats_no_model_action_provided)
{
  std::string json_text = R"(
{
  "Version": "1",
  "EventId": "event_id",
  "pdf": [
    {"action": 185.121}
  ],
  "c": {
    "f":1
  },
  "VWState": {
    "m": "N/A"
  }
}
)";
  auto vw = VW::initialize(
      "--dsjson --cats_pdf 4 --min_value=185 --max_value=23959 --bandwidth 1 --no_stdin --quiet --first_only", nullptr,
      false, nullptr, nullptr);
  auto examples = parse_dsjson(*vw, json_text);

  BOOST_CHECK_EQUAL(examples.size(), 1);

  BOOST_CHECK_EQUAL(examples[0]->pred.pdf.size(), 1);
  BOOST_CHECK_CLOSE(examples[0]->pred.pdf[0].left, 185.121, FLOAT_TOL);
  BOOST_CHECK_CLOSE(examples[0]->pred.pdf[0].right, 0., FLOAT_TOL);
  BOOST_CHECK_CLOSE(examples[0]->pred.pdf[0].pdf_value, 0., FLOAT_TOL);

  vw->predict(*examples[0]);

  BOOST_CHECK_GT(examples[0]->pred.pdf.size(), 1);

  float sum = 0;
  for (auto& p : examples[0]->pred.pdf) { sum += (p.right - p.left) * p.pdf_value; }
  BOOST_CHECK_CLOSE(sum, 1.f, FLOAT_TOL);

  VW::finish_example(*vw, examples);
  VW::finish(*vw);
}

BOOST_AUTO_TEST_CASE(cats_no_model_uniform_random)
{
  std::string json_text = R"(
{
  "Version": "1",
  "EventId": "event_id",
  "c": {
    "f":1
  },
  "VWState": {
    "m": "N/A"
  }
}
)";

  float min_value = 185;
  float max_value = 23959;
  float epsilon = 0.1f;
  auto vw = VW::initialize("--dsjson --cats_pdf 4 --min_value=" + std::to_string(min_value) +
          " --max_value=" + std::to_string(max_value) + " --epsilon " + std::to_string(epsilon) +
          " --bandwidth 1 --no_stdin --quiet --first_only",
      nullptr, false, nullptr, nullptr);
  auto examples = parse_dsjson(*vw, json_text);

  BOOST_CHECK_EQUAL(examples.size(), 1);

  BOOST_CHECK_EQUAL(examples[0]->pred.pdf.size(), 0);

  vw->predict(*examples[0]);

  float sum = 0;
  for (auto& p : examples[0]->pred.pdf) { sum += (p.right - p.left) * p.pdf_value; }
  BOOST_CHECK_CLOSE(sum, 1.f, FLOAT_TOL);

  BOOST_CHECK_EQUAL(examples[0]->pred.pdf.size(), 1);
  BOOST_CHECK_CLOSE(examples[0]->pred.pdf[0].left, min_value, FLOAT_TOL);
  BOOST_CHECK_CLOSE(examples[0]->pred.pdf[0].right, max_value, FLOAT_TOL);
  BOOST_CHECK_CLOSE(examples[0]->pred.pdf[0].pdf_value, static_cast<float>(1.f / (max_value - min_value)), FLOAT_TOL);

  VW::finish_example(*vw, examples);
  VW::finish(*vw);
}

BOOST_AUTO_TEST_CASE(cats_no_model_pdf_provided)
{
  std::string json_text = R"(
{
  "Version": "1",
  "EventId": "event_id",
  "pdf" : [{"left": 185, "right": 8109.67, "pdf_value": 2.10314e-06}, {"left": 8109.67, "right": 23959, "pdf_value": 6.20426e-05}],
  "c": {
    "f":1
  },
  "VWState": {
    "m": "N/A"
  }
}
)";

  float min_value = 185;
  float max_value = 23959;
  float epsilon = 0.1f;
  auto vw = VW::initialize("--dsjson --cats_pdf 4 --min_value=" + std::to_string(min_value) +
          " --max_value=" + std::to_string(max_value) + " --epsilon " + std::to_string(epsilon) +
          " --bandwidth 1 --no_stdin --quiet --first_only",
      nullptr, false, nullptr, nullptr);
  auto examples = parse_dsjson(*vw, json_text);

  BOOST_CHECK_EQUAL(examples.size(), 1);

  BOOST_CHECK_EQUAL(examples[0]->pred.pdf.size(), 2);

  vw->predict(*examples[0]);

  float sum = 0;
  for (auto& p : examples[0]->pred.pdf) { sum += (p.right - p.left) * p.pdf_value; }
  BOOST_CHECK_CLOSE(sum, 1.f, FLOAT_TOL);

  BOOST_CHECK_EQUAL(examples[0]->pred.pdf.size(), 2);
  BOOST_CHECK_CLOSE(examples[0]->pred.pdf[0].left, min_value, FLOAT_TOL);
  BOOST_CHECK_CLOSE(examples[0]->pred.pdf[0].right, 8109.67, FLOAT_TOL);
  BOOST_CHECK_CLOSE(examples[0]->pred.pdf[0].pdf_value, 2.10314e-06, FLOAT_TOL);

  BOOST_CHECK_CLOSE(examples[0]->pred.pdf[1].left, 8109.67, FLOAT_TOL);
  BOOST_CHECK_CLOSE(examples[0]->pred.pdf[1].right, max_value, FLOAT_TOL);
  BOOST_CHECK_CLOSE(examples[0]->pred.pdf[1].pdf_value, 6.20426e-05, FLOAT_TOL);

  VW::finish_example(*vw, examples);
  VW::finish(*vw);
}