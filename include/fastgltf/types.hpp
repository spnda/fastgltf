/*
 * Copyright (C) 2022 - 2025 Sean Apeler
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

#ifndef FASTGLTF_TYPES_HPP
#define FASTGLTF_TYPES_HPP

#if !defined(FASTGLTF_USE_STD_MODULE) || !FASTGLTF_USE_STD_MODULE
#include <cassert>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#endif

// Utils header already includes some headers, which we'll try and avoid including twice.
#include <fastgltf/util.hpp>
#include <fastgltf/math.hpp>

#if defined(_GLIBCXX_USE_CXX11_ABI) && !_GLIBCXX_USE_CXX11_ABI
// polymorphic allocators are only supported with the 'new' GCC ABI.
// Older compilers (older than GCC 5.1) default to the old ABI and sometimes the old ABI is
// explicitly selected on even the newest compilers, which we want to support.
#define FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL 1
#endif

#ifndef FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL
#define FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL 0
#endif

#if __has_include(<memory_resource>)
#define FASTGLTF_MISSING_MEMORY_RESOURCE 0
#else
#define FASTGLTF_MISSING_MEMORY_RESOURCE 1
#if defined(FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL)
#undef FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL
#endif
#define FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL 1
#endif

#if !FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL
#if !defined(FASTGLTF_USE_STD_MODULE) || !FASTGLTF_USE_STD_MODULE
#include <memory_resource>
#endif
#endif

#if FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL
#define FASTGLTF_STD_PMR_NS ::std
#define FASTGLTF_FG_PMR_NS ::fastgltf

#define FASTGLTF_CONSTRUCT_PMR_RESOURCE(type, memoryResource, ...) type(__VA_ARGS__)
#define FASTGLTF_IF_PMR(expr)
#else
#define FASTGLTF_STD_PMR_NS ::std::pmr
#define FASTGLTF_FG_PMR_NS ::fastgltf::pmr

#define FASTGLTF_CONSTRUCT_PMR_RESOURCE(type, memoryResource, ...) type(__VA_ARGS__, memoryResource)
#define FASTGLTF_IF_PMR(expr) expr
#endif

#if FASTGLTF_CPP_20
#if !defined(FASTGLTF_USE_STD_MODULE) || !FASTGLTF_USE_STD_MODULE
#include <span>
#endif
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5030) // attribute 'x' is not recognized
#pragma warning(disable : 4514) // unreferenced inline function has been removed
#endif

#define FASTGLTF_QUOTE_Q(x) #x
#define FASTGLTF_QUOTE(x) FASTGLTF_QUOTE_Q(x)

// fastgltf version string. Use FASTGLTF_QUOTE to stringify.
#define FASTGLTF_VERSION 0.8.0

// FASTGLTF_REQUIRE is a macro that is used to assert that a condition is true in tests.
#if defined(REQUIRE) // if catch2 is used, use its REQUIRE macro
#define FASTGLTF_REQUIRE(x) REQUIRE(x)
#else // otherwise, just return false
#define FASTGLTF_REQUIRE(x)                                                    \
  if (!(x)) {                                                                  \
    return false;                                                              \
  }
#endif

namespace fastgltf {
#if defined(FASTGLTF_USE_64BIT_FLOAT) && FASTGLTF_USE_64BIT_FLOAT
	using num = double;
#else
	using num = float;
#endif

	namespace math {
		FASTGLTF_EXPORT using nvec2 = math::vec<num, 2>;
		FASTGLTF_EXPORT using nvec3 = math::vec<num, 3>;
		FASTGLTF_EXPORT using nvec4 = math::vec<num, 4>;
	}

#pragma region Enums
    // clang-format off
    FASTGLTF_EXPORT enum class PrimitiveType : std::uint8_t {
        Points = 0,
        Lines = 1,
        LineLoop = 2,
        LineStrip = 3,
        Triangles = 4,
        TriangleStrip = 5,
        TriangleFan = 6,
    };

    /**
     * Represents the type of element in the buffer pointed to by the accessor.
     *
     * We encode these values with the number of components in their top 8 bits for fast
     * access & storage. Therefore, use the fastgltf::getNumComponents and fastgltf::getElementByteSize
     * functions to extract data from this enum.
     *
     * 0aaaaabbb, where a is the component num, and b is the type index.
     */
    FASTGLTF_EXPORT enum class AccessorType : std::uint8_t {
        Invalid = 0,
        Scalar  = ( 1 << 3) | 1,
        Vec2    = ( 2 << 3) | 2,
        Vec3    = ( 3 << 3) | 3,
        Vec4    = ( 4 << 3) | 4,
        Mat2    = ( 4 << 3) | 5,
        Mat3    = ( 9 << 3) | 6,
        Mat4    = (16 << 3) | 7,
    };

    /**
     * Represents the various types of components an accessor could point at. This describes the
     * format each component of the structure, which in return is described by fastgltf::AccessorType, is in.
     *
     * As the constants used to identify component type in glTF fit within 13-bits, we store them in the lower 13 bits.
     * The remaining three bits are then used to store the byte width of the type, minus 1, as 8 is not representable
     * with just three bits.
     *
     * To get the byte or bit size of a component, use the fastgltf::getComponentByteSize or fastgltf::getComponentBitSize,
     * respectively. To get the OpenGL constant for the component type, use fastgltf::getGLComponentType.
     *
     * aaabbbbbbbbbbbbb, where a is the byte size, and b the OpenGL type enumeration.
     */
    FASTGLTF_EXPORT enum class ComponentType : std::uint16_t {
        Invalid         = 0,
        Byte            = (0 << 13) | 5120,
        UnsignedByte    = (0 << 13) | 5121,
        Short           = (1 << 13) | 5122,
        UnsignedShort   = (1 << 13) | 5123,
        /**
         * Signed integers are not officially allowed by the glTF spec, but are placed here for
         * the sake of completeness.
         */
        Int             = (3 << 13) | 5124,
        UnsignedInt     = (3 << 13) | 5125,
        Float           = (3 << 13) | 5126,
        /**
         * Doubles are only allowed through the KHR_accessor_float64 extension, but can also
         * be enabled by specifying the AllowDouble option.
         */
        Double          = (7 << 13) | 5130,
    };

    FASTGLTF_EXPORT enum class Filter : std::uint16_t {
        Nearest = 9728,
        Linear = 9729,
        NearestMipMapNearest = 9984,
        LinearMipMapNearest = 9985,
        NearestMipMapLinear = 9986,
        LinearMipMapLinear = 9987,
    };

    FASTGLTF_EXPORT enum class Wrap : std::uint16_t {
        ClampToEdge = 33071,
        MirroredRepeat = 33648,
        Repeat = 10497,
    };

    /**
     * Represents the intended OpenGL GPU buffer type to use with this buffer view.
     */
    FASTGLTF_EXPORT enum class BufferTarget : std::uint16_t {
        ArrayBuffer = 34962,
        ElementArrayBuffer = 34963,
    };

    FASTGLTF_EXPORT enum class MimeType : std::uint8_t {
        None = 0,
        JPEG = 1,
        PNG = 2,
        KTX2 = 3,
        DDS = 4,
        GltfBuffer = 5,
        OctetStream = 6,
        WEBP = 7,
    };

    FASTGLTF_EXPORT enum class AnimationInterpolation : std::uint8_t {
        /**
         * The animated values are linearly interpolated between keyframes. When targeting a
         * rotation, spherical linear interpolation (slerp) SHOULD be used to interpolate quaternions.
         */
        Linear = 0,
        /**
         * The animated values remain constant to the output of the first keyframe, until the next
         * keyframe.
         */
        Step = 1,
        /**
         * The animation’s interpolation is computed using a cubic spline with specified tangents.
         * The number of output elements MUST equal three times the number of input elements. For
         * each input element, the output stores three elements, an in-tangent, a spline vertex,
         * and an out-tangent.
         */
        CubicSpline = 2,
    };

    FASTGLTF_EXPORT enum class AnimationPath : std::uint8_t {
        /**
         * The values are the translation along the X, Y, and Z axes.
         */
        Translation = 1,
        /**
         * The values are a quaternion in the order x, y, z, w where w is the scalar.
         */
        Rotation = 2,
        /**
         * The values are scaling factors along the X, Y, and Z axes.
         */
        Scale = 3,
        Weights = 4,
    };

    FASTGLTF_EXPORT enum class AlphaMode : std::uint8_t {
        Opaque,
        Mask,
        Blend,
    };

    FASTGLTF_EXPORT enum class MeshoptCompressionMode : std::uint8_t {
        Attributes,
        Triangles,
        Indices,
    };

    FASTGLTF_EXPORT enum class MeshoptCompressionFilter : std::uint8_t {
        None = 0,
        Octahedral,
        Quaternion,
        Exponential,
    };

    FASTGLTF_EXPORT enum class LightType : std::uint8_t {
        Directional,
        Spot,
        Point,
    };

    FASTGLTF_EXPORT enum class Category : std::uint32_t {
        None        = 0,
        Buffers     = 1 <<  0,
        BufferViews = 1 <<  1,
        Accessors   = 1 <<  2,
        Images      = 1 <<  3,
        Samplers    = 1 <<  4,
        Textures    = 1 <<  5,
        Animations  = 1 <<  6,
        Cameras     = 1 <<  7,
        Materials   = 1 <<  8,
        Meshes      = 1 <<  9,
        Skins       = 1 << 10,
        Nodes       = 1 << 11,
        Scenes      = 1 << 12,
        Asset       = 1 << 13,

#if FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES
		Shapes		= 1 << 14,
#endif

#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
		PhysicsMaterials	= 1 << 15,
		CollisionFilters	= 1 << 16,
		PhysicsJoints		= 1 << 17,
#endif

		All = ~0u,

        // Includes everything needed for rendering but animations
        OnlyRenderable = All & ~(Animations) & ~(Skins)
#if FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES
		& ~(Shapes)
#endif
#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
		& ~(PhysicsMaterials) & ~(CollisionFilters) & ~(PhysicsJoints)
#endif
        ,
        OnlyAnimations = Animations | Accessors | BufferViews | Buffers,
    };

    FASTGLTF_ARITHMETIC_OP_TEMPLATE_MACRO(Category, Category, |)
    FASTGLTF_ARITHMETIC_OP_TEMPLATE_MACRO(Category, Category, &)
    FASTGLTF_ASSIGNMENT_OP_TEMPLATE_MACRO(Category, Category, |)
    FASTGLTF_ASSIGNMENT_OP_TEMPLATE_MACRO(Category, Category, &)
    FASTGLTF_UNARY_OP_TEMPLATE_MACRO(Category, ~)
    // clang-format on

#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
	FASTGLTF_EXPORT enum class CombineMode : std::uint8_t {
        Average,
		Minimum,
		Maximum,
		Multiply,
		Invalid,
    };

	FASTGLTF_EXPORT enum class DriveType : std::uint8_t {
	    Linear,
		Angular,
		Invalid
	};

	FASTGLTF_EXPORT enum class DriveMode : std::uint8_t {
	    Force,
		Acceleration,
	    Invalid
	};
#endif
#pragma endregion

#pragma region ConversionFunctions
    /**
     * Gets the number of components for each element for the given accessor type. For example, with
     * a Vec3 accessor type this will return 3, as a Vec3 contains 3 components.
     */
    FASTGLTF_EXPORT constexpr auto getNumComponents(AccessorType type) noexcept {
    	static_assert(std::is_same_v<std::underlying_type_t<AccessorType>, std::uint8_t>);
        return static_cast<std::size_t>(to_underlying(type) >> 3U);
    }

    /**
     * Returns the number of rows in the given accessor type.
     */
    FASTGLTF_EXPORT constexpr auto getElementRowCount(AccessorType type) noexcept {
        switch (type) {
            case AccessorType::Mat2:
            case AccessorType::Vec2:
                return std::size_t(2U);
            case AccessorType::Mat3:
            case AccessorType::Vec3:
                return std::size_t(3U);
            case AccessorType::Mat4:
            case AccessorType::Vec4:
                return std::size_t(4U);
            default:
                return std::size_t(1U);
        }
    }

    FASTGLTF_EXPORT constexpr bool isMatrix(AccessorType type) noexcept {
        return type == AccessorType::Mat2 || type == AccessorType::Mat3 || type == AccessorType::Mat4;
    }

	FASTGLTF_EXPORT constexpr auto getComponentByteSize(ComponentType componentType) noexcept {
		static_assert(std::is_same_v<std::underlying_type_t<ComponentType>, std::uint16_t>);
		if (componentType == ComponentType::Invalid)
			return std::size_t(0U);
		return static_cast<std::size_t>(to_underlying(componentType) >> 13U) + 1;
	}

	FASTGLTF_EXPORT constexpr auto getComponentBitSize(ComponentType componentType) noexcept {
		return getComponentByteSize(componentType) * 8U;
	}

    FASTGLTF_EXPORT constexpr auto getElementByteSize(AccessorType type, ComponentType componentType) noexcept {
        const auto componentSize = getComponentByteSize(componentType);
        auto numComponents = getNumComponents(type);
        const auto rowCount = getElementRowCount(type);
        if (isMatrix(type) && (rowCount * componentSize) % 4 != 0) {
            // Matrices need extra padding per-column which affects their size.
            numComponents += rowCount * (4 - (rowCount % 4));
        }
        return numComponents * componentSize;
    }

    /**
     * Returns the OpenGL component type enumeration for the given component type.
     * All OpenGL enumerations use GLenum, which is a 32-bit wide integer, which is why
     * this function returns a std::uint32_t.
     *
     * For example, getGLComponentType(ComponentType::Float) will return GL_FLOAT (0x1406).
     */
    FASTGLTF_EXPORT constexpr auto getGLComponentType(ComponentType type) noexcept {
    	static_assert(std::is_same_v<std::underlying_type_t<ComponentType>, std::uint16_t>);
        return static_cast<std::uint32_t>(to_underlying(type) & 0x1FFF); // 2^13 - 1 in hex, to mask the lower 13 bits.
    }

    /**
     * Don't use this, use getComponentType instead.
     * This order matters as we assume that their glTF constant is ascending to index it.
     */
    static constexpr std::array<ComponentType, 11> components = {
        ComponentType::Byte,
        ComponentType::UnsignedByte,
        ComponentType::Short,
        ComponentType::UnsignedShort,
        ComponentType::Int,
        ComponentType::UnsignedInt,
        ComponentType::Float,
        ComponentType::Invalid,
        ComponentType::Invalid,
        ComponentType::Invalid,
        ComponentType::Double,
    };

    constexpr auto getComponentType(std::underlying_type_t<ComponentType> componentType) noexcept {
        const auto index = static_cast<std::size_t>(componentType - getGLComponentType(ComponentType::Byte));
        if (index >= components.size()) {
            return ComponentType::Invalid;
		}
        return components[index];
    }

    // This order matters as we assume that their glTF constant is ascending to index it.
    static constexpr std::array<AccessorType, 7> accessorTypes = {
        AccessorType::Scalar,
        AccessorType::Vec2,
        AccessorType::Vec3,
        AccessorType::Vec4,
        AccessorType::Mat2,
        AccessorType::Mat3,
        AccessorType::Mat4,
    };

    /**
     * Gets the AccessorType by its string representation found in glTF files.
     */
    constexpr auto getAccessorType(std::string_view accessorTypeName) noexcept {
        assert(!accessorTypeName.empty());
        switch (accessorTypeName[0]) {
            case 'S': return AccessorType::Scalar;
            case 'V': {
                const auto componentCount = static_cast<std::size_t>(accessorTypeName[3] - '2');
                if (componentCount + 1 >= accessorTypes.size()) {
                    return AccessorType::Invalid;
                }
                return accessorTypes[componentCount + 1];
            }
            case 'M': {
                const auto componentCount = static_cast<std::size_t>(accessorTypeName[3] - '2');
                if (componentCount + 4 >= accessorTypes.size()) {
                    return AccessorType::Invalid;
                }
                return accessorTypes[componentCount + 4];
            }
        	default:
        		return AccessorType::Invalid;
        }
    }

	static constexpr std::array<std::string_view, 7> accessorTypeNames = {
		"SCALAR",
		"VEC2",
		"VEC3",
		"VEC4",
		"MAT2",
		"MAT3",
		"MAT4"
	};

	constexpr std::string_view getAccessorTypeName(AccessorType type) noexcept {
    	static_assert(std::is_same_v<std::underlying_type_t<AccessorType>, std::uint8_t>);
		if (type == AccessorType::Invalid)
			return "";
		auto idx = to_underlying(type) & 0x7;
		return accessorTypeNames[idx - 1];
	}

	constexpr std::string_view mimeTypeJpeg = "image/jpeg";
	constexpr std::string_view mimeTypePng = "image/png";
	constexpr std::string_view mimeTypeKtx = "image/ktx2";
	constexpr std::string_view mimeTypeDds = "image/vnd-ms.dds";
	constexpr std::string_view mimeTypeGltfBuffer = "application/gltf-buffer";
	constexpr std::string_view mimeTypeOctetStream = "application/octet-stream";
	constexpr std::string_view mimeTypeWebp = "image/webp";

	constexpr std::string_view getMimeTypeString(MimeType mimeType) noexcept {
		switch (mimeType) {
			case MimeType::JPEG:
				return mimeTypeJpeg;
			case MimeType::PNG:
				return mimeTypePng;
			case MimeType::KTX2:
				return mimeTypeKtx;
			case MimeType::DDS:
				return mimeTypeDds;
			case MimeType::GltfBuffer:
				return mimeTypeGltfBuffer;
			case MimeType::OctetStream:
				return mimeTypeOctetStream;
			case MimeType::WEBP:
				return mimeTypeWebp;
			default:
				return "";
		}
	}

