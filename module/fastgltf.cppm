/*
 * Copyright (C) 2022 - 2024 spnda
 * This file is part of fastgltf <https://github.com/spnda/fastgltf>.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
module;

#include <fastgltf/util.hpp> // This defines FASTGLTF_USE_STD_MODULE if it wasn't already

#if !FASTGLTF_USE_STD_MODULE
#include <fastgltf/core.hpp>
#endif

export module fastgltf;

#if FASTGLTF_USE_STD_MODULE
#include <cassert>
import std;
import std.compat;

extern "C++" {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winclude-angled-in-module-purview"
#include <fastgltf/core.hpp>
#pragma clang diagnostic pop
}
#endif

export namespace fastgltf {
namespace math {
// vector.
using math::vec;
// vector aliases.
using math::s8vec2;
using math::s8vec3;
using math::s8vec4;
using math::u8vec2;
using math::u8vec3;
using math::u8vec4;
using math::s16vec2;
using math::s16vec3;
using math::s16vec4;
using math::u16vec2;
using math::u16vec3;
using math::u16vec4;
using math::s32vec2;
using math::s32vec3;
using math::s32vec4;
using math::u32vec2;
using math::u32vec3;
using math::u32vec4;
using math::fvec2;
using math::fvec3;
using math::fvec4;
using math::f32vec2;
using math::f32vec3;
using math::f32vec4;
using math::dvec2;
using math::dvec3;
using math::dvec4;
using math::f64vec2;
using math::f64vec3;
using math::f64vec4;
// vector functions.
using math::sum;
using math::dot;
using math::cross;
using math::length;
using math::normalize;

// quaternion.
using math::quat;
// quaternion aliases.
using math::fquat;
using math::dquat;
// quaternion functions.
using math::asMatrix;

// matrix.
using math::mat;
// matrix aliases.
using math::fmat2x2;
using math::fmat3x3;
using math::fmat4x4;
using math::dmat2x2;
using math::dmat3x3;
using math::dmat4x4;
// matrix functions.
using math::translate;
using math::scale;
using math::rotate;
using math::transpose;
using math::decomposeTransformMatrix;
}

namespace sources {
using sources::Array;
using sources::BufferView;
using sources::ByteView;
using sources::CustomBuffer;
using sources::Fallback;
using sources::URI;
using sources::Vector;
}

using fastgltf::Accessor;
using fastgltf::AccessorType;
using fastgltf::AlphaMode;
#if defined(__ANDROID__)
using fastgltf::AndroidGltfDataBuffer;
#endif
using fastgltf::Animation;
using fastgltf::AnimationChannel;
using fastgltf::AnimationSampler;
using fastgltf::Asset;
using fastgltf::AssetInfo;
using fastgltf::Buffer;
using fastgltf::BufferTarget;
using fastgltf::BufferView;
using fastgltf::Camera;
using fastgltf::Category;
using fastgltf::ComponentType;
using fastgltf::CompressedBufferView;
using fastgltf::DataSource;
using fastgltf::Error;
using fastgltf::Expected;
using fastgltf::ExportOptions;
using fastgltf::Exporter;
using fastgltf::Extensions;
using fastgltf::Filter;
using fastgltf::GltfDataBuffer;
using fastgltf::GltfFileStream;
using fastgltf::Image;
using fastgltf::Light;
#if FASTGLTF_HAS_MEMORY_MAPPED_FILE
using fastgltf::MappedGltfFile;
#endif
using fastgltf::Material;
using fastgltf::MaterialAnisotropy;
using fastgltf::MaterialSpecular;
using fastgltf::MaterialIridescence;
using fastgltf::MaterialVolume;
using fastgltf::MaterialTransmission;
using fastgltf::MaterialClearcoat;
using fastgltf::MaterialSheen;
#if FASTGLTF_ENABLE_DEPRECATED_EXT
using fastgltf::MaterialSpecularGlossiness;
#endif
using fastgltf::MaterialPackedTextures;
using fastgltf::Mesh;
using fastgltf::MimeType;
using fastgltf::Node;
using fastgltf::NormalTextureInfo;
using fastgltf::OcclusionTextureInfo;
using fastgltf::Optional;
using fastgltf::OptionalFlagValue;
using fastgltf::OptionalWithFlagValue;
using fastgltf::Options;
using fastgltf::Parser;
using fastgltf::PrimitiveType;
using fastgltf::Primitive;
using fastgltf::Sampler;
using fastgltf::Scene;
using fastgltf::Skin;
using fastgltf::SmallVector;
using fastgltf::span;
using fastgltf::SparseAccessor;
using fastgltf::StaticVector;
using fastgltf::Texture;
using fastgltf::TextureInfo;
using fastgltf::TRS;
using fastgltf::URI;
using fastgltf::URIView;
using fastgltf::visitor;
using fastgltf::Wrap;

using fastgltf::operator|;
using fastgltf::operator&;
using fastgltf::operator|=;
using fastgltf::operator&=;
using fastgltf::operator~;

using fastgltf::validate;
using fastgltf::getElementByteSize;
using fastgltf::getErrorName;
using fastgltf::getErrorMessage;
}