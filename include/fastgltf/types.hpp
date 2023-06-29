/*
 * Copyright (C) 2022 - 2023 spnda
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

#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <memory_resource>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

// Utils header already includes some headers, which we'll try and avoid including twice.
#include "util.hpp"

#if FASTGLTF_CPP_20
#include <span>
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5030) // attribute 'x' is not recognized
#pragma warning(disable : 4514) // unreferenced inline function has been removed
#endif

#define FASTGLTF_QUOTE_Q(x) #x
#define FASTGLTF_QUOTE(x) FASTGLTF_QUOTE_Q(x)

// fastgltf version string. Use FASTGLTF_QUOTE to stringify.
#define FASTGLTF_VERSION 0.5.0

namespace fastgltf {
#pragma region Enums
    // clang-format off
    enum class PrimitiveType : std::uint8_t {
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
     */
    enum class AccessorType : std::uint16_t {
        Invalid = 0,
        Scalar  = ( 1 << 8) | 1,
        Vec2    = ( 2 << 8) | 2,
        Vec3    = ( 3 << 8) | 3,
        Vec4    = ( 4 << 8) | 4,
        Mat2    = ( 4 << 8) | 5,
        Mat3    = ( 9 << 8) | 6,
        Mat4    = (16 << 8) | 7,
    };

    /**
     * Represents the various types of components an accessor could point at. This describes the
     * format each component of the structure, which in return is described by fastgltf::AccessorType, is in.
     *
     * We use the top 16-bits to encode the amount of bits this component type needs.
     * The lower 16-bits are used to store the glTF ID for the type. Therefore, use the fastgltf::getComponentBitSize
     * and fastgltf::getGLComponentType functions should be used to extract data from this enum.
     */
    enum class ComponentType : std::uint32_t {
        Invalid         = 0,
        Byte            = ( 8 << 16) | 5120,
        UnsignedByte    = ( 8 << 16) | 5121,
        Short           = (16 << 16) | 5122,
        UnsignedShort   = (16 << 16) | 5123,
        /**
         * Signed integers are not officially allowed by the glTF spec, but are placed here for
         * the sake of completeness.
         */
        Int             = (32 << 16) | 5124,
        UnsignedInt     = (32 << 16) | 5125,
        Float           = (32 << 16) | 5126,
        /**
         * Doubles are not officially allowed by the glTF spec, but can be enabled by passing
         * Options::AllowDouble if you require it.
         */
        Double          = (64 << 16) | 5130,
    };

    enum class Filter : std::uint16_t {
        Nearest = 9728,
        Linear = 9729,
        NearestMipMapNearest = 9984,
        LinearMipMapNearest = 9985,
        NearestMipMapLinear = 9986,
        LinearMipMapLinear = 9987,
    };

    enum class Wrap : std::uint16_t {
        ClampToEdge = 33071,
        MirroredRepeat = 33648,
        Repeat = 10497,
    };

    /**
     * Represents the intended OpenGL GPU buffer type to use with this buffer view.
     */
    enum class BufferTarget : std::uint16_t {
        ArrayBuffer = 34962,
        ElementArrayBuffer = 34963,
    };

    enum class MimeType : std::uint16_t {
        None = 0,
        JPEG = 1,
        PNG = 2,
        KTX2 = 3,
        DDS = 4,
        GltfBuffer = 5,
        OctetStream = 6,
    };

    enum class AnimationInterpolation : std::uint16_t {
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

    enum class AnimationPath : std::uint16_t {
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

    enum class CameraType : std::uint8_t {
        Perspective = 0,
        Orthographic = 1,
    };

    enum class AlphaMode : std::uint8_t {
        Opaque,
        Mask,
        Blend,
    };

    enum class MeshoptCompressionMode : std::uint8_t {
        None = 0,
        Attributes,
        Triangles,
        Indices,
    };

    enum class MeshoptCompressionFilter : std::uint8_t {
        None = 0,
        Octahedral,
        Quaternion,
        Exponential,
    };

    enum class LightType : std::uint8_t {
        Directional,
        Spot,
        Point,
    };

    enum class Category : std::uint32_t {
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

        All = ~(~0u << 14),
        // Includes everything needed for rendering but animations
        OnlyRenderable = All & ~(Animations) & ~(Skins),
        OnlyAnimations = Animations | Accessors | BufferViews | Buffers,
    };

    FASTGLTF_ARITHMETIC_OP_TEMPLATE_MACRO(Category, Category, |)
    FASTGLTF_ARITHMETIC_OP_TEMPLATE_MACRO(Category, Category, &)
    FASTGLTF_ASSIGNMENT_OP_TEMPLATE_MACRO(Category, Category, |)
    FASTGLTF_ASSIGNMENT_OP_TEMPLATE_MACRO(Category, Category, &)
    FASTGLTF_UNARY_OP_TEMPLATE_MACRO(Category, ~)
    // clang-format on
#pragma endregion

#pragma region ConversionFunctions
    /**
     * Gets the number of components for each element for the given accessor type. For example, with
     * a Vec3 accessor type this will return 3, as a Vec3 contains 3 components.
     */
    constexpr std::uint8_t getNumComponents(AccessorType type) noexcept {
        return (to_underlying(type) >> 8) & 0xFF;
    }

    constexpr std::uint16_t getComponentBitSize(ComponentType componentType) noexcept {
        auto masked = to_underlying(componentType) & 0xFFFF0000;
        return masked >> 16;
    }

    constexpr std::uint16_t getElementByteSize(AccessorType type, ComponentType componentType) noexcept {
        return static_cast<std::uint16_t>(getNumComponents(type)) * (getComponentBitSize(componentType) / 8);
    }

    constexpr std::uint16_t getGLComponentType(ComponentType type) noexcept {
        return to_underlying(type) & 0xFFFF;
    }

    /**
     * Don't use this, use getComponenType instead.
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

    constexpr ComponentType getComponentType(std::underlying_type_t<ComponentType> componentType) noexcept {
        std::size_t index = componentType - 5120;
        if (index >= components.size())
            return ComponentType::Invalid;
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
    constexpr AccessorType getAccessorType(std::string_view accessorTypeName) noexcept {
        assert(!accessorTypeName.empty());
        switch (accessorTypeName[0]) {
            case 'S': return AccessorType::Scalar;
            case 'V': {
                auto componentCount = accessorTypeName[3] - '2';
                if (1ULL + componentCount >= accessorTypes.size())
                    return AccessorType::Invalid;
                return accessorTypes[1ULL + componentCount];
            }
            case 'M': {
                auto componentCount = accessorTypeName[3] - '2';
                if (4ULL + componentCount >= accessorTypes.size())
                    return AccessorType::Invalid;
                return accessorTypes[4ULL + componentCount];
            }
        }

        return AccessorType::Invalid;
    }
#pragma endregion

#pragma region Containers
	/*
	 * The amount of items that the SmallVector can initially store in the storage
	 * allocated within the object itself.
	 */
	static constexpr auto initialSmallVectorStorage = 8;

    /**
     * A custom small vector class for fastgltf, as there often only are 1-3 node children and mesh
     * primitives. Therefore, this is a quite basic implementation of a small vector which is mostly
     * standard (C++17) conforming.
     */
    template <typename T, std::size_t N = initialSmallVectorStorage, typename Allocator = std::allocator<T>>
    class SmallVector final {
        static_assert(N != 0, "Cannot create a SmallVector with 0 initial capacity");

        alignas(T) std::array<std::byte, N * sizeof(T)> storage;

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
            if (!isUsingStack() && _data) {
                // The stack data gets destructed automatically, but the heap data does not.
                std::destroy(begin(), end());

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

			// If we can use the objects memory again, we'll copy everything over.
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
            T& result = *(new (std::addressof(back())) T(std::forward<Args>(args)...));
            return (result);
        }

        [[nodiscard]] T& at(std::size_t idx) {
            if (idx >= size()) {
                throw std::out_of_range("Index is out of range for SmallVector");
            }
            return begin()[idx];
        }
        [[nodiscard]] const T& at(std::size_t idx) const {
            if (idx >= size()) {
                throw std::out_of_range("Index is out of range for SmallVector");
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
    };

	namespace pmr {
		template<typename T, std::size_t N>
		using SmallVector = SmallVector<T, N, std::pmr::polymorphic_allocator<T>>;
	} // namespace pmr

#ifndef FASTGLTF_USE_CUSTOM_SMALLVECTOR
#define FASTGLTF_USE_CUSTOM_SMALLVECTOR 0
#endif

#if FASTGLTF_USE_CUSTOM_SMALLVECTOR
	template <typename T, std::size_t N = initialSmallVectorStorage>
	using MaybeSmallVector = SmallVector<T, N>;
#else
	template <typename T, std::size_t N = 0>
	using MaybeSmallVector = std::vector<T>;
#endif

	namespace pmr {
#if FASTGLTF_USE_CUSTOM_SMALLVECTOR
		template <typename T, std::size_t N = initialSmallVectorStorage>
		using MaybeSmallVector = pmr::SmallVector<T, N>;
#else
		template <typename T, std::size_t N = 0>
		using MaybeSmallVector = std::pmr::vector<T>;
#endif
	} // namespace pmr
#pragma endregion

#pragma region Structs
	class glTF;
	class URI;

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
	class URIView {
		friend class glTF;
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
		friend class glTF;

		std::pmr::string uri;
		URIView view;

		void readjustViews(const URIView& other);

	public:
		explicit URI() noexcept;

		explicit URI(std::string uri) noexcept;
		explicit URI(std::pmr::string uri) noexcept;
		explicit URI(std::string_view uri) noexcept;
		explicit URI(URIView view) noexcept;

		URI(const URI& other);
		URI(URI&& other) noexcept;

		URI& operator=(const URI& other);
		URI& operator=(const URIView& other);
		URI& operator=(URI&& other) noexcept;

		operator URIView() const noexcept;

		static void decodePercents(std::pmr::string& x) noexcept;

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

    inline constexpr std::size_t dynamic_extent = std::numeric_limits<std::size_t>::max();

    /**
     * Custom span class imitating C++20's std::span for referencing bytes without owning the
     * allocation. Can also directly be converted to a std::span or used by itself.
     */
    template <typename T, std::size_t Extent = dynamic_extent>
    class span {
        using element_type = T;
        using value_type = std::remove_cv_t<T>;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;

        pointer _ptr = nullptr;
        size_type _size = 0;

    public:
        constexpr span() noexcept = default;

        template <typename Iterator>
        explicit constexpr span(Iterator first, size_type count) : _ptr(first), _size(count) {}

        constexpr span(const span& other) noexcept = default;
        constexpr span& operator=(const span& other) noexcept = default;

        [[nodiscard]] constexpr reference operator[](size_type idx) const {
            return data()[idx];
        }

        [[nodiscard]] constexpr pointer data() const noexcept {
            return _ptr;
        }

        [[nodiscard]] constexpr size_type size() const noexcept {
            return _size;
        }

        [[nodiscard]] constexpr size_type size_bytes() const noexcept {
            return size() * sizeof(element_type);
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return size() == 0;
        }

        [[nodiscard]] constexpr span<T, Extent> first(size_type count) const {
            return span(_ptr, count);
        }

#if FASTGLTF_CPP_20
        operator std::span<T>() const {
            return std::span(data(), size());
        }
#endif
    };

    using CustomBufferId = std::uint64_t;

    /**
     * Namespace for structs that describe individual sources of data for images and/or buffers.
     */
    namespace sources {
        struct BufferView {
            std::size_t bufferViewIndex;
            MimeType mimeType;
        };

        struct URI {
            std::size_t fileByteOffset;
            fastgltf::URI uri;
            MimeType mimeType;
        };

        struct Vector {
            std::vector<std::uint8_t> bytes;
            MimeType mimeType;
        };

        struct CustomBuffer {
            CustomBufferId id;
            MimeType mimeType;
        };

        struct ByteView {
            span<const std::byte> bytes;
            MimeType mimeType;
        };
    } // namespace sources

    /**
     * Represents the data source of a buffer or image. These could be a buffer view, a file path
     * (including offsets), a ordinary vector (if Options::LoadExternalBuffers or Options::LoadGLBBuffers
     * was specified), or the ID of a custom buffer. Note that you, as a user, should never encounter
     * this variant holding the std::monostate, as that would be a ill-formed glTF, which fastgltf
     * already checks for while parsing. Note that for buffers, this variant will never hold a BufferView,
     * as only images are able to reference buffer views as a source.
     */
    using DataSource = std::variant<std::monostate, sources::BufferView, sources::URI, sources::Vector, sources::CustomBuffer, sources::ByteView>;

    struct AnimationChannel {
        std::size_t samplerIndex;
        std::size_t nodeIndex;
        AnimationPath path;
    };

    struct AnimationSampler {
        std::size_t inputAccessor;
        std::size_t outputAccessor;
        AnimationInterpolation interpolation;
    };

    struct Animation {
	    pmr::MaybeSmallVector<AnimationChannel> channels;
	    pmr::MaybeSmallVector<AnimationSampler> samplers;

        std::pmr::string name;
    };

    struct AssetInfo {
        std::pmr::string gltfVersion;
        std::pmr::string copyright;
        std::pmr::string generator;
    };

    struct Camera {
        struct Orthographic {
            float xmag;
            float ymag;
            float zfar;
            float znear;
        };
        struct Perspective {
            std::optional<float> aspectRatio;
            float yfov;
            // If omitted, use an infinite projection matrix.
            std::optional<float> zfar;
            float znear;
        };

        /**
         * Variant holding either a perspective or a orthographic camera. Use std::holds_alternative
         * and/or std::get_if to figure out which camera type is being used.
         */
        std::variant<Perspective, Orthographic> camera;
        std::pmr::string name;
    };

    struct Skin {
	    pmr::MaybeSmallVector<std::size_t> joints;
        std::optional<std::size_t> skeleton;
        std::optional<std::size_t> inverseBindMatrices;

        std::pmr::string name;
    };

    struct Sampler {
        std::optional<Filter> magFilter;
        std::optional<Filter> minFilter;
        Wrap wrapS;
        Wrap wrapT;

        std::pmr::string name;
    };

    struct Scene {
	    pmr::MaybeSmallVector<std::size_t> nodeIndices;

        std::pmr::string name;
    };

    struct Node {
        std::optional<std::size_t> meshIndex;
        std::optional<std::size_t> skinIndex;
        std::optional<std::size_t> cameraIndex;

        /**
         * Only ever empty when KHR_lights_punctual is enabled and used by the asset.
         */
        std::optional<std::size_t> lightsIndex;

	    pmr::MaybeSmallVector<std::size_t> children;
	    pmr::MaybeSmallVector<float> weights;

        struct TRS {
            std::array<float, 3> translation;
            std::array<float, 4> rotation;
            std::array<float, 3> scale;
        };
        using TransformMatrix = std::array<float, 16>;

        /**
         * Variant holding either the three TRS components; transform, rotation, and scale, or a
         * transformation matrix, which cannot skew or shear. The latter can be decomposed into
         * the TRS components by specifying Options::DecomposeNodeMatrices.
         */
        std::variant<TRS, TransformMatrix> transform;

        std::pmr::string name;
    };

    struct Primitive {
		using attribute_type = std::pair<std::pmr::string, std::size_t>;

		// Instead of a map, we have a list of attributes here. Each pair contains
		// the name of the attribute and the corresponding accessor index.
		pmr::SmallVector<attribute_type, 4> attributes;
        PrimitiveType type;

        std::pmr::vector<pmr::SmallVector<attribute_type, 4>> targets;

        std::optional<std::size_t> indicesAccessor;
        std::optional<std::size_t> materialIndex;

		[[nodiscard]] auto findAttribute(std::string_view name) {
			for (decltype(attributes)::iterator it = attributes.begin(); it != attributes.end(); ++it) {
				if (it->first == name)
					return it;
			}
			return attributes.end();
		}

		[[nodiscard]] auto findAttribute(std::string_view name) const {
			for (decltype(attributes)::const_iterator it = attributes.cbegin(); it != attributes.cend(); ++it) {
				if (it->first == name)
					return it;
			}
			return attributes.cend();
		}

		[[nodiscard]] auto findTargetAttribute(std::size_t targetIndex, std::string_view name) {
			auto& targetAttributes = targets[targetIndex];
			for (std::remove_reference_t<decltype(targetAttributes)>::iterator it = targetAttributes.begin(); it != targetAttributes.end(); ++it) {
				if (it->first == name)
					return it;
			}
			return targetAttributes.end();
		}

		[[nodiscard]] auto findTargetAttribute(std::size_t targetIndex, std::string_view name) const {
			const auto& targetAttributes = targets[targetIndex];
			for (std::remove_reference_t<decltype(targetAttributes)>::const_iterator it = targetAttributes.cbegin(); it != targetAttributes.cend(); ++it) {
				if (it->first == name)
					return it;
			}
			return targetAttributes.cend();
		}
	};

    struct Mesh {
		pmr::MaybeSmallVector<Primitive, 2> primitives;
		pmr::MaybeSmallVector<float> weights;

        std::pmr::string name;
    };

    /**
     * Texture transform information as per KHR_texture_transform.
     */
    struct TextureTransform {
        /**
         * The offset of the UV coordinate origin as a factor of the texture dimensions.
         */
        float rotation;

        /**
         * Rotate the UVs by this many radians counter-clockwise around the origin. This is equivalent to a similar rotation of the image clockwise.
         */
        std::array<float, 2> uvOffset;

        /**
         * The scale factor applied to the components of the UV coordinates.
         */
        std::array<float, 2> uvScale;

        /**
         * Overrides the textureInfo texCoord value if supplied.
         */
        std::optional<std::size_t> texCoordIndex;
    };

    struct TextureInfo {
        std::size_t textureIndex;
        std::size_t texCoordIndex;
        float scale;

        /**
         * Data from KHR_texture_transform, and nullptr if the extension wasn't enabled or used.
         */
        std::unique_ptr<TextureTransform> transform;
    };

    struct PBRData {
        /**
         * The factors for the base color of then material. Defaults to 1,1,1,1
         */
        std::array<float, 4> baseColorFactor;

        /**
         * The factor fot eh metalness of the material. Defaults to 1
         */
        float metallicFactor;

        /**
         * The factor fot eh roughness of the material. Defaults to 1
         */
        float roughnessFactor;

        std::optional<TextureInfo> baseColorTexture;
        std::optional<TextureInfo> metallicRoughnessTexture;
    };

    /**
     * Specular information from KHR_materials_specular.
     */
    struct MaterialSpecular {
        float specularFactor;
        std::optional<TextureInfo> specularTexture;
        std::array<float, 3> specularColorFactor;
        std::optional<TextureInfo> specularColorTexture;
    };

    /**
     * Iridescence information from KHR_materials_iridescence
     */
    struct MaterialIridescence {
        float iridescenceFactor;
        std::optional<TextureInfo> iridescenceTexture;
        float iridescenceIor;
        float iridescenceThicknessMinimum;
        float iridescenceThicknessMaximum;
        std::optional<TextureInfo> iridescenceThicknessTexture;
    };

    /**
     * Volume information from KHR_materials_volume
     */
    struct MaterialVolume {
        float thicknessFactor;
        std::optional<TextureInfo> thicknessTexture;
        float attenuationDistance;
        std::array<float, 3> attenuationColor;
    };

    struct MaterialTransmission {
        float transmissionFactor;
        std::optional<TextureInfo> transmissionTexture;
    };

    struct MaterialClearcoat {
        float clearcoatFactor;
        std::optional<TextureInfo> clearcoatTexture;
        float clearcoatRoughnessFactor;
        std::optional<TextureInfo> clearcoatRoughnessTexture;
        std::optional<TextureInfo> clearcoatNormalTexture;
    };

    struct MaterialSheen {
        std::array<float, 3> sheenColorFactor;
        std::optional<TextureInfo> sheenColorTexture;
        float sheenRoughnessFactor;
        std::optional<TextureInfo> sheenRoughnessTexture;
    };

    struct Material {
        /**
         * A set of parameter values that are used to define the metallic-roughness material model
         * from Physically Based Rendering (PBR) methodology. When undefined, all the default
         * values of pbrMetallicRoughness MUST apply.
         */
        std::optional<PBRData> pbrData;

        /**
         * The tangent space normal texture.
         */
        std::optional<TextureInfo> normalTexture;
        std::optional<TextureInfo> occlusionTexture;
        std::optional<TextureInfo> emissiveTexture;

        /**
         * The factors for the emissive color of the material. Defaults to 0,0,0
         */
        std::array<float, 3> emissiveFactor;

        /**
         * The values used to determine the transparency of the material.
         * Defaults to Opaque, and 0.5 for alpha cutoff.
         */
        AlphaMode alphaMode;
        float alphaCutoff;

        /**
         * Determines whether back-face culling should be disabled when using this material.
         */
        bool doubleSided;

        std::unique_ptr<MaterialClearcoat> clearcoat;

        /**
         * Iridescence information from KHR_materials_iridescence.
         */
        std::unique_ptr<MaterialIridescence> iridescence;

        std::unique_ptr<MaterialSheen> sheen;

        /**
         * Specular information from KHR_materials_specular.
         */
        std::unique_ptr<MaterialSpecular> specular;

        /**
         * Specular information from KHR_materials_transmission.
         */
        std::unique_ptr<MaterialTransmission> transmission;

        /**
         * Volume information from KHR_materials_volume
         */
        std::unique_ptr<MaterialVolume> volume;

        /**
         * The emissive strength from the KHR_materials_emissive_strength extension.
         */
        std::optional<float> emissiveStrength;

        /**
         * The index of refraction as specified through KHR_materials_ior.
         */
        std::optional<float> ior;

        /**
         * Only applicable if KHR_materials_unlit is enabled.
         */
        bool unlit;

        std::pmr::string name;
    };

    struct Texture {
        /**
         * When empty, an extension or other mechanism SHOULD supply an alternate texture source,
         * otherwise behavior is undefined.
         */
        std::optional<std::size_t> imageIndex;

        /**
         * If the imageIndex is specified by the KTX2 or DDS glTF extensions, this is supposed to
         * be used as a fallback if those file containers are not supported.
         */
        std::optional<std::size_t> fallbackImageIndex;

        /**
         * If no sampler is specified, use a default sampler with repeat wrap and auto filter.
         */
        std::optional<std::size_t> samplerIndex;

        std::pmr::string name;
    };

    struct Image {
        DataSource data;

        std::pmr::string name;
    };

    struct SparseAccessor {
        std::size_t count;
        std::size_t indicesBufferView;
        std::size_t indicesByteOffset;
        std::size_t valuesBufferView;
        std::size_t valuesByteOffset;
        ComponentType indexComponentType;
    };

    struct Accessor {
        std::size_t byteOffset;
        std::size_t count;
        AccessorType type;
        ComponentType componentType;
        bool normalized;
        
        std::variant<std::monostate, std::pmr::vector<double>, std::pmr::vector<std::int64_t>> max;
        std::variant<std::monostate, std::pmr::vector<double>, std::pmr::vector<std::int64_t>> min;

        // Could have no value for sparse morph targets
        std::optional<std::size_t> bufferViewIndex;

        std::optional<SparseAccessor> sparse;

        std::pmr::string name;
    };

    struct CompressedBufferView {
        std::size_t bufferIndex;
        std::size_t byteOffset;
        std::size_t byteLength;
        std::size_t count;
        MeshoptCompressionMode mode;
        MeshoptCompressionFilter filter;

        std::size_t byteStride;
    };

    struct BufferView {
        std::size_t bufferIndex;
        std::size_t byteOffset;
        std::size_t byteLength;

        std::optional<std::size_t> byteStride;
        std::optional<BufferTarget> target;

        /**
         * Data from EXT_meshopt_compression, and nullptr if the extension was not enabled or used.
         */
        std::unique_ptr<CompressedBufferView> meshoptCompression;

        std::pmr::string name;
    };

    struct Buffer {
        std::size_t byteLength;

        DataSource data;

        std::pmr::string name;
    };

    struct Light {
        LightType type;
        /** RGB light color in linear space. */
        std::array<float, 3> color;

        /** Point and spot lights use candela (lm/sr) while directional use lux (lm/m^2) */
        float intensity;
        /** Range for point and spot lights. If not present, range is infinite. */
        std::optional<float> range;

        std::optional<float> innerConeAngle;
        std::optional<float> outerConeAngle;

        std::pmr::string name;
    };

    struct Asset {
        /**
         * This will only ever have no value if Options::DontRequireValidAssetMember was specified.
         */
        std::optional<AssetInfo> assetInfo;
        std::optional<std::size_t> defaultScene;
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

        // Keeps tracked of categories that were actually parsed.
        Category availableCategories = Category::None;

        explicit Asset() = default;
        explicit Asset(const Asset& scene) = delete;
        Asset& operator=(const Asset& scene) = delete;
    };
#pragma endregion
} // namespace fastgltf

#ifdef _MSC_VER
#pragma warning(pop)
#endif
