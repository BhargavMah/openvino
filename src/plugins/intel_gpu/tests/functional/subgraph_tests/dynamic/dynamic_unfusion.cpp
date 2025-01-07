// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "common_test_utils/ov_tensor_utils.hpp"
#include "common_test_utils/file_utils.hpp"
#include "shared_test_classes/base/ov_subgraph.hpp"

#include "openvino/op/parameter.hpp"
#include "openvino/op/matmul.hpp"
#include "openvino/op/multiply.hpp"

namespace {

using ov::test::InputShape;

using DynamicUnfusionsParams = std::tuple<std::vector<InputShape>,   // input shapes
                                          ov::element::Type>;        // input precision

class DynamicUnfusions : public testing::WithParamInterface<DynamicUnfusionsParams>,
                         virtual public ov::test::SubgraphBaseTest {
public:
    static std::string getTestCaseName(testing::TestParamInfo<DynamicUnfusionsParams> obj) {
        std::vector<InputShape> input_shapes;
        ov::element::Type input_precision;

        std::tie(input_shapes, input_precision) = obj.param;

        std::ostringstream result;
        result << "IS=(";
        for (const auto& shape : input_shapes) {
            result << ov::test::utils::partialShape2str({shape.first}) << "_";
        }
        result << ")_TS=";
        for (const auto& shape : input_shapes) {
            result << "(";
            if (!shape.second.empty()) {
                auto itr = shape.second.begin();
                do {
                    result << ov::test::utils::vec2str(*itr);
                } while (++itr != shape.second.end() && result << "_");
            }
            result << ")_";
        }
        result << "input_precision=" << input_precision;
        return result.str();
    }

protected:
    std::shared_ptr<ov::Model> init_subgraph(std::vector<ov::PartialShape>& input_shapes,
                                             const ov::element::Type input_precision) {
        auto input0 = std::make_shared<ov::op::v0::Parameter>(input_precision, input_shapes[0]);
        auto input1 = std::make_shared<ov::op::v0::Parameter>(input_precision, input_shapes[1]);
        auto input2 = std::make_shared<ov::op::v0::Parameter>(input_precision, input_shapes[2]);

        auto matmul = std::make_shared<ov::op::v0::MatMul>(input0, input1);
        auto mul = std::make_shared<ov::op::v1::Multiply>(matmul, input2);

        matmul->set_friendly_name("MatMul");
        mul->set_friendly_name("Multiply");

        return std::make_shared<ov::Model>(ov::NodeVector{mul}, ov::ParameterVector{input0, input1, input2}, "DynamicUnfusions");
    }

    void SetUp() override {
        targetDevice = ov::test::utils::DEVICE_GPU;

        std::vector<InputShape> input_shapes;
        ov::element::Type input_precision;

        std::tie(input_shapes, input_precision) = GetParam();

        init_input_shapes(input_shapes);

        inType = outType = input_precision;
        function = init_subgraph(inputDynamicShapes, input_precision);
    }
};

TEST_P(DynamicUnfusions, Inference) {
    run();
}

const std::vector<ov::element::Type> input_precisions = {ov::element::f32};

const std::vector<std::vector<InputShape>> input_shapes_dyn = {
    {{{1024, -1}, {{1024, 1024}}}, {{-1, 1024}, {{1024, 1024}}}, {{1, -1}, {{1, 1}}}},
};

INSTANTIATE_TEST_SUITE_P(DynamicUnfusions_basic,
                         DynamicUnfusions,
                         ::testing::Combine(::testing::ValuesIn(input_shapes_dyn),
                                            ::testing::ValuesIn(input_precisions)),
                         DynamicUnfusions::getTestCaseName);
} // namespace