#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
	[[nodiscard]] constexpr auto getCombineMode(const std::string_view name) noexcept {
		assert(!name.empty());
	    if(name[0] == 'a') {
			return CombineMode::Average;
	    }

		switch(name[1]) {
	    case 'i':
			return CombineMode::Minimum;

	    case 'a':
			return CombineMode::Maximum;

	    case 'u':
			return CombineMode::Multiply;
		}

		return CombineMode::Invalid;
	}

	static constexpr std::array<std::string_view, 4> frictionCombineNames{
        "average",
        "minimum",
        "maximum",
        "multiply"
	};

	[[nodiscard]] constexpr std::string_view getFrictionCombineName(const CombineMode frictionCombine) noexcept {
		static_assert(std::is_same_v<std::underlying_type_t<CombineMode>, std::uint8_t>);
		const auto idx = to_underlying(frictionCombine) & 0x3;
		return frictionCombineNames[idx];
	}

	[[nodiscard]] constexpr auto getDriveType(const std::string_view name) noexcept {
		if (name[0] == 'l') {
			return DriveType::Linear;
		} else if (name[0] == 'a') {
			return DriveType::Angular;
		} else {
			return DriveType::Invalid;
		}
	}

	[[nodiscard]] constexpr auto getDriveMode(const std::string_view name) noexcept {
	    if (name[0] == 'f') {
			return DriveMode::Force;
	    } else if (name[0] == 'a') {
			return DriveMode::Acceleration;
	    } else {
			return DriveMode::Invalid;
	    }
	}
#endif
#pragma endregion

#pragma region Containers
    /**
     * A static vector which cannot be resized freely. When constructed, the backing array is allocated once.
     */
    FASTGLTF_EXPORT template <typename T>
    class StaticVector final {
	public:
		using value_type = T;
		using size_type = std::size_t;
        using array_t = value_type[];

	private:
		size_type _size = 0;
        std::unique_ptr<array_t> _array;

		void copy(const T* first, size_type count, T* result) {
            if (count > 0) {
				if constexpr (std::is_trivially_copyable_v<T>) {
					std::memcpy(result, first, count * sizeof(T));
				} else {
					*result++ = *first;
					for (size_type i = 1; i < count; ++i) {
						*result++ = *++first;
					}
				}
            }
        }

    public:
		using reference = value_type&;
		using const_reference = const value_type&;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using iterator = pointer;
        using const_iterator = const_pointer;

        explicit StaticVector(std::size_t size) : _size(size), _array(std::move(std::unique_ptr<array_t>(new std::remove_extent_t<array_t>[size]))) {}
		explicit StaticVector(std::size_t size, const T& initialValue) : _size(size), _array(std::move(std::unique_ptr<array_t>(new std::remove_extent_t<array_t>[size]))) {
			for (auto& value : *this) {
				value = initialValue;
			}
		}

        StaticVector(const StaticVector& other) {
            if (other.size() == 0) {
                _array.reset();
                _size = 0;
            } else {
                _array.reset(new std::remove_extent_t<array_t>[other.size()]);
                _size = other.size();
				copy(other.begin(), _size, begin());
            }
        }

        StaticVector(StaticVector&& other) noexcept {
            _array = std::move(other._array);
            _size = other.size();
        }

		StaticVector& operator=(StaticVector&& other) noexcept {
			_array = std::move(other._array);
			_size = other.size();
			return *this;
		}

		/**
		 * Copies the contents of the given vector into a new StaticVector.
		 */
		static StaticVector<T> fromVector(std::vector<T> vector) {
			StaticVector<T> staticVector(vector.size());
			if constexpr (std::is_trivially_copyable_v<T>) {
				std::memcpy(staticVector.data(), vector.data(), vector.size());
			} else {
				for (auto it = vector.begin(); it != vector.end(); ++it) {
					staticVector[std::distance(vector.begin(), it)] = *it;
				}
			}
			return staticVector;
		}

        [[nodiscard]] pointer data() noexcept {
            return &_array.get()[0];
        }

        [[nodiscard]] const_pointer data() const noexcept {
            return &_array.get()[0];
        }

        [[nodiscard]] size_type size() const noexcept {
            return _size;
        }

        [[nodiscard]] size_type size_bytes() const noexcept {
            return _size * sizeof(value_type);
        }

        [[nodiscard]] bool empty() const noexcept {
            return _size == 0;
        }

        [[nodiscard]] iterator begin() noexcept { return data(); }
        [[nodiscard]] const_iterator begin() const noexcept { return data(); }
        [[nodiscard]] const_iterator cbegin() const noexcept { return data(); }
        [[nodiscard]] iterator end() noexcept { return begin() + size(); }
        [[nodiscard]] const_iterator end() const noexcept { return begin() + size(); }
        [[nodiscard]] const_iterator cend() const noexcept { return begin() + size(); }

		[[nodiscard]] T& operator[](std::size_t idx) {
            assert(idx < size());
            return begin()[idx];
        }
        [[nodiscard]] const T& operator[](std::size_t idx) const {
            assert(idx < size());
            return begin()[idx];
        }

        bool operator==(const StaticVector<value_type>& other) const {
        	FASTGLTF_REQUIRE(other.size() == size());
        	// FASTGLTF_REQUIRE(std::memcmp(data(), other.data(), size_bytes()) == 0);
			FASTGLTF_REQUIRE(std::equal(begin(), end(), other.begin()));
        	return true;
        }

        // This is mostly just here for compatibility and the tests
        bool operator==(const std::vector<value_type>& other) const {
            FASTGLTF_REQUIRE(other.size() == size());
			FASTGLTF_REQUIRE(std::equal(begin(), end(), other.begin()));
        	return true;
        }
    };

	/*
	 * The amount of items that the SmallVector can initially store in the storage
	 * allocated within the object itself.
	 */
	static constexpr auto initialSmallVectorStorage = 8;

    /**
     * A custom vector class for fastgltf, which can store up to N objects within itself.
     * This is useful for cases where the vector is expected to only ever hold a tiny amount of small objects,
     * such as a node's children.
     * SmallVector is also mostly conformant to C++17's std::vector, and can therefore be used as a drop-in replacement.
     * @note It is also available with polymorphic allocators in the fastgltf::pmr namespace.
     */
    FASTGLTF_EXPORT template <typename T, std::size_t N = initialSmallVectorStorage, typename Allocator = std::allocator<T>>
    class SmallVector final {
        static_assert(N != 0, "Cannot create a SmallVector with 0 initial capacity");

        alignas(T) std::array<std::byte, N * sizeof(T)> storage = {};

		Allocator allocator;

        T* _data;
        std::size_t _size = 0, _capacity = N;

        void copy(const T* first, std::size_t count, T* result) {
            if (count > 0) {
				if constexpr (std::is_trivially_copyable_v<T>) {
					std::memcpy(result, first, count * sizeof(T));
				} else {
					*result++ = *first;
					for (std::size_t i = 1; i < count; ++i) {
						*result++ = *++first;
					}
				}
            }
        }

    public:
		using iterator = T*;
		using const_iterator = const T*;

        SmallVector() : _data(reinterpret_cast<T*>(storage.data())) {}

		explicit SmallVector(const Allocator& allocator) noexcept : allocator(allocator), _data(reinterpret_cast<T*>(storage.data())) {}

        explicit SmallVector(std::size_t size, const Allocator& allocator = Allocator()) : allocator(allocator), _data(reinterpret_cast<T*>(storage.data())) {
            resize(size);
        }

        SmallVector(std::size_t size, const T& value, const Allocator& allocator = Allocator()) : allocator(allocator), _data(reinterpret_cast<T*>(storage.data())) {
            assign(size, value);
        }

        SmallVector(std::initializer_list<T> init, const Allocator& allocator = Allocator()) : allocator(allocator), _data(reinterpret_cast<T*>(storage.data())) {
            assign(init);
        }

        SmallVector(const SmallVector& other) noexcept : _data(reinterpret_cast<T*>(storage.data())) {
            resize(other.size());
            copy(other.begin(), other.size(), begin());
        }

        SmallVector(SmallVector&& other) noexcept : _data(reinterpret_cast<T*>(storage.data())) {
            if (other.isUsingStack()) {
                if (!other.empty()) {
                    resize(other.size());
                    copy(other.begin(), other.size(), begin());
                    other._data = reinterpret_cast<T*>(other.storage.data()); // Reset pointer
                    _size = std::exchange(other._size, 0);
                    _capacity = std::exchange(other._capacity, N);
                }
            } else {
                _data = std::exchange(other._data, nullptr);
                _size = std::exchange(other._size, 0);
                _capacity = std::exchange(other._capacity, N);
            }
        }

        SmallVector& operator=(const SmallVector& other) {
            if (std::addressof(other) != this) {
                if (!isUsingStack() && _data) {
	                std::destroy(begin(), end());
					allocator.deallocate(_data, _capacity);
                    _data = reinterpret_cast<T*>(storage.data());
                    _size = _capacity = 0;
                }

                resize(other.size());
                copy(other.begin(), other.size(), begin());
            }
            return *this;
        }

        SmallVector& operator=(SmallVector&& other) noexcept {
            if (std::addressof(other) != this) {
                if (other.isUsingStack()) {
                    if (!other.empty()) {
                        resize(other.size());
                        copy(other.begin(), other.size(), begin());
                        other._data = reinterpret_cast<T*>(other.storage.data()); // Reset pointer
                        _size = std::exchange(other._size, 0);
                        _capacity = std::exchange(other._capacity, N);
                    }
                } else {
                    _data = std::exchange(other._data, nullptr);
                    _size = std::exchange(other._size, 0);
                    _capacity = std::exchange(other._capacity, N);
                }
            }
            return *this;
        }

        ~SmallVector() {
			// As we use an array of std::byte for the stack storage, we have to destruct those manually too.
			std::destroy(begin(), end());

            if (!isUsingStack() && _data) {
                // Not using the stack, we'll have to free.
	            allocator.deallocate(_data, _capacity);
            }
        }

	    [[nodiscard]] iterator begin() noexcept { return _data; }
        [[nodiscard]] const_iterator begin() const noexcept { return _data; }
	    [[nodiscard]] const_iterator cbegin() const noexcept { return _data; }
	    [[nodiscard]] iterator end() noexcept { return begin() + size(); }
	    [[nodiscard]] const_iterator end() const noexcept { return begin() + size(); }
	    [[nodiscard]] const_iterator cend() const noexcept { return begin() + size(); }

        [[nodiscard]] std::reverse_iterator<T*> rbegin() { return end(); }
        [[nodiscard]] std::reverse_iterator<const T*> rbegin() const { return end(); }
        [[nodiscard]] std::reverse_iterator<const T*> crbegin() const { return end(); }
        [[nodiscard]] std::reverse_iterator<T*> rend() { return begin(); }
        [[nodiscard]] std::reverse_iterator<const T*> rend() const { return begin(); }
        [[nodiscard]] std::reverse_iterator<const T*> crend() const { return begin(); }

        [[nodiscard]] T* data() noexcept { return _data; }
        [[nodiscard]] const T* data() const noexcept { return _data; }
        [[nodiscard]] std::size_t size() const noexcept { return _size; }
        [[nodiscard]] std::size_t size_in_bytes() const noexcept { return _size * sizeof(T); }
        [[nodiscard]] std::size_t capacity() const noexcept { return _capacity; }

        [[nodiscard]] bool empty() const noexcept { return _size == 0; }
        [[nodiscard]] bool isUsingStack() const noexcept { return data() == reinterpret_cast<const T*>(storage.data()); }

        void reserve(std::size_t newCapacity) {
	        static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>, "T needs to be copy constructible.");

            // We don't want to reduce capacity with reserve, only with shrink_to_fit.
            if (newCapacity <= capacity()) {
                return;
            }

            // If the new capacity is lower than what we can hold on the stack, we ignore this request.
            if (newCapacity <= N && isUsingStack()) {
                _capacity = newCapacity;
                if (_size > _capacity) {
                    _size = _capacity;
                }
                return;
            }

            // We use geometric growth, similarly to std::vector.
            newCapacity = std::size_t(1) << (std::numeric_limits<decltype(newCapacity)>::digits - clz(newCapacity));

			T* alloc = allocator.allocate(newCapacity);

			// Copy/Move the old data into the new memory
			for (std::size_t i = 0; i < size(); ++i) {
				auto& x = (*this)[i];
				if constexpr (std::is_nothrow_move_constructible_v<T>) {
					new(alloc + i) T(std::move(x));
				} else if constexpr (std::is_copy_constructible_v<T>) {
					new(alloc + i) T(x);
				} else {
					new(alloc + i) T(std::move(x));
				}
			}

			// Destroy all objects in the old allocation
			std::destroy(begin(), end());

            if (!isUsingStack() && _data && size() != 0) {
				allocator.deallocate(_data, _capacity);
            }

            _data = alloc;
            _capacity = newCapacity;
        }

        void resize(std::size_t newSize) {
			static_assert(std::is_constructible_v<T>, "T has to be constructible");
			if (newSize == size()) {
				return;
			}

			if (newSize < size()) {
				// Just destroy the "overflowing" elements.
				std::destroy(begin() + newSize, end());
			} else {
                // Reserve enough capacity and copy the new value over.
                auto oldSize = _size;
                reserve(newSize);
                for (auto it = begin() + oldSize; it != begin() + newSize; ++it) {
                    new (it) T();
                }
            }

            _size = newSize;
        }

		void resize(std::size_t newSize, const T& value) {
			static_assert(std::is_copy_constructible_v<T>, "T needs to be copy constructible.");
			if (newSize == size()) {
				return;
			}

			if (newSize < size()) {
				// Just destroy the "overflowing" elements.
				std::destroy(begin() + newSize, end());
			} else {
				// Reserve enough capacity and copy the new value over.
				auto oldSize = _size;
				reserve(newSize);
				for (auto it = begin() + oldSize; it != begin() + newSize; ++it) {
					if (it == nullptr)
						break;

					if constexpr (std::is_move_constructible_v<T>) {
						new (it) T(std::move(value));
					} else if constexpr (std::is_trivially_copyable_v<T>) {
						std::memcpy(it, std::addressof(value), sizeof(T));
					} else {
						new (it) T(value);
					}
				}
			}

			_size = newSize;
		}

		void shrink_to_fit() {
			// Only have to shrink if there's any unused capacity.
			if (capacity() == size() || size() == 0) {
				return;
			}

			// If we can use the object's memory again, we'll copy everything over.
			if (size() <= N) {
				copy(begin(), size(), reinterpret_cast<T*>(storage.data()));
				_data = reinterpret_cast<T*>(storage.data());
			} else {
				// We have to use heap allocated memory.
				auto* alloc = allocator.allocate(size());
				for (std::size_t i = 0; i < size(); ++i) {
					new(alloc + i) T((*this)[i]);
				}

				if (_data && !isUsingStack()) {
					std::destroy(begin(), end());
					allocator.deallocate(_data, _capacity);
				}

				_data = alloc;
			}

			_capacity = _size;
		}

		void assign(std::size_t count, const T& value) {
			clear();
			resize(count, value);
		}

		void assign(std::initializer_list<T> init) {
			static_assert(std::is_trivially_copyable_v<T> || std::is_copy_constructible_v<T>, "T needs to be trivially copyable or be copy constructible");
			clear();
			reserve(init.size());
			_size = init.size();

			if constexpr (std::is_trivially_copyable_v<T>) {
				std::memcpy(begin(), init.begin(), init.size() * sizeof(T));
			} else if constexpr (std::is_copy_constructible_v<T>) {
				for (auto it = init.begin(); it != init.end(); ++it) {
					new (_data + std::distance(init.begin(), it)) T(*it);
				}
			}
		}

		void clear() noexcept {
			std::destroy(begin(), end());

			if (!isUsingStack() && size() != 0) {
				allocator.deallocate(_data, _capacity);
				_data = reinterpret_cast<T*>(storage.data());
			}

			_size = 0;
		}

        template <typename... Args>
        decltype(auto) emplace_back(Args&&... args) {
            // We reserve enough capacity for the new element, and then just increment the size.
			reserve(_size + 1);
			++_size;
            new (std::addressof(back())) T(std::forward<Args>(args)...);
            return (back());
        }

        [[nodiscard]] T& at(std::size_t idx) {
            if (idx >= size()) {
                raise<std::out_of_range>("Index is out of range for SmallVector");
            }
            return begin()[idx];
        }
        [[nodiscard]] const T& at(std::size_t idx) const {
            if (idx >= size()) {
                raise<std::out_of_range>("Index is out of range for SmallVector");
            }
            return begin()[idx];
        }

        [[nodiscard]] T& operator[](std::size_t idx) {
            assert(idx < size());
            return begin()[idx];
        }
        [[nodiscard]] const T& operator[](std::size_t idx) const {
            assert(idx < size());
            return begin()[idx];
        }

        [[nodiscard]] T& front() {
            assert(!empty());
            return begin()[0];
        }
        [[nodiscard]] const T& front() const {
            assert(!empty());
            return begin()[0];
        }

        [[nodiscard]] T& back() {
            assert(!empty());
            return end()[-1];
        }
        [[nodiscard]] const T& back() const {
            assert(!empty());
            return end()[-1];
        }

		bool operator==(const SmallVector& other) const {
			FASTGLTF_REQUIRE(size() == other.size());
			FASTGLTF_REQUIRE(std::equal(begin(), end(), other.begin(), other.end()));
			return true;
		}
    };

