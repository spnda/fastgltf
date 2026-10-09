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

#ifndef FASTGLTF_ERROR_HPP
#define FASTGLTF_ERROR_HPP

#if !defined(FASTGLTF_MODULE)
#include <cassert>
#include <tuple>
#endif

#include <fastgltf/util.hpp>

namespace fastgltf {
	FASTGLTF_EXPORT enum class [[nodiscard]] Error : std::uint64_t {
		None = 0,
		InvalidPath = 1, ///< The glTF directory passed to load*GLTF is invalid.
		MissingExtensions = 2, ///< One or more extensions are required by the glTF but not enabled in the Parser.
		UnknownRequiredExtension = 3, ///< An extension required by the glTF is not supported by fastgltf.
		InvalidJson = 4, ///< An error occurred while parsing the JSON.
		InvalidGltf = 5, ///< The glTF is either missing something or has invalid data.
		InvalidOrMissingAssetField = 6, ///< The glTF asset object is missing or invalid.
		InvalidGLB = 7, ///< The GLB container is invalid.
		/**
		 * A field is missing in the JSON.
		 * @note This is only used internally.
		 */
		MissingField = 8,
		MissingExternalBuffer = 9, ///< With Options::LoadExternalBuffers, an external buffer was not found.
		UnsupportedVersion = 10, ///< The glTF version is not supported by fastgltf.
		InvalidURI = 11, ///< A URI from a buffer or image failed to be parsed.
		InvalidFileData = 12, ///< The file data is invalid, or the file type could not be determined.
		FailedWritingFiles = 13, ///< The exporter failed to write some files (buffers/images) to disk.
		FileBufferAllocationFailed = 14, ///< The constructor of GltfDataBuffer failed to allocate a sufficiently large buffer.
	};

	FASTGLTF_EXPORT [[nodiscard]] constexpr std::string_view getErrorName(const Error error) {
		switch (error) {
			case Error::None: return "None";
			case Error::InvalidPath: return "InvalidPath";
			case Error::MissingExtensions: return "MissingExtensions";
			case Error::UnknownRequiredExtension: return "UnknownRequiredExtension";
			case Error::InvalidJson: return "InvalidJson";
			case Error::InvalidGltf: return "InvalidGltf";
			case Error::InvalidOrMissingAssetField: return "InvalidOrMissingAssetField";
			case Error::InvalidGLB: return "InvalidGLB";
			case Error::MissingField: return "MissingField";
			case Error::MissingExternalBuffer: return "MissingExternalBuffer";
			case Error::UnsupportedVersion: return "UnsupportedVersion";
			case Error::InvalidURI: return "InvalidURI";
			case Error::InvalidFileData: return "InvalidFileData";
			case Error::FailedWritingFiles: return "FailedWritingFiles";
			case Error::FileBufferAllocationFailed: return "FileBufferAllocationFailed";
			default: FASTGLTF_UNREACHABLE
		}
	}

	FASTGLTF_EXPORT [[nodiscard]] constexpr std::string_view getErrorMessage(const Error error) {
		switch (error) {
			case Error::None: return "";
			case Error::InvalidPath: return "The glTF directory passed to load*GLTF is invalid";
			case Error::MissingExtensions: return "One or more extensions are required by the glTF but not enabled in the Parser.";
			case Error::UnknownRequiredExtension: return "An extension required by the glTF is not supported by fastgltf.";
			case Error::InvalidJson: return "An error occurred while parsing the JSON.";
			case Error::InvalidGltf: return "The glTF is either missing something or has invalid data.";
			case Error::InvalidOrMissingAssetField: return "The glTF asset object is missing or invalid.";
			case Error::InvalidGLB: return "The GLB container is invalid.";
			case Error::MissingField: return "";
			case Error::MissingExternalBuffer: return "An external buffer was not found.";
			case Error::UnsupportedVersion: return "The glTF version is not supported by fastgltf.";
			case Error::InvalidURI: return "A URI from a buffer or image failed to be parsed.";
			case Error::InvalidFileData: return "The file data is invalid, or the file type could not be determined.";
			case Error::FailedWritingFiles: return "The exporter failed to write some files (buffers/images) to disk.";
			case Error::FileBufferAllocationFailed: return "The constructor of GltfDataBuffer failed to allocate a sufficiently large buffer.";
			default: FASTGLTF_UNREACHABLE
		}
	}

	/**
	 * A tagged union that stores either T or an Error.
	 *
	 * To inspect if an error has occurred, use @ref hasError(), @ref error(), or @ref operator bool().
	 * If @ref hasError() returned false or @ref error() return @ref Error::None then one of the value getters,
	 * such as @ref get() or @ref operator*() can be used.
	 */
	FASTGLTF_EXPORT template <typename T>
	class [[nodiscard]] Expected {
		static_assert(!std::is_same_v<Error, T>);

		template <typename> friend class Expected;

