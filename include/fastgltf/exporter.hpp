/*
 * Copyright (C) 2022 - 2026 Sean Apeler
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

#ifndef FASTGLTF_EXPORTER_HPP
#define FASTGLTF_EXPORTER_HPP

#if !defined(FASTGLTF_MODULE)
#include <filesystem>
#include <optional>
#include <vector>
#endif

#include <simdjson.h>

#include <fastgltf/error.hpp>
#include <fastgltf/types.hpp>

namespace fastgltf {
	// clang-format off
	FASTGLTF_EXPORT enum class ExportOptions : std::uint64_t {
		None                            = 0,

		/**
		 * Calls fastgltf::validate for the passed asset before writing.
		 */
		ValidateAsset                   = 1 << 1,

		/**
		 * Pretty-prints the outputted JSON. This option is ignored for binary glTFs.
		 */
		PrettyPrintJson                 = 1 << 2,
	};
	// clang-format on

	FASTGLTF_ARITHMETIC_OP_TEMPLATE_MACRO(ExportOptions, ExportOptions, |)
	FASTGLTF_ARITHMETIC_OP_TEMPLATE_MACRO(ExportOptions, ExportOptions, &)
	FASTGLTF_ASSIGNMENT_OP_TEMPLATE_MACRO(ExportOptions, ExportOptions, |)
	FASTGLTF_ASSIGNMENT_OP_TEMPLATE_MACRO(ExportOptions, ExportOptions, &)
	FASTGLTF_UNARY_OP_TEMPLATE_MACRO(ExportOptions, ~)

	FASTGLTF_EXPORT using ExtrasWriteCallback = std::optional<std::string>(std::size_t objectIndex, Category objectType, void* userPointer);

	/**
	 * This converts a compacted JSON string into a more readable pretty format.
	 */
	void prettyPrintJson(std::string& json);

	FASTGLTF_EXPORT template <typename T>
	struct ExportResult {
		T output;

		std::vector<std::optional<std::filesystem::path>> bufferPaths;
		std::vector<std::optional<std::filesystem::path>> imagePaths;
	};

	/**
	 * A exporter for serializing one or more glTF assets into JSON and GLB forms.
	 *
	 * @note This does not write anything to any files. This class only serializes data
	 * into memory structures, which can then be used to manually write them to disk.
	 * If you want to let fastgltf handle the file writing too, use fastgltf::FileExporter.
	 */
	FASTGLTF_EXPORT class Exporter {
	protected:
		Error errorCode = Error::None;
		ExportOptions options = ExportOptions::None;
		bool exportingBinary = false;

		std::filesystem::path bufferFolder = "";
		std::filesystem::path imageFolder = "";

		void* userPointer = nullptr;
		ExtrasWriteCallback* extrasWriteCallback = nullptr;

		std::vector<std::optional<std::filesystem::path>> bufferPaths;
		std::vector<std::optional<std::filesystem::path>> imagePaths;

		void writeAccessors(const Asset& asset, simdjson::builder::string_builder& builder);
		void writeAnimations(const Asset& asset, simdjson::builder::string_builder& builder);
		void writeBuffers(const Asset& asset, simdjson::builder::string_builder& builder);
		void writeBufferViews(const Asset& asset, simdjson::builder::string_builder& builder);
		void writeCameras(const Asset& asset, simdjson::builder::string_builder& builder);
		void writeImages(const Asset& asset, simdjson::builder::string_builder& builder);
		void writeLights(const Asset& asset, simdjson::builder::string_builder& builder);
		void writeMaterialExtensions(const Material& material, simdjson::builder::string_builder& builder);
		void writeMaterials(const Asset& asset, simdjson::builder::string_builder& builder);
		void writeMeshes(const Asset& asset, simdjson::builder::string_builder& builder);
		void writeNodes(const Asset& asset, simdjson::builder::string_builder& builder);
		void writeSamplers(const Asset& asset, simdjson::builder::string_builder& builder);
		void writeScenes(const Asset& asset, simdjson::builder::string_builder& builder);
		void writeSkins(const Asset& asset, simdjson::builder::string_builder& builder);
		void writeTextures(const Asset& asset, simdjson::builder::string_builder& builder);
#if FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES
		void writeShapes(const Asset& asset, simdjson::builder::string_builder& builder);
#endif
#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
		void writePhysicsMaterials(const Asset& asset, simdjson::builder::string_builder& builder);
		void writeCollisionFilters(const Asset& asset, simdjson::builder::string_builder& builder);
		void writePhysicsJoints(const Asset& asset, simdjson::builder::string_builder& builder);
#endif
		void writeExtensions(const Asset& asset, simdjson::builder::string_builder& builder);

		std::filesystem::path getBufferFilePath(const Asset& asset, std::size_t index);
		std::filesystem::path getImageFilePath(const Asset& asset, std::size_t index, MimeType mimeType);

		std::string writeJson(const Asset& asset);

	public:
		/**
		 * Sets the relative base path for buffer URIs.
		 *
		 * If folder.is_relative() returns false, this has no effect.
		 */
		void setBufferPath(std::filesystem::path folder);
		/**
		 * Sets the relative base path for image URIs.
		 *
		 * If folder.is_relative() returns false, this has no effect.
		 */
		void setImagePath(std::filesystem::path folder);

		void setExtrasWriteCallback(ExtrasWriteCallback* callback) noexcept;

		void setUserPointer(void* pointer) noexcept;

		/**
		 * Generates a glTF JSON string from the given asset.
		 */
		Expected<ExportResult<std::string>> writeGltfJson(const Asset& asset, ExportOptions options = ExportOptions::None);

		/**
		 * Generates a glTF binary (GLB) blob from the given asset.
		 *
		 * If the first buffer holds a sources::Vector, a sources::Array, a or sources::ByteView and the byte length is smaller than 2^32 (4.2GB),
		 * it will be embedded into the binary. Note that the returned vector might therefore get quite large.
		 */
		Expected<ExportResult<std::vector<std::byte>>> writeGltfBinary(const Asset& asset, ExportOptions options = ExportOptions::None);
	};

	/**
	 * A exporter for serializing one or more glTF files into JSON and GLB forms.
	 * This exporter builds upon Exporter by writing all files automatically to the
	 * given paths.
	 */
	FASTGLTF_EXPORT class FileExporter : public Exporter {
		using Exporter::writeGltfJson;
		using Exporter::writeGltfBinary;

	public:
		/**
		 * Writes a glTF JSON string generated from the given asset to the specified target file. This will also write
		 * all buffers and textures to disk using the buffer and image paths set using Exporter::setBufferPath and
		 * Exporter::setImagePath.
		 */
		Error writeGltfJson(const Asset& asset, const std::filesystem::path& target, ExportOptions options = ExportOptions::None);

		/**
		 * Writes a glTF binary (GLB) blob from the given asset to the specified target file. This will also write
		 * all buffers and textures to disk using the buffer and image paths set using Exporter::setBufferPath and
		 * Exporter::setImagePath.
		 *
		 * If the first buffer holds a sources::Vector, a sources::Array, a or sources::ByteView and the byte length is smaller than 2^32 (4.2GB),
		 * it will be embedded into the binary.
		 *
		 * \see Exporter::writeGltfBinary
		 */
		Error writeGltfBinary(const Asset& asset, const std::filesystem::path& target, ExportOptions options = ExportOptions::None);
	};
}

#endif