#if !FASTGLTF_MISSING_MEMORY_RESOURCE
	namespace pmr {
		FASTGLTF_EXPORT template<typename T, std::size_t N>
		using SmallVector = SmallVector<T, N, std::pmr::polymorphic_allocator<T>>;
	} // namespace pmr
#endif

#ifndef FASTGLTF_USE_CUSTOM_SMALLVECTOR
#define FASTGLTF_USE_CUSTOM_SMALLVECTOR 0
#endif

#if FASTGLTF_USE_CUSTOM_SMALLVECTOR
	FASTGLTF_EXPORT template <typename T, std::size_t N = initialSmallVectorStorage>
	using MaybeSmallVector = SmallVector<T, N>;
#else
	FASTGLTF_EXPORT template <typename T, std::size_t N = 0>
	using MaybeSmallVector = std::vector<T>;
#endif

#if !FASTGLTF_MISSING_MEMORY_RESOURCE
	namespace pmr {
#if FASTGLTF_USE_CUSTOM_SMALLVECTOR
		FASTGLTF_EXPORT template <typename T, std::size_t N = initialSmallVectorStorage>
		using MaybeSmallVector = pmr::SmallVector<T, N>;
#else
		FASTGLTF_EXPORT template <typename T, std::size_t N = 0>
		using MaybeSmallVector = std::pmr::vector<T>;
#endif
	} // namespace pmr