		static constexpr bool isRef = std::is_reference_v<T>;

		using wrap = std::reference_wrapper<std::remove_reference_t<T>>;

		using storage_type = std::conditional_t<isRef, wrap, T>;
		using value_type = T;
		using error_type = Error;

		union {
			storage_type _valueStorage;
			error_type _errorStorage;
		};
		bool _hasError;

		using pointer = std::remove_reference_t<T>*;
		using const_pointer = const std::remove_reference_t<T>*;
		using reference = std::remove_reference_t<T>&;
		using const_reference = const std::remove_reference_t<T>&;

		[[nodiscard]] storage_type* getValueStorage() {
			assert(!_hasError && "Cannot get value when an error exists!");
			return &_valueStorage;
		}

		[[nodiscard]] const storage_type* getValueStorage() const {
			assert(!_hasError && "Cannot get value when an error exists!");
			return &_valueStorage;
		}

		[[nodiscard]] error_type* getErrorStorage() {
			assert(_hasError && "Cannot get error when a value exists!");
			return &_errorStorage;
		}

		[[nodiscard]] const error_type* getErrorStorage() const {
			assert(_hasError && "Cannot get error when a value exists!");
			return &_errorStorage;
		}

		[[nodiscard]] pointer toPointer(pointer value) {
			return value;
		}
		[[nodiscard]] const_pointer toPointer(const_pointer value) const {
			return value;
		}

		[[nodiscard]] pointer toPointer(wrap* value) {
			return &value->get();
		}
		[[nodiscard]] const_pointer toPointer(const wrap* value) const {
			return &value->get();
		}

		template <typename U>
		void moveConstruct(Expected<U>&& other) {
			_hasError = other._hasError;

			if (!_hasError)
				std::construct_at(getValueStorage(), std::move(*other.getValueStorage()));
			else
				std::construct_at(getErrorStorage(), std::move(*other.getErrorStorage()));
		}

	public:
		Expected(Error error) : _hasError(true) {
			assert(error != Error::None && "Cannot create Expected from successful Error");
			std::construct_at(getErrorStorage(), error);
		}

		Expected(T&& value)  noexcept(std::is_nothrow_move_constructible_v<T>) : _hasError(false) {
			std::construct_at(getValueStorage(), std::forward<T>(value));
		}
		template <typename U>
		requires std::is_convertible_v<U, T>
		Expected(U&& other) noexcept(std::is_nothrow_move_constructible_v<T>) : _hasError(false) {
			std::construct_at(getValueStorage(), std::forward<U>(other));
		}

		Expected(const Expected& other) = delete;
		Expected(Expected&& other) noexcept {
			moveConstruct(std::move(other));
		}
		template <typename U>
		requires std::is_convertible_v<U, T>
		explicit Expected(Expected<U>&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
			moveConstruct(std::move(other));
		}

		Expected& operator=(const Expected& other) = delete;
		Expected& operator=(Expected&& other) noexcept = delete;

		~Expected() {
			if (_hasError)
				getErrorStorage()->~error_type();
			else
				getValueStorage()->~storage_type();
		}

		[[nodiscard]] bool hasError() const {
			return _hasError;
		}
		[[nodiscard]] Error error() const {
			// TODO: Is this reasonable?
			return _hasError ? *getErrorStorage() : Error::None;
		}

		/**
		 * Returns a reference to the value of T.
		 * When error() returns anything but Error::None, the returned value is undefined.
		 */
		[[nodiscard]] reference get() {
			assert(!_hasError);
			return *getValueStorage();
		}
		[[nodiscard]] const_reference get() const {
			assert(!_hasError);
			return *getValueStorage();
		}

		/**
		 * Returns the address of the value of T, or nullptr if error() returns anything but Error::None.
		 */
		[[nodiscard]] pointer get_if() noexcept {
			if (_hasError)
				return nullptr;
			return toPointer(getValueStorage());
		}
		[[nodiscard]] const_pointer get_if() const noexcept {
			if (_hasError)
				return nullptr;
			return toPointer(getValueStorage());
		}

		[[nodiscard]] pointer operator->() {
			assert(!_hasError);
			return toPointer(getValueStorage());
		}
		[[nodiscard]] const_pointer operator->() const {
			assert(!_hasError);
			return toPointer(getValueStorage());
		}

		[[nodiscard]] reference operator*() {
			assert(!_hasError);
			return *getValueStorage();
		}
		[[nodiscard]] const_reference operator*() const {
			assert(!_hasError);
			return *getValueStorage();
		}

		[[nodiscard]] operator bool() const noexcept {
			return !_hasError;
		}
	};

	// If a reference_wrapper is passed explicitly, deduce it instead to T&
	template <typename T>
	Expected(std::reference_wrapper<T>) -> Expected<T&>;

	static_assert(!std::is_constructible_v<Expected<int&>, int>, "Cannot construct reference Expected from rvalue");
}

#endif
