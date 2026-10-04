#ifndef FASTGLTF_ERROR_HPP
#define FASTGLTF_ERROR_HPP

#if !defined(FASTGLTF_MODULE)
#include <cassert>
#include <tuple>
#endif

#include <fastgltf/util.hpp>

namespace fastgltf {
	enum class Error : std::uint64_t {
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

	FASTGLTF_EXPORT constexpr std::string_view getErrorName(const Error error) {
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

	FASTGLTF_EXPORT constexpr std::string_view getErrorMessage(const Error error) {
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
	 * A type that stores an error together with an expected value.
	 * To use this type, first call error() to inspect if any errors have occurred.
	 * If error() is not fastgltf::Error::None,
	 * calling get(), operator->(), and operator*() is undefined behaviour.
	 */
	template <typename T>
	class Expected {
		static_assert(std::is_default_constructible_v<T>);
		static_assert(!std::is_same_v<Error, T>);

		Error err;
		T value;

	public:
		Expected(Error error) : err(error) {}
		Expected(T&& value) : err(Error::None), value(std::forward<T>(value)) {}

		Expected(const Expected& other) = delete;
		Expected(Expected&& other) noexcept : err(other.err), value(std::move(other.value)) {}

		Expected& operator=(const Expected& other) = delete;
		Expected& operator=(Expected&& other) noexcept {
			err = other.err;
			value = std::move(other.value);
			return *this;
		}

		[[nodiscard]] Error error() const noexcept {
			return err;
		}

		/**
		 * Returns a reference to the value of T.
		 * When error() returns anything but Error::None, the returned value is undefined.
		 */
		[[nodiscard]] T& get() noexcept {
			assert(err == Error::None);
			return value;
		}

		/**
		 * Returns the address of the value of T, or nullptr if error() returns anything but Error::None.
		 */
		[[nodiscard]] T* get_if() noexcept {
			if (err != Error::None)
				return nullptr;
			return std::addressof(value);
		}

		template <std::size_t I>
		[[nodiscard]] auto& get() noexcept {
			if constexpr (I == 0) return err;
			else if constexpr (I == 1) return value;
		}

		template <std::size_t I>
		[[nodiscard]] const auto& get() const noexcept {
			if constexpr (I == 0) return err;
			else if constexpr (I == 1) return value;
		}

		/**
		 * Returns the address of the value of T.
		 * When error() returns anything but Error::None, the returned value is undefined.
		 */
		[[nodiscard]] T* operator->() noexcept {
			assert(err == Error::None);
			return std::addressof(value);
		}

		/**
		 * Returns the address of the const value of T.
		 * When error() returns anything but Error::None, the returned value is undefined.
		 */
		[[nodiscard]] const T* operator->() const noexcept {
			assert(err == Error::None);
			return std::addressof(value);
		}

		[[nodiscard]] T&& operator*() && noexcept {
			assert(err == Error::None);
			return std::move(value);
		}

		[[nodiscard]] operator bool() const noexcept {
			return err == Error::None;
		}
	};
}

namespace std {
	template <typename T>
	struct tuple_size<fastgltf::Expected<T>> : std::integral_constant<std::size_t, 2> {};

	template <typename T>
	struct tuple_element<0, fastgltf::Expected<T>> { using type = fastgltf::Error; };
	template <typename T>
	struct tuple_element<1, fastgltf::Expected<T>> { using type = T; };
} // namespace std

#endif