#endif

	FASTGLTF_EXPORT template<typename, typename = void>
	struct OptionalFlagValue {
		static constexpr std::nullopt_t missing_value = std::nullopt;
	};

	template<>
	struct OptionalFlagValue<std::size_t> {
		static constexpr auto missing_value = std::numeric_limits<std::size_t>::max();
	};

	template<>
	struct OptionalFlagValue<float, std::enable_if_t<std::numeric_limits<float>::is_iec559>> {
		// This float is a quiet NaN with a specific bit pattern to be able to differentiate
		// between this flag value and any result from FP operations.
		static constexpr auto missing_value = static_cast<float>(0x7fedb6db);
	};

	template<>
	struct OptionalFlagValue<double, std::enable_if_t<std::numeric_limits<double>::is_iec559>> {
		static constexpr auto missing_value = static_cast<double>(0x7ffdb6db6db6db6d);
	};

	template<>
	struct OptionalFlagValue<Filter> {
		static constexpr auto missing_value = static_cast<Filter>(std::numeric_limits<std::underlying_type_t<Filter>>::max());
	};

	template<>
	struct OptionalFlagValue<BufferTarget> {
		static constexpr auto missing_value = static_cast<BufferTarget>(std::numeric_limits<std::underlying_type_t<BufferTarget>>::max());
	};

	FASTGLTF_EXPORT template<typename T>
	class OptionalWithFlagValue;

	/**
	 * A type alias which checks if there is a specialization of OptionalFlagValue for T and "switches"
	 * between fastgltf::OptionalWithFlagValue and std::optional.
	 */
	FASTGLTF_EXPORT template <typename T>
	using Optional = std::conditional_t<
		!std::is_same_v<std::nullopt_t, std::remove_const_t<decltype(OptionalFlagValue<T>::missing_value)>>,
		OptionalWithFlagValue<T>,
		std::optional<T>>;

	/**
	 * A custom optional class for fastgltf,
	 * which uses so-called "flag values" which are specific values of T that will never be present as an actual value.
	 * We can therefore use those values as flags for whether there is an actual value stored,
	 * instead of the additional bool used by std::optional.
	 *
	 * These flag values are obtained from the specializations of OptionalFlagValue.
	 * If no specialization for T of OptionalFlagValue is provided, a static assert will be triggered.
	 * In those cases, use std::optional or fastgltf::Optional instead.
	 */
	template<typename T>
	class OptionalWithFlagValue final {
		static_assert(!std::is_same_v<std::nullopt_t, std::remove_const_t<decltype(OptionalFlagValue<T>::missing_value)>>,
			"OptionalWithFlagValue can only be used when there is an appropriate specialization of OptionalFlagValue<T>.");

		struct NonTrivialDummy {
			constexpr NonTrivialDummy() noexcept {}
		};

		union {
			NonTrivialDummy dummy;
			std::remove_const_t<T> _value;
		};

	public:
		OptionalWithFlagValue() noexcept {
			reset();
		}

		OptionalWithFlagValue(std::nullopt_t) noexcept {
			reset();
		}

		template <typename U = T, std::enable_if_t<std::is_copy_constructible_v<T>, int> = 0>
		OptionalWithFlagValue(const OptionalWithFlagValue<U>& other) {
			if (other.has_value()) {
				new (std::addressof(_value)) T(*other);
			} else {
				reset();
			}
		}

		template <typename U = T, std::enable_if_t<std::is_move_constructible_v<T>, int> = 0>
		OptionalWithFlagValue(OptionalWithFlagValue<U>&& other) {
			if (other.has_value()) {
				new (std::addressof(_value)) T(std::move(*other));
			} else {
				reset();
			}
		}

		template<typename... Args, std::enable_if_t<std::is_constructible_v<T, Args...>, int> = 0>
		explicit OptionalWithFlagValue(std::in_place_t, Args&& ... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
			: _value(std::forward<Args>(args)...) {}

		template <typename U = T, std::enable_if_t<std::is_constructible_v<T, U&&>, int> = 0>
		OptionalWithFlagValue(U&& _new) noexcept(std::is_nothrow_assignable_v<T&, U> && std::is_nothrow_constructible_v<T, U>) {
			new (std::addressof(_value)) T(std::forward<U>(_new));
		}

		~OptionalWithFlagValue() {
			reset();
		}

		OptionalWithFlagValue& operator=(std::nullopt_t) noexcept {
			reset();
			return *this;
		}

		template<typename U = T, std::enable_if_t<std::is_constructible_v<T, U&&>, int> = 0>
		OptionalWithFlagValue& operator=(U&& _new) noexcept(std::is_nothrow_assignable_v<T&, U> && std::is_nothrow_constructible_v<T, U>) {
			if (has_value()) {
				_value = std::forward<U>(_new);
			} else {
				new(std::addressof(_value)) T(std::forward<U>(_new));
			}
			return *this;
		}

		template <typename U, std::enable_if_t<std::conjunction_v<std::is_constructible<T, const U&>, std::is_assignable<T&, const U&>>, int> = 0>
		OptionalWithFlagValue& operator=(const OptionalWithFlagValue<U>& other) {
			if (other.has_value()) {
				if (has_value()) {
					_value = other._value;
				} else {
					new(std::addressof(_value)) T(other._value);
				}
			} else {
				reset();
			}
			return *this;
		}

		template <typename U, std::enable_if_t<std::conjunction_v<std::is_constructible<T, U>, std::is_assignable<T&, U>>, int> = 0>
		OptionalWithFlagValue& operator=(OptionalWithFlagValue<U>&& other) noexcept(std::is_nothrow_assignable_v<T&, T> && std::is_nothrow_constructible_v<T, T>) {
			if (other.has_value()) {
				if (has_value()) {
					_value = std::move(other._value);
				} else {
					new(std::addressof(_value)) T(other._value);
				}
			} else {
				reset();
			}
			return *this;
		}

		[[nodiscard]] bool has_value() const {
			return this->_value != OptionalFlagValue<T>::missing_value;
		}

		[[nodiscard]] T& value()& {
			if (!has_value()) {
                raise<std::bad_optional_access>();
			}
			return _value;
		}

		[[nodiscard]] const T& value() const& {
			if (!has_value()) {
				raise<std::bad_optional_access>();
			}
			return _value;
		}

		[[nodiscard]] T&& value()&& {
			if (!has_value()) {
				raise<std::bad_optional_access>();
			}
			return std::move(_value);
		}

		[[nodiscard]] const T&& value() const&& {
			if (!has_value()) {
				raise<std::bad_optional_access>();
			}
			return std::move(_value);
		}

		template<typename U>
		[[nodiscard]] T value_or(U&& default_value) const& {
			return has_value() ? **this : static_cast<T>(std::forward<U>(default_value));
		}

		template<typename U>
		[[nodiscard]] T value_or(U&& default_value)&& {
			return has_value() ? std::move(**this) : static_cast<T>(std::forward<U>(default_value));
		}

		template <typename F>
		[[nodiscard]] auto and_then(F&& func)& {
			using U = remove_cvref_t<std::invoke_result_t<F, T&>>;
			if (!has_value())
				return U(std::nullopt);
			return std::invoke(std::forward<F>(func), **this);
		}

		template <typename F>
		[[nodiscard]] auto and_then(F&& func) const& {
			using U = remove_cvref_t<std::invoke_result_t<F, const T&>>;
			if (!has_value())
				return U(std::nullopt);
			return std::invoke(std::forward<F>(func), **this);
		}

		template <typename F>
		[[nodiscard]] auto and_then(F&& func)&& {
			using U = remove_cvref_t<std::invoke_result_t<F, T>>;
			if (!has_value())
				return U(std::nullopt);
			return std::invoke(std::forward<F>(func), std::move(**this));
		}

		template <typename F>
		[[nodiscard]] auto and_then(F&& func) const&& {
			using U = remove_cvref_t<std::invoke_result_t<F, const T>>;
			if (!has_value())
				return U(std::nullopt);
			return std::invoke(std::forward<F>(func), std::move(**this));
		}

		template <typename F>
		[[nodiscard]] auto transform(F&& func)& {
			using U = std::remove_cv_t<std::invoke_result_t<F, T&>>;
			if (!has_value())
				return Optional<U>(std::nullopt);
			return Optional<U>(std::invoke(std::forward<F>(func), **this));
		}

		template <typename F>
		[[nodiscard]] auto transform(F&& func) const& {
			using U = std::remove_cv_t<std::invoke_result_t<F, const T&>>;
			if (!has_value())
				return Optional<U>(std::nullopt);
			return Optional<U>(std::invoke(std::forward<F>(func), **this));
		}

		template <typename F>
		[[nodiscard]] auto transform(F&& func)&& {
			using U = std::remove_cv_t<std::invoke_result_t<F, T>>;
			if (!has_value())
				return Optional<U>(std::nullopt);
			return Optional<U>(std::invoke(std::forward<F>(func), std::move(**this)));
		}

		template <typename F>
		[[nodiscard]] auto transform(F&& func) const&& {
			using U = std::remove_cv_t<std::invoke_result_t<F, const T>>;
			if (!has_value())
				return Optional<U>(std::nullopt);
			return Optional<U>(std::invoke(std::forward<F>(func), std::move(**this)));
		}

		template <typename F>
		[[nodiscard]] T or_else(F&& func) const& {
			return *this ? *this : std::invoke(std::forward<F>(func));
		}

		template <typename F>
		[[nodiscard]] T or_else(F&& func)&& {
			return *this ? std::move(*this) : std::invoke(std::forward<F>(func));
		}

		void swap(OptionalWithFlagValue<T>& other) noexcept(std::is_nothrow_move_constructible_v<T> &&
		                                                    std::is_nothrow_swappable_v<T>) {
			static_assert(std::is_move_constructible_v<T>);
			if (has_value() && other.has_value()) {
				std::swap(_value, other._value);
			} else if (has_value() && !other.has_value()) {
				other._value = std::move(_value);
			} else if (!has_value() && other.has_value()) {
				_value = std::move(other._value);
			}
		}

		void reset() noexcept {
			this->_value = OptionalFlagValue<T>::missing_value;
		}

		template <typename... Args>
		T& emplace(Args&&... args) {
			new (std::addressof(_value)) T(std::forward<Args>(args)...);
			return _value;
		}

		template <typename U, typename... Args>
		T& emplace(std::initializer_list<U> list, Args&&... args) {
			static_assert(std::is_constructible_v<T, std::initializer_list<U>&, Args&&...>);
			new (std::addressof(_value)) T(list, std::forward<Args>(args)...);
			return _value;
		}

		explicit operator bool() const noexcept {
			return has_value();
		}

		T* operator->() noexcept {
			return std::addressof(_value);
		}

		const T* operator->() const noexcept {
			return std::addressof(_value);
		}

		T& operator*()& noexcept {
			return _value;
		}

		const T& operator*() const& noexcept {
			return _value;
		}

		T&& operator*()&& noexcept {
			return std::move(_value);
		}

		const T&& operator*() const&& noexcept {
			return std::move(_value);
		}

		operator std::optional<T>() const noexcept {
			return has_value() ? std::optional<T>(_value) : std::nullopt;
		}

		operator std::optional<T>&&()&& noexcept {
			return has_value() ? std::optional<T>(std::move(_value)) : std::nullopt;
		}
	};

	FASTGLTF_EXPORT template <typename T, typename U>
	bool operator==(const OptionalWithFlagValue<T>& lhs, const OptionalWithFlagValue<U>& rhs) {
		return bool(lhs) == bool(rhs) && (!bool(lhs) || *lhs == *rhs);
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	bool operator!=(const OptionalWithFlagValue<T>& lhs, const OptionalWithFlagValue<U>& rhs) {
		return !(lhs == rhs);
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	bool operator==(const OptionalWithFlagValue<T>& opt, const U& value) {
		return opt.has_value() && (*opt) == value;
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	bool operator!=(const OptionalWithFlagValue<T>& opt, const U& value) {
		return !(opt == value);
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	bool operator<(const OptionalWithFlagValue<T>& opt, const U& value) {
		return opt.has_value() && *opt < value;
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	bool operator<=(const OptionalWithFlagValue<T>& opt, const U& value) {
		return opt.has_value() && *opt <= value;
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	bool operator>(const OptionalWithFlagValue<T>& opt, const U& value) {
		return opt.has_value() && *opt > value;
	}

	FASTGLTF_EXPORT template <typename T, typename U>
	bool operator>=(const OptionalWithFlagValue<T>& opt, const U& value) {
		return opt.has_value() && *opt >= value;
	}

#pragma endregion

#pragma region Structs
	FASTGLTF_EXPORT class URI;

	/**
	 * Custom URI class for fastgltf's needs. glTF 2.0 only allows two types of URIs:
	 *  (1) Data URIs as specified in RFC 2397.
	 *  (2) Relative paths as specified in RFC 3986.
	 *
	 * See https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#uris for details.
	 * However, the glTF spec allows more broader URIs in client implementations. Therefore,
	 * this supports all types of URIs as defined in RFC 3986.
	 *
	 * This class, unlike fastgltf::URI, only holds a std::string_view to the URI and therefore
	 * doesn't own the allocation.
	 */
	FASTGLTF_EXPORT class URIView {
		friend class URI;

		std::string_view view;

		std::string_view _scheme;
		std::string_view _path;

		std::string_view _userinfo;
		std::string_view _host;
		std::string_view _port;

		std::string_view _query;
		std::string_view _fragment;

		bool _valid = true;

		void parse();

		[[nodiscard]] auto data() const noexcept -> const char*;

	public:
		explicit URIView() noexcept;
		explicit URIView(std::string_view uri) noexcept;
		URIView(const URIView& other) noexcept;

		URIView& operator=(const URIView& other);
		URIView& operator=(std::string_view other);

		[[nodiscard]] auto string() const noexcept -> std::string_view;

		[[nodiscard]] auto scheme() const noexcept -> std::string_view;
		[[nodiscard]] auto userinfo() const noexcept -> std::string_view;
		[[nodiscard]] auto host() const noexcept -> std::string_view;
		[[nodiscard]] auto port() const noexcept -> std::string_view;
		[[nodiscard]] auto path() const noexcept -> std::string_view;
		[[nodiscard]] auto query() const noexcept -> std::string_view;
		[[nodiscard]] auto fragment() const noexcept -> std::string_view;

		[[nodiscard]] auto fspath() const -> std::filesystem::path;
		[[nodiscard]] bool valid() const noexcept;
		[[nodiscard]] bool isLocalPath() const noexcept;
		[[nodiscard]] bool isDataUri() const noexcept;
	};

	/**
	 * Custom URI class for fastgltf's needs. glTF 2.0 only allows two types of URIs:
	 *  (1) Data URIs as specified in RFC 2397.
	 *  (2) Relative paths as specified in RFC 3986.
	 *
	 * See https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#uris for details.
	 * However, the glTF spec allows more broader URIs in client implementations. Therefore,
	 * this supports all types of URIs as defined in RFC 3986.
	 *
	 * This class, unlike fastgltf::URIView, holds a std::string which contains the URI. It
	 * also decodes any percent-encoded characters.
	 */
	class URI {
		std::string uri;
		URIView view;

		void readjustViews(const URIView& other);

	public:
		explicit URI() noexcept;

		explicit URI(std::string uri) noexcept;
		explicit URI(std::string_view uri) noexcept;
		explicit URI(const URIView& view) noexcept;

		URI(const URI& other);
		URI(URI&& other) noexcept;

		URI& operator=(const URI& other);
		URI& operator=(const URIView& other);
		URI& operator=(URI&& other) noexcept;
		bool operator==(const URI& other) const;

		operator URIView() const noexcept;

		static void decodePercents(std::string& x) noexcept;

		[[nodiscard]] auto string() const noexcept -> std::string_view;
		[[nodiscard]] auto c_str() const noexcept -> const char*;

		[[nodiscard]] auto scheme() const noexcept -> std::string_view;
		[[nodiscard]] auto userinfo() const noexcept -> std::string_view;
		[[nodiscard]] auto host() const noexcept -> std::string_view;
		[[nodiscard]] auto port() const noexcept -> std::string_view;
		[[nodiscard]] auto path() const noexcept -> std::string_view;
		[[nodiscard]] auto query() const noexcept -> std::string_view;
		[[nodiscard]] auto fragment() const noexcept -> std::string_view;

		[[nodiscard]] auto fspath() const -> std::filesystem::path;
		[[nodiscard]] bool valid() const noexcept;
		[[nodiscard]] bool isLocalPath() const noexcept;
		[[nodiscard]] bool isDataUri() const noexcept;
	};

	/**
	 * Represents the minimum and maximum bounds for glTF accessors in a better interface to avoid
	 * heavy usage of std::variant, which can pollute the user's code needlessly.
	 */
	FASTGLTF_EXPORT class AccessorBoundsArray {
	public:
		enum class BoundsType {
			int64,
			float64,
		};

		template <typename T>
		using is_valid_type = is_any_of<T, std::int64_t, double>;
		template <typename T>
		static constexpr auto is_valid_type_v = is_valid_type<T>::value;

	private:
		std::size_t len;
		BoundsType dataType;
		union {
			std::unique_ptr<std::int64_t[]> int64_buffer;
			std::unique_ptr<double[]> float64_buffer;
		};

	public:
		explicit AccessorBoundsArray(const std::size_t len, const BoundsType type) : len(len), dataType(type) {
			switch (dataType) {
				case BoundsType::int64:
					new (&int64_buffer) std::unique_ptr<std::int64_t[]>(new std::int64_t[len]());
					break;
				case BoundsType::float64:
					new (&float64_buffer) std::unique_ptr<double[]>(new double[len]());
					break;
				default:
					FASTGLTF_UNREACHABLE
			}
		}

		template <typename T, std::enable_if_t<is_valid_type_v<T>, bool> = true>
		static AccessorBoundsArray ForType(const std::size_t len) {
			if constexpr (std::is_same_v<T, std::int64_t>) {
				return AccessorBoundsArray(len, BoundsType::int64);
			} else if constexpr(std::is_same_v<T, double>) {
				return AccessorBoundsArray(len, BoundsType::float64);
			}
			FASTGLTF_UNREACHABLE
		}

		~AccessorBoundsArray() {
			switch (dataType) {
				case BoundsType::int64:
					std::destroy_at(&int64_buffer);
					break;
				case BoundsType::float64:
					std::destroy_at(&float64_buffer);
					break;
				default:
					FASTGLTF_UNREACHABLE
			}
		}

		AccessorBoundsArray(const AccessorBoundsArray& other) = delete;
		AccessorBoundsArray(AccessorBoundsArray&& other) noexcept
			: len(other.len), dataType(other.dataType) {
			switch (other.dataType) {
				case BoundsType::int64:
					new (&int64_buffer) std::unique_ptr(std::move(other.int64_buffer));
					break;
				case BoundsType::float64:
					new (&float64_buffer) std::unique_ptr(std::move(other.float64_buffer));
					break;
				default:
					FASTGLTF_UNREACHABLE
			}
		}

		auto& operator=(const AccessorBoundsArray& other) = delete;
		auto& operator=(AccessorBoundsArray&& other) noexcept {
			len = other.len;
			dataType = other.dataType;
			switch (dataType) {
				case BoundsType::int64:
					std::destroy_at(&int64_buffer);
					break;
				case BoundsType::float64:
					std::destroy_at(&float64_buffer);
					break;
				default:
					FASTGLTF_UNREACHABLE
			}
			switch (other.dataType) {
				case BoundsType::int64:
					new (&int64_buffer) std::unique_ptr(std::move(other.int64_buffer));
					break;
				case BoundsType::float64:
					new (&float64_buffer) std::unique_ptr(std::move(other.float64_buffer));
					break;
				default:
					FASTGLTF_UNREACHABLE
			}
			return *this;
		}

		bool operator==(const AccessorBoundsArray& other) const {
			FASTGLTF_REQUIRE(dataType == other.dataType);
			FASTGLTF_REQUIRE(len == other.len);
			if (dataType == BoundsType::int64) {
				FASTGLTF_REQUIRE(std::equal(int64_buffer.get(), int64_buffer.get() + len, other.int64_buffer.get()));
			} else {
				FASTGLTF_REQUIRE(std::equal(float64_buffer.get(), float64_buffer.get() + len, other.float64_buffer.get()));
			}
			return true;
		}

		[[nodiscard]] BoundsType type() const noexcept {
			return dataType;
		}

		template <typename T, std::enable_if_t<is_valid_type_v<T>, bool> = true>
		[[nodiscard]] bool isType() const noexcept {
			switch (dataType) {
				case BoundsType::int64:
					return std::is_same_v<T, std::int64_t>;
				case BoundsType::float64:
					return std::is_same_v<T, double>;
				default:
					return false;
			}
		}

		[[nodiscard]] std::size_t size() const noexcept {
			return len;
		}

		template <typename T, std::enable_if_t<is_valid_type_v<T>, bool> = true>
		[[nodiscard]] auto* data() noexcept {
			assert(isType<T>());
			if constexpr (std::is_same_v<T, std::int64_t>) {
				return int64_buffer.get();
			} else if constexpr (std::is_same_v<T, double>) {
				return float64_buffer.get();
			}
			FASTGLTF_UNREACHABLE
		}

		template <typename T, std::enable_if_t<is_valid_type_v<T>, bool> = true>
		[[nodiscard]] const auto* data() const noexcept {
			assert(isType<T>());
			if constexpr (std::is_same_v<T, std::int64_t>) {
				return int64_buffer.get();
			} else if constexpr (std::is_same_v<T, double>) {
				return float64_buffer.get();
			}
			FASTGLTF_UNREACHABLE
		}

		template <typename T, std::enable_if_t<is_valid_type_v<T>, bool> = true>
		[[nodiscard]] T get(const std::size_t pos) const {
			assert(pos < len && isType<T>());
			return data<T>()[pos];
		}

		template <typename T, std::enable_if_t<is_valid_type_v<T>, bool> = true>
		void set(const std::size_t pos, const T value) {
			assert(pos < len && isType<T>());
			data<T>()[pos] = value;
		}
	};

	FASTGLTF_EXPORT inline constexpr std::size_t dynamic_extent = std::numeric_limits<std::size_t>::max();

	/**
	 * Custom span class imitating C++20's std::span for referencing bytes without owning the
	 * allocation. Can also directly be converted to a std::span or used by itself.
	 */
	FASTGLTF_EXPORT template <typename T, std::size_t Extent = dynamic_extent>
	class span {
		using element_type = T;
		using value_type = std::remove_cv_t<T>;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;
		using pointer = T*;
		using const_pointer = const T*;
		using reference = T&;
		using const_reference = const T&;

		using iterator = pointer;
		using reverse_iterator = std::reverse_iterator<iterator>;

		pointer _ptr = nullptr;
		size_type _size = 0;

	public:
		static constexpr std::size_t extent = Extent;

		constexpr span() = default;

		// std::span ctor (2)
		template <typename Iterator>
		explicit /*(Extent != dynamic_extent)*/ constexpr span(Iterator first, const size_type count) : _ptr(first), _size(count) {}

		// std::span ctor (5)
		template<typename U, std::size_t N>
		constexpr span(std::array<U, N>& arr) noexcept : _ptr(arr.data()), _size(N) {}

		// std::span ctor (6)
		template<typename U, std::size_t N>
		constexpr span(const std::array<U, N>& arr) noexcept : _ptr(arr.data()), _size(N) {}

#if FASTGLTF_CPP_20
		constexpr span(std::span<T> data) : _ptr(data.data()), _size(data.size()) {}
#endif

		constexpr span(const span& other) noexcept = default;
		constexpr span& operator=(const span& other) = default;

		[[nodiscard]] constexpr iterator begin() const noexcept {
			return data();
		}
		[[nodiscard]] constexpr iterator end() const noexcept {
			return data() + size();
		}
		[[nodiscard]] constexpr reverse_iterator rbegin() const noexcept {
			return std::reverse_iterator(end());
		}
		[[nodiscard]] constexpr reverse_iterator rend() const noexcept {
			return std::reverse_iterator(begin());
		}

		[[nodiscard]] constexpr reference front() const { return *begin(); }
		[[nodiscard]] constexpr reference back() const { return *(end() - 1); }

		[[nodiscard]] constexpr reference operator[](size_type idx) const {
			return data()[idx];
		}

		[[nodiscard]] constexpr reference at(size_type idx) const {
			return data()[idx];
		}

		[[nodiscard]] constexpr pointer data() const {
			return _ptr;
		}

		[[nodiscard]] constexpr size_type size() const {
			return _size;
		}

		[[nodiscard]] constexpr size_type size_bytes() const {
			return size() * sizeof(element_type);
		}

		[[nodiscard]] constexpr bool empty() const {
			return size() == 0;
		}

		[[nodiscard]] constexpr span first(size_type count) const {
			return span(_ptr, count);
		}

		[[nodiscard]] constexpr span last(size_type count) const {
			return span(&data()[size() - count], count);
		}

		[[nodiscard]] constexpr span subspan(size_type offset, size_type count = dynamic_extent) const {
			return span(&data()[offset], count == dynamic_extent ? size() - offset : count);
		}

#if FASTGLTF_CPP_20
		operator std::span<T, Extent == dynamic_extent ? std::dynamic_extent : Extent>() const {
			return std::span<T, Extent == dynamic_extent ? std::dynamic_extent : Extent>(data(), size());
		}
#endif
	};

	FASTGLTF_EXPORT template <typename T>
	span(T* data, std::size_t count) -> span<T>;

	FASTGLTF_EXPORT template <typename T>
	span(const T* data, std::size_t size) -> span<const T>;

	// std::span deduction guide (4)
	FASTGLTF_EXPORT template<class T, std::size_t N>
	span(const std::array<T, N>&) -> span<const T, N>;

    FASTGLTF_EXPORT using CustomBufferId = std::uint64_t;

	template <typename T>
	constexpr bool span_equals(const span<T>& first, const span<T>& other) {
		if (first.size() != other.size()) {
			return false;
		}
		return std::equal(first.data(), first.data() + first.size(), other.data());
	}


    /**
     * Namespace for structs that describe individual sources of data for images and/or buffers.
     */
    namespace sources {
        FASTGLTF_EXPORT struct BufferView {
            std::size_t bufferViewIndex;
            MimeType mimeType = MimeType::None;
			bool operator==(const BufferView& other) const {
				FASTGLTF_REQUIRE(bufferViewIndex == other.bufferViewIndex);
				FASTGLTF_REQUIRE(mimeType == other.mimeType);
				return true;
			}
        };

        FASTGLTF_EXPORT struct URI {
            std::size_t fileByteOffset;
            fastgltf::URI uri;
            MimeType mimeType = MimeType::None;
			bool operator==(const URI& other) const {
				FASTGLTF_REQUIRE(fileByteOffset == other.fileByteOffset);
				FASTGLTF_REQUIRE(uri == other.uri);
				FASTGLTF_REQUIRE(mimeType == other.mimeType);
				return true;
			}
        };

        FASTGLTF_EXPORT struct Array {
            StaticVector<std::byte> bytes;
            MimeType mimeType = MimeType::None;
			bool operator==(const Array& other) const {
				FASTGLTF_REQUIRE(mimeType == other.mimeType);
				FASTGLTF_REQUIRE(bytes == other.bytes);
				return true;
			}
        };

		/** @note This type is not used by the fastgltf parser and is only used for exporting. Use sources::Array instead when importing intead. */
		FASTGLTF_EXPORT struct Vector {
			std::vector<std::byte> bytes;
			MimeType mimeType = MimeType::None;
			bool operator==(const Vector& other) const {
				FASTGLTF_REQUIRE(bytes == other.bytes);
				FASTGLTF_REQUIRE(mimeType == other.mimeType);
				return true;
			}
		};

        FASTGLTF_EXPORT struct CustomBuffer {
            CustomBufferId id;
            MimeType mimeType = MimeType::None;
			bool operator==(const CustomBuffer& other) const {
				FASTGLTF_REQUIRE(id == other.id);
				FASTGLTF_REQUIRE(mimeType == other.mimeType);
				return true;
			}
        };
		

        FASTGLTF_EXPORT struct ByteView {
            span<const std::byte> bytes;
            MimeType mimeType = MimeType::None;
			bool operator==(const ByteView& other) const {
				FASTGLTF_REQUIRE(span_equals(bytes, other.bytes));
				FASTGLTF_REQUIRE(mimeType == other.mimeType);
				return true;
			}
        };

		FASTGLTF_EXPORT struct Fallback {
			bool operator==(const Fallback& other) const {
				return true;
			}
		};
    } // namespace sources

    /**
     * Represents the data source of a buffer or image. These could be a buffer view, a file path
     * (including offsets), a StaticVector (if #Options::LoadExternalBuffers or #Options::LoadGLBBuffers
     * was specified), or the ID of a custom buffer.
     *
     * @note As a user, you should never encounter this variant holding the std::monostate, as that would be an ill-formed glTF,
     * which fastgltf already checks for while parsing.
     *
     * @note For buffers, this variant will never hold a sources::BufferView, as only images are able to reference buffer views as a source.
     */
    FASTGLTF_EXPORT using DataSource = std::variant<std::monostate, sources::BufferView, sources::URI, sources::Array, sources::Vector, sources::CustomBuffer, sources::ByteView, sources::Fallback>;

	namespace {
		template <typename T>
		static bool isPaddedBufferEquivalent(const T& padded_buf, const T& unpadded_buf, std::size_t minSize = -1) {
			bool check_padding = minSize != -1;
			if (!check_padding) {
				FASTGLTF_REQUIRE(padded_buf.size() == unpadded_buf.size());
				minSize = padded_buf.size();
			} else {
				FASTGLTF_REQUIRE(minSize <= padded_buf.size());
				FASTGLTF_REQUIRE(minSize <= unpadded_buf.size());
			}
			FASTGLTF_REQUIRE(std::equal(padded_buf.begin(), padded_buf.begin() + minSize, unpadded_buf.begin()));
			// if (check_padding) {
			// 	auto extra_bytes = padded_buf.size() - minSize;
			// 	for (std::size_t i = minSize; i < minSize + extra_bytes; ++i) {
			// 		// check if all the extra bytes are zero or equals padded_buf
			// 		if (unpadded_buf.size() <= i || unpadded_buf[i] != padded_buf[i]) {
			// 			FASTGLTF_REQUIRE(padded_buf[i] == std::byte(0));
			// 		}
			// 	}
			// }
			return true;
		}
	}

	// This assumes that, if you're comparing two data sources with possible padding, the padded buffer is the first one
	static bool compareDataSource(const fastgltf::DataSource &padded,
							const fastgltf::DataSource &unpadded,
							std::size_t minSize = -1, bool ignoreMimeType = false) {
		if (std::holds_alternative<std::monostate>(padded)) {
			FASTGLTF_REQUIRE(!std::holds_alternative<std::monostate>(unpadded));
			return true;
		} else if (std::holds_alternative<fastgltf::sources::BufferView>(padded)) {
			FASTGLTF_REQUIRE(std::holds_alternative<fastgltf::sources::BufferView>(unpadded));
			auto a_bv = std::get<fastgltf::sources::BufferView>(padded);
			auto b_bv = std::get<fastgltf::sources::BufferView>(unpadded);
			FASTGLTF_REQUIRE(a_bv == b_bv);
			return true;
		} else if (std::holds_alternative<fastgltf::sources::URI>(padded)) {
			FASTGLTF_REQUIRE(std::holds_alternative<fastgltf::sources::URI>(unpadded));
			auto a_uri = std::get<fastgltf::sources::URI>(padded);
			auto b_uri = std::get<fastgltf::sources::URI>(unpadded);
			FASTGLTF_REQUIRE(a_uri == b_uri);
			return true;
		} else if (std::holds_alternative<fastgltf::sources::Array>(padded)) {
			FASTGLTF_REQUIRE(std::holds_alternative<fastgltf::sources::Array>(unpadded));
			auto a_array = std::get<fastgltf::sources::Array>(padded);
			auto b_array = std::get<fastgltf::sources::Array>(unpadded);
			if (!ignoreMimeType) {
				FASTGLTF_REQUIRE(a_array.mimeType == b_array.mimeType);
			}
			FASTGLTF_REQUIRE(isPaddedBufferEquivalent(a_array.bytes, b_array.bytes, minSize));
			return true;
		} else if (std::holds_alternative<fastgltf::sources::Vector>(padded)) {
			FASTGLTF_REQUIRE(std::holds_alternative<fastgltf::sources::Vector>(unpadded));
			auto a_vector = std::get<fastgltf::sources::Vector>(padded);
			auto b_vector = std::get<fastgltf::sources::Vector>(unpadded);
			if (!ignoreMimeType) {
				FASTGLTF_REQUIRE(a_vector.mimeType == b_vector.mimeType);
			}
			FASTGLTF_REQUIRE(isPaddedBufferEquivalent(a_vector.bytes, b_vector.bytes, minSize));
			return true;
		} else if (std::holds_alternative<fastgltf::sources::CustomBuffer>(padded)) {
			FASTGLTF_REQUIRE(std::holds_alternative<fastgltf::sources::CustomBuffer>(unpadded));
			auto a_custom_buffer = std::get<fastgltf::sources::CustomBuffer>(padded);
			auto b_custom_buffer = std::get<fastgltf::sources::CustomBuffer>(unpadded);
			FASTGLTF_REQUIRE(a_custom_buffer == b_custom_buffer);
			return true;
		} else if (std::holds_alternative<fastgltf::sources::ByteView>(padded)) {
			FASTGLTF_REQUIRE(std::holds_alternative<fastgltf::sources::ByteView>(unpadded));
			auto a_byte_view = std::get<fastgltf::sources::ByteView>(padded);
			auto b_byte_view = std::get<fastgltf::sources::ByteView>(unpadded);
			if (!ignoreMimeType) {
				FASTGLTF_REQUIRE(a_byte_view.mimeType == b_byte_view.mimeType);
			}
			FASTGLTF_REQUIRE(isPaddedBufferEquivalent(a_byte_view.bytes, b_byte_view.bytes, minSize));
			return true;
		} else if (std::holds_alternative<fastgltf::sources::Fallback>(padded)) {
			FASTGLTF_REQUIRE(std::holds_alternative<fastgltf::sources::Fallback>(unpadded));
			return true;
		}
		return false;
	}
	// operator== for DataSource
	inline bool operator==(const DataSource& a, const DataSource& b) {
		return compareDataSource(a, b);
	}
	inline bool operator!=(const DataSource& a, const DataSource& b) {
		return !(a == b);
	}


	template <typename T>
	bool compareUniquePtr(const std::unique_ptr<T>& a, const std::unique_ptr<T>& b) {
		FASTGLTF_REQUIRE(!a == !b);
		if (!a) {
			return true;
		}
		FASTGLTF_REQUIRE(*a == *b);
		return true;
	}

    FASTGLTF_EXPORT struct AnimationChannel {
        std::size_t samplerIndex;
        Optional<std::size_t> nodeIndex;
        AnimationPath path;
		bool operator==(const AnimationChannel& other) const {
			FASTGLTF_REQUIRE(samplerIndex == other.samplerIndex);
			FASTGLTF_REQUIRE(nodeIndex == other.nodeIndex);
			FASTGLTF_REQUIRE(path == other.path);
			return true;
		}
    };

    FASTGLTF_EXPORT struct AnimationSampler {
        std::size_t inputAccessor;
        std::size_t outputAccessor;
        AnimationInterpolation interpolation = AnimationInterpolation::Linear;
		bool operator==(const AnimationSampler& other) const {
			FASTGLTF_REQUIRE(inputAccessor == other.inputAccessor);
			FASTGLTF_REQUIRE(outputAccessor == other.outputAccessor);
			FASTGLTF_REQUIRE(interpolation == other.interpolation);
			return true;
		}
    };

    FASTGLTF_EXPORT struct Animation {
	    FASTGLTF_FG_PMR_NS::MaybeSmallVector<AnimationChannel> channels;
	    FASTGLTF_FG_PMR_NS::MaybeSmallVector<AnimationSampler> samplers;

        FASTGLTF_STD_PMR_NS::string name;
		bool operator==(const Animation& other) const {
			FASTGLTF_REQUIRE(channels == other.channels);
			FASTGLTF_REQUIRE(samplers == other.samplers);
			FASTGLTF_REQUIRE(name == other.name);
			return true;
		}
    };

    FASTGLTF_EXPORT struct AssetInfo {
        FASTGLTF_STD_PMR_NS::string gltfVersion;
        FASTGLTF_STD_PMR_NS::string copyright;
        FASTGLTF_STD_PMR_NS::string generator;
		bool operator==(const AssetInfo& other) const {
			FASTGLTF_REQUIRE(gltfVersion == other.gltfVersion);
			FASTGLTF_REQUIRE(copyright == other.copyright);
			FASTGLTF_REQUIRE(generator == other.generator);
			return true;
		}
    };

    FASTGLTF_EXPORT struct Camera {
        struct Orthographic {
            num xmag;
            num ymag;
            num zfar;
            num znear;
			bool operator==(const Orthographic& other) const {
				FASTGLTF_REQUIRE(xmag == other.xmag);
				FASTGLTF_REQUIRE(ymag == other.ymag);
				FASTGLTF_REQUIRE(zfar == other.zfar);
				FASTGLTF_REQUIRE(znear == other.znear);
				return true;
			}
        };
        struct Perspective {
            Optional<num> aspectRatio;
            num yfov;
            // If omitted, use an infinite projection matrix.
            Optional<num> zfar;
            num znear;
			bool operator==(const Perspective& other) const {
				FASTGLTF_REQUIRE(aspectRatio == other.aspectRatio);
				FASTGLTF_REQUIRE(yfov == other.yfov);
				FASTGLTF_REQUIRE(zfar == other.zfar);
				FASTGLTF_REQUIRE(znear == other.znear);
				return true;
			}
        };

        /**
         * Variant holding either a perspective or a orthographic camera. Use std::holds_alternative
         * and/or std::get_if to figure out which camera type is being used.
         */
        std::variant<Perspective, Orthographic> camera;
        FASTGLTF_STD_PMR_NS::string name;
		bool operator==(const Camera& other) const {
			FASTGLTF_REQUIRE(camera == other.camera);
			FASTGLTF_REQUIRE(name == other.name);
			return true;
		}
    };

    FASTGLTF_EXPORT struct Skin {
	    Optional<std::size_t> inverseBindMatrices;
        Optional<std::size_t> skeleton;
        FASTGLTF_FG_PMR_NS::MaybeSmallVector<std::size_t> joints;

        FASTGLTF_STD_PMR_NS::string name;
		bool operator==(const Skin& other) const {
			FASTGLTF_REQUIRE(inverseBindMatrices == other.inverseBindMatrices);
			FASTGLTF_REQUIRE(skeleton == other.skeleton);
			FASTGLTF_REQUIRE(joints == other.joints);
			FASTGLTF_REQUIRE(name == other.name);
			return true;
		}
    };

    FASTGLTF_EXPORT struct Sampler {
	    Optional<Filter> magFilter;
	    Optional<Filter> minFilter;
        Wrap wrapS = Wrap::Repeat;
        Wrap wrapT = Wrap::Repeat;

        FASTGLTF_STD_PMR_NS::string name;
		bool operator==(const Sampler& other) const {
			FASTGLTF_REQUIRE(magFilter == other.magFilter);
			FASTGLTF_REQUIRE(minFilter == other.minFilter);
			FASTGLTF_REQUIRE(wrapS == other.wrapS);
			FASTGLTF_REQUIRE(wrapT == other.wrapT);
			FASTGLTF_REQUIRE(name == other.name);
			return true;
		}
    };

    FASTGLTF_EXPORT struct Scene {
	    FASTGLTF_FG_PMR_NS::MaybeSmallVector<std::size_t> nodeIndices;

        FASTGLTF_STD_PMR_NS::string name;
		bool operator==(const Scene& other) const {
			FASTGLTF_REQUIRE(nodeIndices == other.nodeIndices);
			FASTGLTF_REQUIRE(name == other.name);
			return true;
		}
    };

	FASTGLTF_EXPORT struct TRS {
		math::fvec3 translation = math::fvec3(0.f);
		math::fquat rotation = math::fquat(0.f, 0.f, 0.f, 1.f);
		math::fvec3 scale = math::fvec3(1.f);
		bool operator==(const TRS& other) const {
			FASTGLTF_REQUIRE(translation == other.translation);
			FASTGLTF_REQUIRE(rotation == other.rotation);
			FASTGLTF_REQUIRE(scale == other.scale);
			return true;
		}
	};

	FASTGLTF_EXPORT struct Attribute {
		FASTGLTF_STD_PMR_NS::string name;
		std::size_t accessorIndex;
		bool operator==(const Attribute& other) const {
			FASTGLTF_REQUIRE(name == other.name);
			FASTGLTF_REQUIRE(accessorIndex == other.accessorIndex);
			return true;
		}
	};

#if FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES
	FASTGLTF_EXPORT struct SphereShape {
		num radius = 0.5;

		bool operator==(const SphereShape& other) const {
			FASTGLTF_REQUIRE(radius == other.radius);
			return true;
		}
	};

	FASTGLTF_EXPORT struct BoxShape {
		math::fvec3 size = { 1, 1, 1 };

		bool operator==(const BoxShape& other) const {
			FASTGLTF_REQUIRE(size == other.size);
			return true;
		}
	};

	FASTGLTF_EXPORT struct CapsuleShape {
		num height = 0.5;

		num radiusBottom = 0.25;

		num radiusTop = 0.25;

		bool operator==(const CapsuleShape& other) const {
			FASTGLTF_REQUIRE(height == other.height);
			FASTGLTF_REQUIRE(radiusBottom == other.radiusBottom);
			FASTGLTF_REQUIRE(radiusTop == other.radiusTop);
			return true;
		}
	};

	FASTGLTF_EXPORT struct CylinderShape {
		num height = 0.5;

		num radiusBottom = 0.25;

		num radiusTop = 0.25;

		bool operator==(const CylinderShape& other) const {
			FASTGLTF_REQUIRE(height == other.height);
			FASTGLTF_REQUIRE(radiusBottom == other.radiusBottom);
			FASTGLTF_REQUIRE(radiusTop == other.radiusTop);
			return true;
		}
	};

	using Shape = std::variant<SphereShape, BoxShape, CapsuleShape, CylinderShape>;
#endif

#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
    FASTGLTF_EXPORT struct Motion {
        /**
         * When true, treat the rigid body as having infinite mass. Its velocity will be constant during simulation
         */
        bool isKinematic = false;

        /**
         * The mass of the rigid body. Larger values imply the rigid body is harder to move
         */
        Optional<num> mass;

		/**
		 * Center of mass of the rigid body in node space
		 */
		math::fvec3 centerOfMass = { 0, 0, 0 };

		/**
		 * The principal moments of inertia. Larger values imply the rigid body is harder to rotate
		 */
		Optional<math::fvec3> inertialDiagonal;

        /**
         * The quaternion rotating from inertia major axis space to node space
         */
        Optional<math::fvec4> inertialOrientation;

        /**
         * Initial linear velocity of the rigid body in node space
         */
        math::fvec3 linearVelocity = {0, 0, 0};

        /**
         * Initial angular velocity of the rigid body in node space
         */
        math::fvec3 angularVelocity = {0, 0, 0};

        /**
         * A multiplier applied to the acceleration due to gravity
         */
        num gravityFactor = 1;

		bool operator==(const Motion& other) const {
			FASTGLTF_REQUIRE(isKinematic == other.isKinematic);
			FASTGLTF_REQUIRE(mass == other.mass);
			FASTGLTF_REQUIRE(centerOfMass == other.centerOfMass);
			FASTGLTF_REQUIRE(inertialDiagonal == other.inertialDiagonal);
			FASTGLTF_REQUIRE(inertialOrientation == other.inertialOrientation);
			FASTGLTF_REQUIRE(linearVelocity == other.linearVelocity);
			FASTGLTF_REQUIRE(angularVelocity == other.angularVelocity);
			FASTGLTF_REQUIRE(gravityFactor == other.gravityFactor);
			return true;
		}
	};

	FASTGLTF_EXPORT struct Geometry {
        /**
         * The index of a top-level `KHR_implicit_shapes.shape`, providing an implicit representation of the geometry
         */
        Optional<size_t> shape;

        /**
         * The index of a glTF `node` which provides a mesh representation of the geometry
         */
        Optional<size_t> node;

        /**
         * Flag to indicate that the geometry should be a convex hull.
         */
        bool convexHull;

		bool operator==(const Geometry& other) const {
			FASTGLTF_REQUIRE(shape == other.shape);
			FASTGLTF_REQUIRE(node == other.node);
			FASTGLTF_REQUIRE(convexHull == other.convexHull);
			return true;
		}
	};

	FASTGLTF_EXPORT struct PhysicsMaterial {
		num staticFriction = 0.6f;

		num dynamicFriction = 0.6f;

		num restitution = 0.0f;

		CombineMode frictionCombine;

		CombineMode restitutionCombine;

		bool operator==(const PhysicsMaterial& other) const {
			FASTGLTF_REQUIRE(staticFriction == other.staticFriction);
			FASTGLTF_REQUIRE(dynamicFriction == other.dynamicFriction);
			FASTGLTF_REQUIRE(restitution == other.restitution);
			FASTGLTF_REQUIRE(frictionCombine == other.frictionCombine);
			FASTGLTF_REQUIRE(restitutionCombine == other.restitutionCombine);
			return true;
		}
	};

	FASTGLTF_EXPORT struct CollisionFilter {
        /**
         * An array of arbitrary strings indicating the "system" a node is a member of
         */
        FASTGLTF_FG_PMR_NS::MaybeSmallVector<FASTGLTF_STD_PMR_NS::string> collisionSystems;

        /**
         * An array of strings representing the systems which this node can _not_ collide with
         */
        FASTGLTF_FG_PMR_NS::MaybeSmallVector<FASTGLTF_STD_PMR_NS::string> notCollideWithSystems;

        /**
         * An array of strings representing the systems which this node can collide with
         */
        FASTGLTF_FG_PMR_NS::MaybeSmallVector<FASTGLTF_STD_PMR_NS::string> collideWithSystems;

		bool operator==(const CollisionFilter& other) const {
			FASTGLTF_REQUIRE(collisionSystems == other.collisionSystems);
			FASTGLTF_REQUIRE(notCollideWithSystems == other.notCollideWithSystems);
			FASTGLTF_REQUIRE(collideWithSystems == other.collideWithSystems);
			return true;
		}
	};

	FASTGLTF_EXPORT struct Collider {
        /**
         * An object describing the geometrical representation of this collider
         */
        Geometry geometry;

        /**
         * Indexes into the top-level `physicsMaterials` and describes how the collider should respond to collisions
         */
        Optional<std::size_t> physicsMaterial;

        /**
         * Indexes into the top-level `collisionFilters` and describes a filter which determines if this collider should perform collision detection against another collider
         */
        Optional<std::size_t> collisionFilter;

		bool operator==(const Collider& other) const {
			FASTGLTF_REQUIRE(geometry == other.geometry);
			FASTGLTF_REQUIRE(physicsMaterial == other.physicsMaterial);
			FASTGLTF_REQUIRE(collisionFilter == other.collisionFilter);
			return true;
		}
	};

	FASTGLTF_EXPORT struct GeometryTrigger {
		/**
		 * An object describing the geometrical representation of this collider
		 */
		Geometry geometry;

		/**
		 * Indexes into the top-level `collisionFilters` and describes a filter which determines if this collider should perform collision detection against another collider
		 */
		Optional<std::size_t> collisionFilter;

		bool operator==(const GeometryTrigger& other) const {
			FASTGLTF_REQUIRE(geometry == other.geometry);
			FASTGLTF_REQUIRE(collisionFilter == other.collisionFilter);
			return true;
		}
	};

	FASTGLTF_EXPORT struct NodeTrigger {

		/**
		 * For compound triggers, the set of descendant glTF nodes with a trigger property that make up this compound trigger
		 */
		FASTGLTF_FG_PMR_NS::MaybeSmallVector<std::size_t> nodes;

		bool operator==(const NodeTrigger& other) const {
			FASTGLTF_REQUIRE(nodes == other.nodes);
			return true;
		}
	};

	FASTGLTF_EXPORT struct JointLimit {
		/**
		 * The linear axes to constrain (0=X, 1=Y, 2=Z)
		 */
		FASTGLTF_FG_PMR_NS::SmallVector<uint8_t, 3> linearAxes;

		/**
		 * The angular axes to constrain (0=X, 1=Y, 2=Z)
		 */
		FASTGLTF_FG_PMR_NS::SmallVector<uint8_t, 3> angularAxes;

		/**
		 * The minimum allowed relative distance/angle
		 */
		Optional<num> min;

		/**
		 * The maximum allowed relative distance/angle
		 */
		Optional<num> max;

		/**
		 * Optional softness of the limits when beyond the limits
		 */
		Optional<num> stiffness;

		/**
		 * Optional spring damping applied when beyond the limits
		 */
		num damping = 0;

		bool operator==(const JointLimit& other) const {
			FASTGLTF_REQUIRE(linearAxes == other.linearAxes);
			FASTGLTF_REQUIRE(angularAxes == other.angularAxes);
			FASTGLTF_REQUIRE(min == other.min);
			FASTGLTF_REQUIRE(max == other.max);
			FASTGLTF_REQUIRE(stiffness == other.stiffness);
			FASTGLTF_REQUIRE(damping == other.damping);
			return true;
		}
	};

	FASTGLTF_EXPORT struct JointDrive {
        /**
         * Determines if the drive affects is a `linear` or `angular` drive
         */
        DriveType type;

        /**
         * Determines if the drive is operating in `force` or `acceleration` mode
         */
        DriveMode mode;

        /**
         * The index of the axis which this drive affects
         */
        uint8_t axis;

        /**
         * The maximum force that the drive can apply
         */
        num maxForce;

        /**
         * The desired relative target between the pivot axes
         */
        num positionTarget;

        /**
         * The desired relative velocity of the pivot axes
         */
        num velocityTarget;

        /**
         * The drive's stiffness, used to achieve the position target
         */
        num stiffness = 0;

        /**
         * The damping factor applied to reach the velocity target
         */
        num damping = 0;

		bool operator==(const JointDrive& other) const {
			FASTGLTF_REQUIRE(type == other.type);
			FASTGLTF_REQUIRE(mode == other.mode);
			FASTGLTF_REQUIRE(axis == other.axis);
			FASTGLTF_REQUIRE(maxForce == other.maxForce);
			FASTGLTF_REQUIRE(positionTarget == other.positionTarget);
			FASTGLTF_REQUIRE(velocityTarget == other.velocityTarget);
			FASTGLTF_REQUIRE(stiffness == other.stiffness);
			FASTGLTF_REQUIRE(damping == other.damping);
			return true;
		}
	};

	FASTGLTF_EXPORT struct PhysicsJoint {
		FASTGLTF_FG_PMR_NS::MaybeSmallVector<JointLimit> limits;

        /**
         * Each drive specifies a force to apply along a single axis
         */
        FASTGLTF_FG_PMR_NS::MaybeSmallVector<JointDrive> drives;

		bool operator==(const PhysicsJoint& other) const {
			FASTGLTF_REQUIRE(limits == other.limits);
			FASTGLTF_REQUIRE(drives == other.drives);
			return true;
		}
	};

	FASTGLTF_EXPORT struct Joint {
	    std::size_t connectedNode;

		std::size_t joint;

		bool enableCollision = false;

		bool operator==(const Joint& other) const {
			FASTGLTF_REQUIRE(connectedNode == other.connectedNode);
			FASTGLTF_REQUIRE(joint == other.joint);
			FASTGLTF_REQUIRE(enableCollision == other.enableCollision);
			return true;
		}
	};

    FASTGLTF_EXPORT struct PhysicsRigidBody {
		Optional<Motion> motion;

		Optional<Collider> collider;

		Optional<std::variant<GeometryTrigger, NodeTrigger>> trigger;

		Optional<Joint> joint;

		bool operator==(const PhysicsRigidBody& other) const {
			FASTGLTF_REQUIRE(motion == other.motion);
			FASTGLTF_REQUIRE(collider == other.collider);
			FASTGLTF_REQUIRE(trigger == other.trigger);
			FASTGLTF_REQUIRE(joint == other.joint);
			return true;
		}
	};
#endif

    FASTGLTF_EXPORT struct Node {
        Optional<std::size_t> meshIndex;
	    Optional<std::size_t> skinIndex;
	    Optional<std::size_t> cameraIndex;

        /**
         * Only ever non-empty when KHR_lights_punctual is enabled and used by the asset.
         */
        Optional<std::size_t> lightIndex;

	    FASTGLTF_FG_PMR_NS::MaybeSmallVector<std::size_t> children;
	    FASTGLTF_FG_PMR_NS::MaybeSmallVector<num> weights;

        /**
         * Variant holding either the three TRS components; transform, rotation, and scale, or a
         * transformation matrix, which cannot skew or shear. The latter can be decomposed into
         * the TRS components by specifying Options::DecomposeNodeMatrices.
         */
        std::variant<TRS, math::fmat4x4> transform;

        /**
         * Only ever non-empty when EXT_mesh_gpu_instancing is enabled and used by the asset.
         */
        FASTGLTF_STD_PMR_NS::vector<Attribute> instancingAttributes;

        FASTGLTF_STD_PMR_NS::string name;

#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
		std::unique_ptr<PhysicsRigidBody> physicsRigidBody;
#endif

        [[nodiscard]] auto findInstancingAttribute(std::string_view attributeName) noexcept {
            for (auto it = instancingAttributes.begin(); it != instancingAttributes.end(); ++it) {
                if (it->name == attributeName)
                    return it;
            }
            return instancingAttributes.end();
        }

        [[nodiscard]] auto findInstancingAttribute(std::string_view attributeName) const noexcept {
            for (auto it = instancingAttributes.cbegin(); it != instancingAttributes.cend(); ++it) {
                if (it->name == attributeName)
                    return it;
            }
            return instancingAttributes.cend();
        }

		bool operator==(const Node& other) const {
			FASTGLTF_REQUIRE(meshIndex == other.meshIndex);
			FASTGLTF_REQUIRE(skinIndex == other.skinIndex);
			FASTGLTF_REQUIRE(cameraIndex == other.cameraIndex);
			FASTGLTF_REQUIRE(lightIndex == other.lightIndex);
			FASTGLTF_REQUIRE(children == other.children);
			FASTGLTF_REQUIRE(weights == other.weights);
			FASTGLTF_REQUIRE(transform == other.transform);
			FASTGLTF_REQUIRE(instancingAttributes == other.instancingAttributes);
			FASTGLTF_REQUIRE(name == other.name);
			return true;
		}
    };

	struct DracoCompressedPrimitive {
		std::size_t bufferView;
		FASTGLTF_FG_PMR_NS::SmallVector<Attribute, 4> attributes;

		[[nodiscard]] auto findAttribute(std::string_view name) noexcept {
			for (auto* it = attributes.begin(); it != attributes.end(); ++it) {
				if (it->name == name)
					return it;
			}
			return attributes.end();
		}

		[[nodiscard]] auto findAttribute(std::string_view name) const noexcept {
			for (const auto* it = attributes.cbegin(); it != attributes.cend(); ++it) {
				if (it->name == name)
					return it;
			}
			return attributes.cend();
		}

		bool operator==(const DracoCompressedPrimitive& other) const {
			FASTGLTF_REQUIRE(bufferView == other.bufferView);
			FASTGLTF_REQUIRE(attributes == other.attributes);
			return true;
		}
	};

    FASTGLTF_EXPORT struct Primitive {
		// Instead of a map, we have a list of attributes here. Each pair contains
		// the name of the attribute and the corresponding accessor index.
		FASTGLTF_FG_PMR_NS::SmallVector<Attribute, 4> attributes;
        PrimitiveType type = PrimitiveType::Triangles;

        FASTGLTF_STD_PMR_NS::vector<FASTGLTF_FG_PMR_NS::SmallVector<Attribute, 4>> targets;

        Optional<std::size_t> indicesAccessor;
        Optional<std::size_t> materialIndex;

		/**
		 * Represents the mappings data from KHR_material_variants.
		 * Use the variant index to index into this array to get the corresponding material index to use.
		 * If this vector is empty, the normal materialIndex should be used as a fallback.
		 */
		std::vector<Optional<std::size_t>> mappings;

		std::unique_ptr<DracoCompressedPrimitive> dracoCompression;

		[[nodiscard]] auto findAttribute(std::string_view name) noexcept {
			for (auto* it = attributes.begin(); it != attributes.end(); ++it) {
				if (it->name == name)
					return it;
			}
			return attributes.end();
		}

		[[nodiscard]] auto findAttribute(std::string_view name) const noexcept {
			for (const auto* it = attributes.cbegin(); it != attributes.cend(); ++it) {
				if (it->name == name)
					return it;
			}
			return attributes.cend();
		}

		[[nodiscard]] auto findTargetAttribute(std::size_t targetIndex, std::string_view name) noexcept {
			auto& targetAttributes = targets[targetIndex];
			for (auto* it = targetAttributes.begin(); it != targetAttributes.end(); ++it) {
				if (it->name == name)
					return it;
			}
			return targetAttributes.end();
		}

		[[nodiscard]] auto findTargetAttribute(std::size_t targetIndex, std::string_view name) const noexcept {
			const auto& targetAttributes = targets[targetIndex];
			for (const auto* it = targetAttributes.cbegin(); it != targetAttributes.cend(); ++it) {
				if (it->name == name)
					return it;
			}
			return targetAttributes.cend();
		}

		bool operator==(const Primitive& other) const {
			FASTGLTF_REQUIRE(attributes == other.attributes);
			FASTGLTF_REQUIRE(type == other.type);
			FASTGLTF_REQUIRE(targets == other.targets);
			FASTGLTF_REQUIRE(indicesAccessor == other.indicesAccessor);
			FASTGLTF_REQUIRE(materialIndex == other.materialIndex);
			FASTGLTF_REQUIRE(mappings == other.mappings);
			FASTGLTF_REQUIRE(compareUniquePtr(dracoCompression, other.dracoCompression));
			return true;
		}
	};

    FASTGLTF_EXPORT struct Mesh {
		FASTGLTF_FG_PMR_NS::MaybeSmallVector<Primitive, 2> primitives;
	    FASTGLTF_FG_PMR_NS::MaybeSmallVector<num> weights;

        FASTGLTF_STD_PMR_NS::string name;
		bool operator==(const Mesh& other) const {
			FASTGLTF_REQUIRE(primitives == other.primitives);
			FASTGLTF_REQUIRE(weights == other.weights);
			FASTGLTF_REQUIRE(name == other.name);
			return true;
		}
    };

    /**
     * Texture transform information as per KHR_texture_transform.
     */
    FASTGLTF_EXPORT struct TextureTransform {
        /**
         * Rotate the UVs by this many radians counter-clockwise around the origin. This is equivalent to a similar rotation of the image clockwise.
         */
        num rotation;

		/**
		 * The offset of the UV coordinate origin as a factor of the texture dimensions.
		 */
		math::nvec2 uvOffset = math::nvec2(0);

		/**
		 * The scale factor applied to the components of the UV coordinates.
		 */
		math::nvec2 uvScale = math::nvec2(1);

        /**
         * Overrides the textureInfo texCoord value if supplied.
         */
        Optional<std::size_t> texCoordIndex;

		bool operator==(const TextureTransform& other) const {
			FASTGLTF_REQUIRE(rotation == other.rotation);
			FASTGLTF_REQUIRE(uvOffset == other.uvOffset);
			FASTGLTF_REQUIRE(uvScale == other.uvScale);
			return true;
		}
    };

    FASTGLTF_EXPORT struct TextureInfo {
        std::size_t textureIndex;
        std::size_t texCoordIndex = 0;

        /**
         * Data from KHR_texture_transform, and nullptr if the extension wasn't enabled or used.
         */
        std::unique_ptr<TextureTransform> transform;

		bool operator==(const TextureInfo& other) const {
			FASTGLTF_REQUIRE(textureIndex == other.textureIndex);
			FASTGLTF_REQUIRE(texCoordIndex == other.texCoordIndex);
			FASTGLTF_REQUIRE(compareUniquePtr(transform, other.transform));
			return true;
		}
    };

	FASTGLTF_EXPORT struct NormalTextureInfo : TextureInfo {
		num scale = 1.f;

		bool operator==(const NormalTextureInfo& other) const {
			FASTGLTF_REQUIRE(scale == other.scale);
			FASTGLTF_REQUIRE(TextureInfo::operator==(other));
			return true;
		}
	};

	FASTGLTF_EXPORT struct OcclusionTextureInfo : TextureInfo {
		num strength = 1.f;

		bool operator==(const OcclusionTextureInfo& other) const {
			FASTGLTF_REQUIRE(strength == other.strength);
			FASTGLTF_REQUIRE(TextureInfo::operator==(other));
			return true;
		}
	};

    FASTGLTF_EXPORT struct PBRData {
		/**
		 * The factors for the base color of then material.
		 */
		math::nvec4 baseColorFactor = math::nvec4(1);

        /**
         * The factor for the metalness of the material.
         */
        num metallicFactor = 1.0f;

        /**
         * The factor for the roughness of the material.
         */
        num roughnessFactor = 1.0f;

        Optional<TextureInfo> baseColorTexture;
        Optional<TextureInfo> metallicRoughnessTexture;

		bool operator==(const PBRData& other) const {
			FASTGLTF_REQUIRE(baseColorFactor == other.baseColorFactor);
			FASTGLTF_REQUIRE(metallicFactor == other.metallicFactor);
			FASTGLTF_REQUIRE(roughnessFactor == other.roughnessFactor);
			FASTGLTF_REQUIRE(baseColorTexture == other.baseColorTexture);
			FASTGLTF_REQUIRE(metallicRoughnessTexture == other.metallicRoughnessTexture);
			return true;
		}
    };

	FASTGLTF_EXPORT struct MaterialAnisotropy {
		num anisotropyStrength = 0.0f;
		num anisotropyRotation = 0.0f;
		Optional<TextureInfo> anisotropyTexture;

		bool operator==(const MaterialAnisotropy& other) const {
			FASTGLTF_REQUIRE(anisotropyStrength == other.anisotropyStrength);
			FASTGLTF_REQUIRE(anisotropyRotation == other.anisotropyRotation);
			FASTGLTF_REQUIRE(anisotropyTexture == other.anisotropyTexture);
			return true;
		}
	};

	/**
	 * Diffuse transmission information from KHR_materials_diffuse_transmission
	 */
	FASTGLTF_EXPORT struct MaterialDiffuseTransmission {
		num diffuseTransmissionFactor = 0.0f;
		Optional<TextureInfo> diffuseTransmissionTexture;
		math::nvec3 diffuseTransmissionColorFactor = math::nvec3(1);
		Optional<TextureInfo> diffuseTransmissionColorTexture;

		bool operator==(const MaterialDiffuseTransmission& other) const {
			FASTGLTF_REQUIRE(diffuseTransmissionFactor == other.diffuseTransmissionFactor);
			FASTGLTF_REQUIRE(diffuseTransmissionTexture == other.diffuseTransmissionTexture);
			FASTGLTF_REQUIRE(diffuseTransmissionColorFactor == other.diffuseTransmissionColorFactor);
			FASTGLTF_REQUIRE(diffuseTransmissionColorTexture == other.diffuseTransmissionColorTexture);
			return true;
		}
	};

    /**
     * Specular information from KHR_materials_specular.
     */
    FASTGLTF_EXPORT struct MaterialSpecular {
        num specularFactor = 1.0f;
        Optional<TextureInfo> specularTexture;
		math::nvec3 specularColorFactor = math::nvec3(1);
        Optional<TextureInfo> specularColorTexture;

		bool operator==(const MaterialSpecular& other) const {
			FASTGLTF_REQUIRE(specularFactor == other.specularFactor);
			FASTGLTF_REQUIRE(specularColorFactor == other.specularColorFactor);
			FASTGLTF_REQUIRE(specularTexture == other.specularTexture);
			FASTGLTF_REQUIRE(specularColorTexture == other.specularColorTexture);
			return true;
		}
	};

    /**
     * Iridescence information from KHR_materials_iridescence
     */
    FASTGLTF_EXPORT struct MaterialIridescence {
        num iridescenceFactor = 0.0f;
        Optional<TextureInfo> iridescenceTexture;
        num iridescenceIor = 1.3f;
        num iridescenceThicknessMinimum = 100.0f;
        num iridescenceThicknessMaximum = 400.0f;
        Optional<TextureInfo> iridescenceThicknessTexture;

		bool operator==(const MaterialIridescence& other) const {
			FASTGLTF_REQUIRE(iridescenceFactor == other.iridescenceFactor);
			FASTGLTF_REQUIRE(iridescenceIor == other.iridescenceIor);
			FASTGLTF_REQUIRE(iridescenceThicknessMinimum == other.iridescenceThicknessMinimum);
			FASTGLTF_REQUIRE(iridescenceThicknessMaximum == other.iridescenceThicknessMaximum);
			FASTGLTF_REQUIRE(iridescenceTexture == other.iridescenceTexture);
			FASTGLTF_REQUIRE(iridescenceThicknessTexture == other.iridescenceThicknessTexture);
			return true;
		}
    };

    /**
     * Volume information from KHR_materials_volume
     */
    FASTGLTF_EXPORT struct MaterialVolume {
        num thicknessFactor = 0.0f;
        Optional<TextureInfo> thicknessTexture;
        num attenuationDistance = std::numeric_limits<num>::infinity();
		math::nvec3 attenuationColor = math::nvec3(1);

		bool operator==(const MaterialVolume& other) const {
			FASTGLTF_REQUIRE(thicknessFactor == other.thicknessFactor);
			FASTGLTF_REQUIRE(attenuationColor == other.attenuationColor);
			FASTGLTF_REQUIRE(thicknessTexture == other.thicknessTexture);
			FASTGLTF_REQUIRE(attenuationDistance == other.attenuationDistance);
			return true;
		}
    };

    FASTGLTF_EXPORT struct MaterialTransmission {
        num transmissionFactor = 0.0f;
        Optional<TextureInfo> transmissionTexture;

		bool operator==(const MaterialTransmission& other) const {
			FASTGLTF_REQUIRE(transmissionFactor == other.transmissionFactor);
			FASTGLTF_REQUIRE(transmissionTexture == other.transmissionTexture);
			return true;
		}
    };

    FASTGLTF_EXPORT struct MaterialClearcoat {
        num clearcoatFactor = 0.0f;
        Optional<TextureInfo> clearcoatTexture;
        num clearcoatRoughnessFactor = 0.0f;
        Optional<TextureInfo> clearcoatRoughnessTexture;
        Optional<NormalTextureInfo> clearcoatNormalTexture;

		bool operator==(const MaterialClearcoat& other) const {
			FASTGLTF_REQUIRE(clearcoatFactor == other.clearcoatFactor);
			FASTGLTF_REQUIRE(clearcoatRoughnessFactor == other.clearcoatRoughnessFactor);
			FASTGLTF_REQUIRE(clearcoatTexture == other.clearcoatTexture);
			FASTGLTF_REQUIRE(clearcoatNormalTexture == other.clearcoatNormalTexture);
			FASTGLTF_REQUIRE(clearcoatRoughnessTexture == other.clearcoatRoughnessTexture);
			return true;
		}
	};

    FASTGLTF_EXPORT struct MaterialSheen {
		math::nvec3 sheenColorFactor = math::nvec3(0);
        Optional<TextureInfo> sheenColorTexture;
        num sheenRoughnessFactor = 0.0f;
        Optional<TextureInfo> sheenRoughnessTexture;

		bool operator==(const MaterialSheen& other) const {
			FASTGLTF_REQUIRE(sheenColorFactor == other.sheenColorFactor);
			FASTGLTF_REQUIRE(sheenColorTexture == other.sheenColorTexture);
			FASTGLTF_REQUIRE(sheenRoughnessFactor == other.sheenRoughnessFactor);
			FASTGLTF_REQUIRE(sheenRoughnessTexture == other.sheenRoughnessTexture);
			return true;
		}
    };

#if FASTGLTF_ENABLE_DEPRECATED_EXT
    /**
     * Specular/Glossiness information from KHR_materials_pbrSpecularGlossiness.
     */
    FASTGLTF_EXPORT struct MaterialSpecularGlossiness {
		math::nvec4 diffuseFactor = math::nvec4(1);
        Optional<TextureInfo> diffuseTexture;
		math::nvec3 specularFactor = math::nvec3(1);
        num glossinessFactor = 1.0f;
        Optional<TextureInfo> specularGlossinessTexture;

		bool operator==(const MaterialSpecularGlossiness& other) const {
			FASTGLTF_REQUIRE(diffuseFactor == other.diffuseFactor);
			FASTGLTF_REQUIRE(diffuseTexture == other.diffuseTexture);
			FASTGLTF_REQUIRE(specularFactor == other.specularFactor);
			FASTGLTF_REQUIRE(glossinessFactor == other.glossinessFactor);
			FASTGLTF_REQUIRE(specularGlossinessTexture == other.specularGlossinessTexture);
			return true;
		}
    };
#endif

	FASTGLTF_EXPORT struct MaterialPackedTextures {
		Optional<TextureInfo> occlusionRoughnessMetallicTexture;
		Optional<TextureInfo> roughnessMetallicOcclusionTexture;
		Optional<TextureInfo> normalTexture;

		bool operator==(const MaterialPackedTextures& other) const {
			FASTGLTF_REQUIRE(occlusionRoughnessMetallicTexture == other.occlusionRoughnessMetallicTexture);
			FASTGLTF_REQUIRE(roughnessMetallicOcclusionTexture == other.roughnessMetallicOcclusionTexture);
			FASTGLTF_REQUIRE(normalTexture == other.normalTexture);
			return true;
		}
	};

    FASTGLTF_EXPORT struct Material {
        /**
         * A set of parameter values that are used to define the metallic-roughness material model
         * from Physically Based Rendering (PBR) methodology.
         */
        PBRData pbrData;

        /**
         * The tangent space normal texture.
         */
        Optional<NormalTextureInfo> normalTexture;
        Optional<OcclusionTextureInfo> occlusionTexture;
        Optional<TextureInfo> emissiveTexture;

		/**
		 * The factors for the emissive color of the material.
		 */
		math::nvec3 emissiveFactor = math::nvec3(0);

        /**
         * The values used to determine the transparency of the material.
         */
        AlphaMode alphaMode = AlphaMode::Opaque;

		/**
		 * Determines whether back-face culling should be disabled when using this material.
		 */
		bool doubleSided = false;

		/**
		 * Only true when KHR_materials_unlit is enabled and specified for this material.
		 */
		bool unlit = false;

		/**
		 * The alpha value that determines the upper limit for fragments that
		 * should be discarded for transparency.
		 */
        num alphaCutoff = 0.5f;

		/**
		 * The emissive strength from the KHR_materials_emissive_strength extension.
		 */
		num emissiveStrength = 1.0f;

		/**
		 * The index of refraction as specified through KHR_materials_ior.
		 */
		num ior = 1.5f;

		/**
		 * The dispersion factor from KHR_materials_dispersion, specifies as 20/Abbe number (20/V).
		 */
		num dispersion = 0.0f;

		std::unique_ptr<MaterialAnisotropy> anisotropy;

        std::unique_ptr<MaterialClearcoat> clearcoat;

		/**
		 * Diffuse transmission information from KHR_materials_diffuse_transmission.
		 */
		std::unique_ptr<MaterialDiffuseTransmission> diffuseTransmission;

        /**
         * Iridescence information from KHR_materials_iridescence.
         */
        std::unique_ptr<MaterialIridescence> iridescence;

        std::unique_ptr<MaterialSheen> sheen;

        /**
         * Specular information from KHR_materials_specular.
         */
        std::unique_ptr<MaterialSpecular> specular;

#if FASTGLTF_ENABLE_DEPRECATED_EXT
        /**
         * Specular/Glossiness information from KHR_materials_pbrSpecularGlossiness.
         */
        std::unique_ptr<MaterialSpecularGlossiness> specularGlossiness;
#endif

        /**
         * Specular information from KHR_materials_transmission.
         */
        std::unique_ptr<MaterialTransmission> transmission;

        /**
         * Volume information from KHR_materials_volume
         */
        std::unique_ptr<MaterialVolume> volume;

		/**
		 * The index of a packed texture from the MSFT_packing_normalRoughnessMetallic extension,
		 * providing normal, roughness and metallic data.
		 */
		Optional<TextureInfo> packedNormalMetallicRoughnessTexture;

		std::unique_ptr<MaterialPackedTextures> packedOcclusionRoughnessMetallicTextures;

        FASTGLTF_STD_PMR_NS::string name;

		bool operator==(const Material& other) const {
			// compare all members, using compareUniquePtr for unique_ptr members
			FASTGLTF_REQUIRE(pbrData == other.pbrData);
			FASTGLTF_REQUIRE(normalTexture == other.normalTexture);
			FASTGLTF_REQUIRE(occlusionTexture == other.occlusionTexture);
			FASTGLTF_REQUIRE(emissiveTexture == other.emissiveTexture);
			FASTGLTF_REQUIRE(emissiveFactor == other.emissiveFactor);
			FASTGLTF_REQUIRE(alphaMode == other.alphaMode);
			FASTGLTF_REQUIRE(doubleSided == other.doubleSided);
			FASTGLTF_REQUIRE(unlit == other.unlit);
			FASTGLTF_REQUIRE(alphaCutoff == other.alphaCutoff);
			FASTGLTF_REQUIRE(emissiveStrength == other.emissiveStrength);
			FASTGLTF_REQUIRE(ior == other.ior);
			FASTGLTF_REQUIRE(dispersion == other.dispersion);
			FASTGLTF_REQUIRE(compareUniquePtr(anisotropy, other.anisotropy));
			FASTGLTF_REQUIRE(compareUniquePtr(clearcoat, other.clearcoat));
			FASTGLTF_REQUIRE(compareUniquePtr(diffuseTransmission, other.diffuseTransmission));
			FASTGLTF_REQUIRE(compareUniquePtr(iridescence, other.iridescence));
			FASTGLTF_REQUIRE(compareUniquePtr(sheen, other.sheen));
			FASTGLTF_REQUIRE(compareUniquePtr(specular, other.specular));
			FASTGLTF_REQUIRE(compareUniquePtr(transmission, other.transmission));
			FASTGLTF_REQUIRE(compareUniquePtr(volume, other.volume));
			FASTGLTF_REQUIRE(packedNormalMetallicRoughnessTexture == other.packedNormalMetallicRoughnessTexture);
			FASTGLTF_REQUIRE(compareUniquePtr(packedOcclusionRoughnessMetallicTextures, other.packedOcclusionRoughnessMetallicTextures));
			FASTGLTF_REQUIRE(name == other.name);
			return true;
		}
    };

    FASTGLTF_EXPORT struct Texture {
		/**
		 * If no sampler is specified, use a default sampler with repeat wrap and auto filter.
		 */
		Optional<std::size_t> samplerIndex;

		/**
		 * The index of the image used by this texture. Either this will have a value,
		 * or one of the following extensions will define a texture index. If no extensions
		 * were enabled while parsing, this will always have a value.
		 */
		Optional<std::size_t> imageIndex;

		/**
		 * An optional texture index from the KHR_texture_basisu extension.
		 */
		Optional<std::size_t> basisuImageIndex;

		/**
		 * An optional texture index from the MSFT_texture_dds extension.
		 */
		Optional<std::size_t> ddsImageIndex;

		/**
		 * An optional texture index from the EXT_texture_webp extension.
		 */
		Optional<std::size_t> webpImageIndex;

        FASTGLTF_STD_PMR_NS::string name;

		bool operator==(const Texture& other) const {
			FASTGLTF_REQUIRE(samplerIndex == other.samplerIndex);
			FASTGLTF_REQUIRE(imageIndex == other.imageIndex);
			FASTGLTF_REQUIRE(basisuImageIndex == other.basisuImageIndex);
			FASTGLTF_REQUIRE(ddsImageIndex == other.ddsImageIndex);
			FASTGLTF_REQUIRE(webpImageIndex == other.webpImageIndex);
			FASTGLTF_REQUIRE(name == other.name);
			return true;
		}
    };

    FASTGLTF_EXPORT struct Image {
        DataSource data;

        FASTGLTF_STD_PMR_NS::string name;

		bool operator==(const Image& other) const {
			FASTGLTF_REQUIRE(data == other.data);
			FASTGLTF_REQUIRE(name == other.name);
			return true;
		}

    	bool equivalent(const Image& other) const
		{
			FASTGLTF_REQUIRE(name == other.name);
			FASTGLTF_REQUIRE(compareDataSource(	data, other.data, -1, true));
			return true;
		}
    };

    FASTGLTF_EXPORT struct SparseAccessor {
        std::size_t count;
        std::size_t indicesBufferView;
        std::size_t indicesByteOffset = 0;
        std::size_t valuesBufferView;
        std::size_t valuesByteOffset = 0;
        ComponentType indexComponentType;

		bool operator==(const SparseAccessor& other) const {
			FASTGLTF_REQUIRE(count == other.count);
			FASTGLTF_REQUIRE(indicesBufferView == other.indicesBufferView);
			FASTGLTF_REQUIRE(indicesByteOffset == other.indicesByteOffset);
			FASTGLTF_REQUIRE(valuesBufferView == other.valuesBufferView);
			FASTGLTF_REQUIRE(valuesByteOffset == other.valuesByteOffset);
			FASTGLTF_REQUIRE(indexComponentType == other.indexComponentType);
			return true;
		}
    };

	FASTGLTF_EXPORT struct Accessor {
		std::size_t byteOffset = 0;
		std::size_t count;
		AccessorType type;
		ComponentType componentType;
		bool normalized = false;

		std::optional<AccessorBoundsArray> max;
		std::optional<AccessorBoundsArray> min;

		// Could have no value for sparse morph targets
		Optional<std::size_t> bufferViewIndex;

		Optional<SparseAccessor> sparse;

		FASTGLTF_STD_PMR_NS::string name;

		/**
		 * Helper function that updates the max/min variables dynamically.
		 */
		template <typename T, std::enable_if_t<AccessorBoundsArray::is_valid_type_v<T>, bool> = true>
		void updateBoundsToInclude(T value) {
			if (!max)
				max = AccessorBoundsArray::ForType<T>(1);
			if (!min)
				min = AccessorBoundsArray::ForType<T>(1);

			assert(max->isType<T>() && min->isType<T>());
			assert(max->size() == 1 && min->size() == 1);

			const auto cur_max = max->get<T>(0);
			const auto cur_min = min->get<T>(0);
			if (value > cur_max)
				max->set<T>(0, value);
			if (value < cur_min)
				min->set<T>(0, value);
		}

		/**
		 * Helper function that updates the max/min variables dynamically. Note that the value passed in
		 * needs to be a vector with the same size as max/min
		 */
		template <typename T, std::size_t N, std::enable_if_t<AccessorBoundsArray::is_valid_type_v<T>, bool> = true>
		void updateBoundsToInclude(math::vec<T, N> value) {
			if (!max)
				max = AccessorBoundsArray::ForType<T>(value.size());
			if (!min)
				min = AccessorBoundsArray::ForType<T>(value.size());

			assert(max->isType<T>() && min->isType<T>());
			assert(max->size() == value.size() && min->size() == value.size());

			for (std::size_t i = 0; i < value.size(); ++i) {
				const auto cur_max = max->get<T>(i);
				const auto cur_min = min->get<T>(i);
				if (value[i] > cur_max)
					max->set<T>(i, value[i]);
				if (value[i] < cur_min)
					min->set<T>(i, value[i]);
			}
		}

		bool operator==(const Accessor& other) const {
			FASTGLTF_REQUIRE(byteOffset == other.byteOffset);
			FASTGLTF_REQUIRE(count == other.count);
			FASTGLTF_REQUIRE(type == other.type);
			FASTGLTF_REQUIRE(componentType == other.componentType);
			FASTGLTF_REQUIRE(normalized == other.normalized);
			FASTGLTF_REQUIRE(max == other.max);
			FASTGLTF_REQUIRE(min == other.min);
			FASTGLTF_REQUIRE(bufferViewIndex == other.bufferViewIndex);
			FASTGLTF_REQUIRE(sparse == other.sparse);
			FASTGLTF_REQUIRE(name == other.name);
			return true;
		}
	};

    FASTGLTF_EXPORT struct CompressedBufferView {
        std::size_t bufferIndex;
        std::size_t byteOffset;
        std::size_t byteLength;
        std::size_t count;
        MeshoptCompressionMode mode;
        MeshoptCompressionFilter filter;

        std::size_t byteStride;

		bool operator==(const CompressedBufferView& other) const {
			FASTGLTF_REQUIRE(bufferIndex == other.bufferIndex);
			FASTGLTF_REQUIRE(byteOffset == other.byteOffset);
			FASTGLTF_REQUIRE(byteLength == other.byteLength);
			FASTGLTF_REQUIRE(count == other.count);
			FASTGLTF_REQUIRE(mode == other.mode);
			FASTGLTF_REQUIRE(filter == other.filter);
			FASTGLTF_REQUIRE(byteStride == other.byteStride);
			return true;
		}
		
    };

    FASTGLTF_EXPORT struct BufferView {
        std::size_t bufferIndex;
        std::size_t byteOffset = 0;
        std::size_t byteLength;

        Optional<std::size_t> byteStride;
        Optional<BufferTarget> target;

        /**
         * Data from EXT_meshopt_compression, and nullptr if the extension was not enabled or used.
         */
        std::unique_ptr<CompressedBufferView> meshoptCompression;

        FASTGLTF_STD_PMR_NS::string name;

		bool operator==(const BufferView& other) const {
			FASTGLTF_REQUIRE(bufferIndex == other.bufferIndex);
			FASTGLTF_REQUIRE(byteOffset == other.byteOffset);
			FASTGLTF_REQUIRE(byteLength == other.byteLength);
			FASTGLTF_REQUIRE(byteStride == other.byteStride);
			FASTGLTF_REQUIRE(target == other.target);
			FASTGLTF_REQUIRE(compareUniquePtr(meshoptCompression, other.meshoptCompression));
			return true;
		}
    };

    FASTGLTF_EXPORT struct Buffer {
        std::size_t byteLength;

        DataSource data;

        FASTGLTF_STD_PMR_NS::string name;

		bool operator==(const Buffer& other) const {
			FASTGLTF_REQUIRE(byteLength == other.byteLength);
			FASTGLTF_REQUIRE(data == other.data);
			FASTGLTF_REQUIRE(name == other.name);
			return true;
		}

		// This is for the tests; rewritting a GLB file can change the length because of alignment padding
		bool equivalent(const Buffer& other) const {
			FASTGLTF_REQUIRE(fastgltf::alignUp(byteLength, 4) == fastgltf::alignUp(other.byteLength, 4));
			FASTGLTF_REQUIRE(name == other.name);
			// auto minSize = byteLength == other.byteLength ? -1 : std::min(byteLength, other.byteLength);
			auto minSize = std::min(byteLength, other.byteLength);

			auto &paddedBuf = byteLength > other.byteLength ? data : other.data;
			auto &unpaddedBuf = byteLength > other.byteLength ? other.data : data;
			FASTGLTF_REQUIRE(fastgltf::compareDataSource(paddedBuf, unpaddedBuf, minSize, true));
			return true;
		}

    };

    FASTGLTF_EXPORT struct Light {
        LightType type;
        /** RGB light color in linear space. */
        math::nvec3 color;

        /** Point and spot lights use candela (lm/sr) while directional use lux (lm/m^2) */
        num intensity;
        /** Range for point and spot lights. If not present, range is infinite. */
        Optional<num> range;

		/** The inner and outer cone angles only apply to spot lights */
        Optional<num> innerConeAngle;
        Optional<num> outerConeAngle;

        FASTGLTF_STD_PMR_NS::string name;

		bool operator==(const Light& other) const {
			FASTGLTF_REQUIRE(type == other.type);
			FASTGLTF_REQUIRE(color == other.color);
			FASTGLTF_REQUIRE(intensity == other.intensity);
			FASTGLTF_REQUIRE(range == other.range);
			FASTGLTF_REQUIRE(innerConeAngle == other.innerConeAngle);
			FASTGLTF_REQUIRE(outerConeAngle == other.outerConeAngle);
			FASTGLTF_REQUIRE(name == other.name);
			return true;
		}
    };

	class ChunkMemoryResource;
	FASTGLTF_EXPORT class Parser;

	FASTGLTF_EXPORT class Asset {
		friend class Parser;

#if !FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL
		// This has to be first in this struct so that it gets destroyed last, leaving all allocations
		// alive until the end.
		std::shared_ptr<std::pmr::monotonic_buffer_resource> memoryResource;
#endif

		bool nonBufferMembersEqual(const Asset& other) const {
			FASTGLTF_REQUIRE(assetInfo == other.assetInfo);
			FASTGLTF_REQUIRE(accessors == other.accessors);
			FASTGLTF_REQUIRE(animations == other.animations);
			FASTGLTF_REQUIRE(bufferViews == other.bufferViews);
			FASTGLTF_REQUIRE(cameras == other.cameras);
			FASTGLTF_REQUIRE(images == other.images);
			FASTGLTF_REQUIRE(lights == other.lights);
			FASTGLTF_REQUIRE(materials == other.materials);
			FASTGLTF_REQUIRE(meshes == other.meshes);
			FASTGLTF_REQUIRE(nodes == other.nodes);
			FASTGLTF_REQUIRE(samplers == other.samplers);
			FASTGLTF_REQUIRE(scenes == other.scenes);
			FASTGLTF_REQUIRE(skins == other.skins);	
			FASTGLTF_REQUIRE(textures == other.textures);
			FASTGLTF_REQUIRE(materialVariants == other.materialVariants);
			FASTGLTF_REQUIRE(availableCategories == other.availableCategories);
			FASTGLTF_REQUIRE(extensionsUsed == other.extensionsUsed);
			FASTGLTF_REQUIRE(extensionsRequired == other.extensionsRequired);
			FASTGLTF_REQUIRE(defaultScene == other.defaultScene);
		#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
			FASTGLTF_REQUIRE(physicsMaterials == other.physicsMaterials);
			FASTGLTF_REQUIRE(physicsJoints == other.physicsJoints);
			FASTGLTF_REQUIRE(collisionFilters == other.collisionFilters);
		#endif
			return true;
		}

	public:
        /**
         * This will only ever have no value if #Options::DontRequireValidAssetMember was specified.
         */
        Optional<AssetInfo> assetInfo;
		FASTGLTF_STD_PMR_NS::vector<FASTGLTF_STD_PMR_NS::string> extensionsUsed;
		FASTGLTF_STD_PMR_NS::vector<FASTGLTF_STD_PMR_NS::string> extensionsRequired;

        Optional<std::size_t> defaultScene;
        std::vector<Accessor> accessors;
        std::vector<Animation> animations;
        std::vector<Buffer> buffers;
        std::vector<BufferView> bufferViews;
        std::vector<Camera> cameras;
        std::vector<Image> images;
        std::vector<Light> lights;
        std::vector<Material> materials;
        std::vector<Mesh> meshes;
        std::vector<Node> nodes;
        std::vector<Sampler> samplers;
        std::vector<Scene> scenes;
        std::vector<Skin> skins;
        std::vector<Texture> textures;

		std::vector<std::string> materialVariants;

#if FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES
		std::vector<Shape> shapes;
#endif

#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
		std::vector<PhysicsMaterial> physicsMaterials;
	    std::vector<PhysicsJoint> physicsJoints;
		std::vector<CollisionFilter> collisionFilters;
#endif

        // Keeps tracked of categories that were actually parsed.
        Category availableCategories = Category::None;

        explicit Asset() = default;
        explicit Asset(const Asset& other) = delete;
        Asset(Asset&& other) noexcept :
#if !FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL
				memoryResource(std::move(other.memoryResource)),
#endif
				assetInfo(std::move(other.assetInfo)),
				extensionsUsed(std::move(other.extensionsUsed)),
				extensionsRequired(std::move(other.extensionsRequired)),
				defaultScene(other.defaultScene),
				accessors(std::move(other.accessors)),
				animations(std::move(other.animations)),
				buffers(std::move(other.buffers)),
				bufferViews(std::move(other.bufferViews)),
				cameras(std::move(other.cameras)),
				images(std::move(other.images)),
				lights(std::move(other.lights)),
				materials(std::move(other.materials)),
				meshes(std::move(other.meshes)),
				nodes(std::move(other.nodes)),
				samplers(std::move(other.samplers)),
				scenes(std::move(other.scenes)),
				skins(std::move(other.skins)),
				textures(std::move(other.textures)),
				materialVariants(std::move(other.materialVariants)),
#if FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES
			    shapes(std::move(other.shapes)),
#endif
#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
			    physicsMaterials(std::move(other.physicsMaterials)),
			    physicsJoints(std::move(other.physicsJoints)),
			    collisionFilters(std::move(other.collisionFilters)),
#endif
	            availableCategories(other.availableCategories) {}

		Asset& operator=(const Asset& other) = delete;
		Asset& operator=(Asset&& other) noexcept {
			assetInfo = std::move(other.assetInfo);
			extensionsUsed = std::move(other.extensionsUsed);
			extensionsRequired = std::move(other.extensionsRequired);
			defaultScene = other.defaultScene;
			accessors = std::move(other.accessors);
			animations = std::move(other.animations);
			buffers = std::move(other.buffers);
			bufferViews = std::move(other.bufferViews);
			cameras = std::move(other.cameras);
			images = std::move(other.images);
			lights = std::move(other.lights);
			materials = std::move(other.materials);
			meshes = std::move(other.meshes);
			nodes = std::move(other.nodes);
			samplers = std::move(other.samplers);
			scenes = std::move(other.scenes);
			skins = std::move(other.skins);
			textures = std::move(other.textures);
			materialVariants = std::move(other.materialVariants);
#if FASTGLTF_ENABLE_KHR_IMPLICIT_SHAPES
			shapes = std::move(other.shapes);
#endif
#if FASTGLTF_ENABLE_KHR_PHYSICS_RIGID_BODIES
			physicsMaterials = std::move(other.physicsMaterials);
			physicsJoints = std::move(other.physicsJoints);
			collisionFilters = std::move(other.collisionFilters);
#endif
			availableCategories = other.availableCategories;
#if !FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL
			// This needs to be last to not destroy the old memoryResource for the current data.
			memoryResource = std::move(other.memoryResource);
#endif
			return *this;
		}


		bool operator==(const Asset& other) const {
			// in order, every member
			FASTGLTF_REQUIRE(nonBufferMembersEqual(other));
			FASTGLTF_REQUIRE(buffers == other.buffers);
			return true;
		}

		bool equivalent(const Asset& other) const {
			FASTGLTF_REQUIRE(nonBufferMembersEqual(other));
			FASTGLTF_REQUIRE(buffers.size() == other.buffers.size());
			for (std::size_t i = 0; i < buffers.size(); ++i) {
				FASTGLTF_REQUIRE(buffers[i].equivalent(other.buffers[i]));
			}
			return true;
		}
    };
#pragma endregion
} // namespace fastgltf

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
