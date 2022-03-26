/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/fastertransformer/models/t5/T5DecoderLayerWeight.h"
#include "src/fastertransformer/utils/logger.h"
#include "src/fastertransformer/utils/memory_utils.h"

namespace fastertransformer {

template<typename T>
T5DecoderLayerWeight<T>::T5DecoderLayerWeight(const size_t head_num,
                                              const size_t size_per_head,
                                              const size_t d_model,
                                              const size_t inter_size,
                                              const size_t mem_d_model,
                                              const size_t tensor_para_size,
                                              const size_t tensor_para_rank):
    head_num_(head_num),
    size_per_head_(size_per_head),
    d_model_(d_model),
    inter_size_(inter_size),
    mem_d_model_(mem_d_model),
    tensor_para_size_(tensor_para_size),
    tensor_para_rank_(tensor_para_rank)
{
    FT_LOG_DEBUG("T5DecoderLayerWeight " + std::string(__func__) + " start");

    initialize();
    mallocWeights();
    setWeightPtr();

    FT_LOG_DEBUG("T5DecoderLayerWeight " + std::string(__func__) + " end");
}

template<typename T>
void T5DecoderLayerWeight<T>::initialize()
{
    FT_LOG_DEBUG("T5DecoderLayerWeight " + std::string(__func__) + " start");

    weights_size[0] = d_model_;
    weights_size[1] = d_model_ * 3 * (head_num_ / tensor_para_size_) * size_per_head_;
    weights_size[2] = (head_num_ / tensor_para_size_) * size_per_head_ * d_model_;
    weights_size[3] = d_model_;
    weights_size[4] = d_model_ * (head_num_ / tensor_para_size_) * size_per_head_;
    weights_size[5] = mem_d_model_ * (head_num_ / tensor_para_size_) * size_per_head_;
    weights_size[6] = mem_d_model_ * (head_num_ / tensor_para_size_) * size_per_head_;
    weights_size[7] = (head_num_ / tensor_para_size_) * size_per_head_ * d_model_;
    weights_size[8] = d_model_;
    weights_size[9] = d_model_ * (inter_size_ / tensor_para_size_);
    weights_size[10] = (inter_size_ / tensor_para_size_) * d_model_;

    FT_LOG_DEBUG("T5DecoderLayerWeight " + std::string(__func__) + " end");
}

template<typename T>
T5DecoderLayerWeight<T>::~T5DecoderLayerWeight()
{
    FT_LOG_DEBUG("T5DecoderLayerWeight " + std::string(__func__) + " start");

    if (is_maintain_buffer == true) {
        for (int i = 0; i < weights_num_; i++) {
            deviceFree(weights_ptr[i]);
        }

        pre_layernorm_weights.gamma = nullptr;
        self_attention_weights.query_weight.kernel = nullptr;
        self_attention_weights.attention_output_weight.kernel = nullptr;
        self_attn_layernorm_weights.gamma = nullptr;

        cross_attention_weights.query_weight.kernel = nullptr;
        cross_attention_weights.key_weight.kernel = nullptr;
        cross_attention_weights.value_weight.kernel = nullptr;
        cross_attention_weights.attention_output_weight.kernel = nullptr;
        cross_attn_layernorm_weights.gamma = nullptr;

        ffn_weights.intermediate_weight.kernel = nullptr;
        ffn_weights.output_weight.kernel = nullptr;
        is_maintain_buffer = false;
    }

    FT_LOG_DEBUG("T5DecoderLayerWeight " + std::string(__func__) + " end");
}

template<typename T>
T5DecoderLayerWeight<T>::T5DecoderLayerWeight(const T5DecoderLayerWeight& other):
    head_num_(other.head_num_),
    size_per_head_(other.size_per_head_),
    d_model_(other.d_model_),
    inter_size_(other.inter_size_),
    mem_d_model_(other.mem_d_model_),
    tensor_para_size_(other.tensor_para_size_),
    tensor_para_rank_(other.tensor_para_rank_)
{

    initialize();
    mallocWeights();
    for (int i = 0; i < weights_num_; i++) {
        cudaD2Dcpy(weights_ptr[i], other.weights_ptr[i], weights_size[i]);
    }
    setWeightPtr();
}

template<typename T>
T5DecoderLayerWeight<T>& T5DecoderLayerWeight<T>::operator=(const T5DecoderLayerWeight& other)
{
    head_num_ = other.head_num_;
    size_per_head_ = other.size_per_head_;
    d_model_ = other.d_model_;
    inter_size_ = other.inter_size_;
    mem_d_model_ = other.mem_d_model_;
    tensor_para_size_ = other.tensor_para_size_;
    tensor_para_rank_ = other.tensor_para_rank_;

    initialize();
    mallocWeights();
    for (int i = 0; i < weights_num_; i++) {
        cudaD2Dcpy(weights_ptr[i], other.weights_ptr[i], weights_size[i]);
    }
    setWeightPtr();

    return *this;
}

template<typename T>
void T5DecoderLayerWeight<T>::setWeightPtr()
{
    pre_layernorm_weights.gamma = weights_ptr[0];
    self_attention_weights.query_weight.kernel = weights_ptr[1];
    self_attention_weights.attention_output_weight.kernel = weights_ptr[2];
    self_attn_layernorm_weights.gamma = weights_ptr[3];

    cross_attention_weights.query_weight.kernel = weights_ptr[4];
    cross_attention_weights.key_weight.kernel = weights_ptr[5];
    cross_attention_weights.value_weight.kernel = weights_ptr[6];
    cross_attention_weights.attention_output_weight.kernel = weights_ptr[7];
    cross_attn_layernorm_weights.gamma = weights_ptr[8];

    ffn_weights.intermediate_weight.kernel = weights_ptr[9];
    ffn_weights.output_weight.kernel = weights_ptr[10];
}

template<typename T>
void T5DecoderLayerWeight<T>::mallocWeights()
{
    for (int i = 0; i < weights_num_; i++) {
        deviceMalloc(&weights_ptr[i], weights_size[i]);
    }
    is_maintain_buffer = true;
}

template<typename T>
void T5DecoderLayerWeight<T>::loadModel(std::string dir_path)
{
    FT_LOG_DEBUG("T5DecoderLayerWeight " + std::string(__func__) + " start");

    FT_CHECK(is_maintain_buffer == true);
    loadWeightFromBin<T>(weights_ptr[0], {weights_size[0]}, dir_path + "layer.0.layer_norm.weight.bin");
    loadWeightFromBin<T>(weights_ptr[1],
                         {weights_size[1]},
                         dir_path + "layer.0.SelfAttention.qkv.weight." + std::to_string(tensor_para_rank_) + ".bin");
    loadWeightFromBin<T>(weights_ptr[2],
                         {weights_size[2]},
                         dir_path + "layer.0.SelfAttention.o.weight." + std::to_string(tensor_para_rank_) + ".bin");
    loadWeightFromBin<T>(weights_ptr[3], {weights_size[3]}, dir_path + "layer.1.layer_norm.weight.bin");
    loadWeightFromBin<T>(weights_ptr[4],
                         {weights_size[4]},
                         dir_path + "layer.1.EncDecAttention.q.weight." + std::to_string(tensor_para_rank_) + ".bin");
    loadWeightFromBin<T>(weights_ptr[5],
                         {weights_size[5]},
                         dir_path + "layer.1.EncDecAttention.k.weight." + std::to_string(tensor_para_rank_) + ".bin");
    loadWeightFromBin<T>(weights_ptr[6],
                         {weights_size[6]},
                         dir_path + "layer.1.EncDecAttention.v.weight." + std::to_string(tensor_para_rank_) + ".bin");
    loadWeightFromBin<T>(weights_ptr[7],
                         {weights_size[7]},
                         dir_path + "layer.1.EncDecAttention.o.weight." + std::to_string(tensor_para_rank_) + ".bin");
    loadWeightFromBin<T>(weights_ptr[8], {weights_size[8]}, dir_path + "layer.2.layer_norm.weight.bin");
    loadWeightFromBin<T>(weights_ptr[9],
                         {weights_size[9]},
                         dir_path + "layer.2.DenseReluDense.wi.weight." + std::to_string(tensor_para_rank_) + ".bin");
    loadWeightFromBin<T>(weights_ptr[10],
                         {weights_size[10]},
                         dir_path + "layer.2.DenseReluDense.wo.weight." + std::to_string(tensor_para_rank_) + ".bin");

    FT_LOG_DEBUG("T5DecoderLayerWeight " + std::string(__func__) + " end");
}

template struct T5DecoderLayerWeight<float>;
template struct T5DecoderLayerWeight<half>;

}  // namespace fastertransformer