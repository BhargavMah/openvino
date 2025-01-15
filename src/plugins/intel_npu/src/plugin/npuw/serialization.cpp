// Copyright (C) 2024-2025 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "serialization.hpp"

#include "intel_npu/config/config.hpp"
#include "logging.hpp"
#include "openvino/op/constant.hpp"
#include "spatial.hpp"

void ov::npuw::s11n::write(std::ostream& stream, const std::streampos& var) {
    stream.write(reinterpret_cast<const char*>(&var), sizeof var);
}

void ov::npuw::s11n::write(std::ostream& stream, const std::string& var) {
    auto var_size = var.size();
    stream.write(reinterpret_cast<const char*>(&var_size), sizeof var_size);
    stream.write(&var[0], var.size());
}

void ov::npuw::s11n::write(std::ostream& stream, const bool& var) {
    stream.write(reinterpret_cast<const char*>(&var), sizeof var);
}

void ov::npuw::s11n::write(std::ostream& stream, const ov::npuw::compiled::Spatial& var) {
    using ov::npuw::s11n::write;

    write(stream, var.params.size());
    for (const auto& p : var.params) {
        write(stream, p.idx);
        write(stream, p.dim);
    }
    write(stream, var.range);
    write(stream, var.nway);
    write(stream, var.out_dim);
    write(stream, var.nway_iters);
    write(stream, var.tail_size);
}

void ov::npuw::s11n::write(std::ostream& stream, const ov::Tensor& var) {
    using ov::npuw::s11n::write;

    if (!var) {
        write(stream, false);
        return;
    }
    write(stream, true);

    auto type_str = var.get_element_type().to_string();
    write(stream, type_str);
    write(stream, var.get_shape());
    write(stream, var.get_byte_size());

    ov::Tensor tensor;
    if (var.is_continuous()) {
        tensor = var;
    } else {
        // Just copy strided tensor to a non-strided one
        tensor = ov::Tensor(var.get_element_type(), var.get_shape());
        var.copy_to(tensor);
    }
    NPUW_ASSERT(tensor);
    stream.write(reinterpret_cast<const char*>(var.data()), var.get_byte_size());
}

void ov::npuw::s11n::write(std::ostream& stream, const ::intel_npu::Config& var) {
    write(stream, var.toString());
}

void ov::npuw::s11n::write(std::ostream& stream, const ov::Output<const ov::Node>& var) {
    write(stream, var.get_element_type().to_string());
    write(stream, var.get_partial_shape().to_string());
    write(stream, var.get_names());
}

void ov::npuw::s11n::read(std::istream& stream, std::streampos& var) {
    stream.read(reinterpret_cast<char*>(&var), sizeof var);
}

void ov::npuw::s11n::read(std::istream& stream, std::string& var) {
    std::size_t var_size = 0;
    stream.read(reinterpret_cast<char*>(&var_size), sizeof var_size);
    var.resize(var_size);
    stream.read(&var[0], var_size);
}

void ov::npuw::s11n::read(std::istream& stream, bool& var) {
    stream.read(reinterpret_cast<char*>(&var), sizeof var);
}

void ov::npuw::s11n::read(std::istream& stream, ov::npuw::compiled::Spatial& var) {
    using ov::npuw::s11n::read;

    ov::npuw::compiled::Spatial spat;
    std::size_t params_size = 0;
    read(stream, params_size);
    for (std::size_t i = 0; i < params_size; ++i) {
        ov::npuw::compiled::Spatial::Param p;
        read(stream, p.idx);
        read(stream, p.dim);
        spat.params.push_back(p);
    }
    read(stream, spat.range);
    read(stream, spat.nway);
    read(stream, spat.out_dim);
    read(stream, spat.nway_iters);
    read(stream, spat.tail_size);
}

void ov::npuw::s11n::read(std::istream& stream, ov::Tensor& var) {
    bool is_initialized = false;
    read(stream, is_initialized);

    if (!is_initialized) {
        return;
    }

    std::string type_str;
    read(stream, type_str);
    ov::element::Type type(type_str);

    ov::Shape shape;
    read(stream, shape);

    std::size_t byte_size = 0;
    read(stream, byte_size);

    var = ov::Tensor(type, shape);

    stream.read(reinterpret_cast<char*>(var.data()), byte_size);
}

void ov::npuw::s11n::read(std::istream& stream, ::intel_npu::Config& var) {
    std::string str;
    read(stream, str);
    var.fromString(str);
}

void ov::npuw::s11n::read(std::istream& stream, std::shared_ptr<ov::op::v0::Parameter>& var) {
    std::string elem_type_str;
    std::string part_shape_str;
    std::unordered_set<std::string> names;
    read(stream, elem_type_str);
    read(stream, part_shape_str);
    read(stream, names);
    // NOTE: the code below is taken from NPU plugin's create_dummy_model()
    var = std::make_shared<op::v0::Parameter>(ov::element::Type(elem_type_str), ov::PartialShape(part_shape_str));
    var->set_friendly_name(*names.begin());  // FIXME: any_name ?
    var->output(0).get_tensor().set_names(names);
}

void ov::npuw::s11n::read(std::istream& stream, std::shared_ptr<ov::Node>& var) {
    std::string elem_type_str;
    std::string part_shape_str;
    std::unordered_set<std::string> names;
    read(stream, elem_type_str);
    read(stream, part_shape_str);
    read(stream, names);
    // NOTE: the code below is taken from NPU plugin's create_dummy_model()
    std::shared_ptr<ov::Node> res =
        std::make_shared<ov::op::v0::Constant>(ov::element::Type(elem_type_str), std::vector<size_t>{1});
    // FIXME: serialize names as well?
    const std::shared_ptr<ov::descriptor::Tensor>& tensor_dummy =
        std::make_shared<ov::descriptor::Tensor>(ov::element::Type(elem_type_str),
                                                 ov::PartialShape(part_shape_str),
                                                 names);
    var = std::make_shared<ov::op::v0::Result>(res);
    var->output(0).set_tensor_ptr(tensor_dummy);
    var->set_friendly_name(*names.begin());  // any_name ?
}