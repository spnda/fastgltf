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

#pragma once

#include <cmath>
#include <tuple>

#include <fastgltf/util.hpp>

/**
 * The fastgltf::math namespace contains all math functions and types which are needed for working with glTF assets.
 */
namespace fastgltf::math {
	inline constexpr double pi = 3.141592653589793116;

	template <typename T, std::size_t N, std::size_t M>
	class mat;

	template <typename T, std::size_t N>
	class vec {
		static_assert(N >= 2 && N <= 4);

		std::array<T, N> data;

	public:
		constexpr vec() noexcept : data() {}

		constexpr explicit vec(T value) noexcept {
			data.fill(value);
		}

		/** Creates a new vector with N components from N values in the order X, Y, Z, W */
		template <typename... Args, std::enable_if_t<sizeof...(Args) == N, bool> = true>
		constexpr explicit vec(Args... args) noexcept : data { T(std::forward<Args>(args))... } {}

		constexpr vec(const vec<T, N>& other) noexcept : data(other.data) {}
		constexpr vec<T, N>& operator=(const vec<T, N>& other) noexcept {
			data = other.data;
			return *this;
		}

		template <typename U, std::enable_if_t<!std::is_same_v<T, U>, bool> = true>
		constexpr explicit vec(const vec<U, N>& other) noexcept {
			for (std::size_t i = 0; i < N; ++i)
				(*this)[i] = static_cast<T>(other[i]);
		}
		template <typename U, std::enable_if_t<!std::is_same_v<T, U>, bool> = true>
		constexpr vec<T, N>& operator=(const vec<U, N>& other) noexcept {
			for (std::size_t i = 0; i < N; ++i)
				(*this)[i] = static_cast<T>(other[i]);
			return *this;
		}

		template <std::size_t M, std::enable_if_t<M >= N, bool> = true>
		constexpr explicit vec(const vec<T, M>& other) noexcept {
			for (std::size_t i = 0; i < N; ++i)
				(*this)[i] = other[i];
		}
		template <std::size_t M, std::enable_if_t<M >= N, bool> = true>
		constexpr vec<T, N>& operator=(const vec<T, M>& other) noexcept {
			for (std::size_t i = 0; i < N; ++i)
				(*this)[i] = other[i];
			return *this;
		}

		[[nodiscard]] constexpr std::size_t size() const noexcept {
			return N;
		}
		[[nodiscard]] constexpr std::size_t size_bytes() const noexcept {
			return size() * sizeof(T);
		}

		[[nodiscard]] constexpr decltype(auto) operator[](std::size_t idx) noexcept {
			return data[idx];
		}
		[[nodiscard]] constexpr decltype(auto) operator[](std::size_t idx) const noexcept {
			return data[idx];
		}

		[[nodiscard]] constexpr decltype(auto) x() noexcept {
			return data[0];
		}
		[[nodiscard]] constexpr decltype(auto) y() noexcept {
			return data[1];
		}
		[[nodiscard]] constexpr decltype(auto) z() noexcept {
			static_assert(N >= 3);
			return data[2];
		}
		[[nodiscard]] constexpr decltype(auto) w() noexcept {
			static_assert(N >= 4);
			return data[3];
		}
		[[nodiscard]] constexpr decltype(auto) x() const noexcept {
			return data[0];
		}
		[[nodiscard]] constexpr decltype(auto) y() const noexcept {
			return data[1];
		}
		[[nodiscard]] constexpr decltype(auto) z() const noexcept {
			static_assert(N >= 3);
			return data[2];
		}
		[[nodiscard]] constexpr decltype(auto) w() const noexcept {
			static_assert(N >= 4);
			return data[3];
		}

		[[nodiscard]] constexpr T* value_ptr() noexcept {
			return data.data();
		}
		[[nodiscard]] constexpr const T* value_ptr() const noexcept {
			return data.data();
		}

		[[nodiscard]] constexpr bool operator==(const vec<T, N>& other) const noexcept {
			for (std::size_t i = 0; i < N; ++i)
				if ((*this)[i] != other[i])
					return false;
			return true;
		}
		[[nodiscard]] constexpr bool operator!=(const vec<T, N>& other) const noexcept {
			return !(*this == other);
		}

		constexpr auto operator*(T scalar) const noexcept {
			vec<T, N> ret(T(0));
			for (std::size_t i = 0; i < N; ++i)
				ret[i] = (*this)[i] * scalar;
			return ret;
		}
		constexpr auto operator*=(T scalar) noexcept {
			for (std::size_t i = 0; i < N; ++i)
				(*this)[i] *= scalar;
			return *this;
		}

		constexpr auto operator/(T scalar) const noexcept {
			vec<T, N> ret(T(0));
			for (std::size_t i = 0; i < N; ++i)
				ret[i] = (*this)[i] / scalar;
			return ret;
		}
		constexpr auto operator/=(T scalar) noexcept {
			for (std::size_t i = 0; i < N; ++i)
				(*this)[i] /= scalar;
			return *this;
		}

		constexpr auto operator+(T scalar) const noexcept {
			vec<T, N> ret(T(0));
			for (std::size_t i = 0; i < N; ++i)
				ret[i] = (*this)[i] + scalar;
			return ret;
		}
		constexpr auto operator+=(T scalar) noexcept {
			for (std::size_t i = 0; i < N; ++i)
				(*this)[i] += scalar;
			return *this;
		}
		constexpr auto operator+(const vec<T, N>& other) const noexcept {
			vec<T, N> ret(T(0));
			for (std::size_t i = 0; i < N; ++i)
				ret[i] = (*this)[i] + other[i];
			return ret;
		}
		constexpr auto operator+=(const vec<T, N>& other) noexcept {
			for (std::size_t i = 0; i < N; ++i)
				(*this)[i] += other[i];
			return *this;
		}

		constexpr auto operator-(T scalar) const noexcept {
			vec<T, N> ret(T(0));
			for (std::size_t i = 0; i < N; ++i)
				ret[i] = (*this)[i] - scalar;
			return ret;
		}
		constexpr auto operator-=(T scalar) noexcept {
			for (std::size_t i = 0; i < N; ++i)
				(*this)[i] -= scalar;
			return *this;
		}
		constexpr auto operator-(const vec<T, N>& other) const noexcept {
			vec<T, N> ret(T(0));
			for (std::size_t i = 0; i < N; ++i)
				ret[i] = (*this)[i] - other[i];
			return ret;
		}
		constexpr auto operator-=(const vec<T, N>& other) noexcept {
			for (std::size_t i = 0; i < N; ++i)
				(*this)[i] -= other[i];
			return *this;
		}

		template <std::size_t M, std::enable_if_t<M < N, bool> = true>
		constexpr operator vec<T, M>() const noexcept {
			vec<T, M> ret;
			for (std::size_t i = 0; i < M; ++i)
				ret[i] = (*this)[i];
			return ret;
		}
	};

	/** Computes the dot product of two vectors */
	template <typename T, std::size_t N>
	[[nodiscard]] auto dot(const vec<T, N>& a, const vec<T, N>& b) noexcept {
		T ret(0);
		for (std::size_t i = 0; i < N; ++i)
			ret += a[i] * b[i];
		return ret;
	}

	/** Computes the 3D cross product of two vectors */
	template <typename T>
	[[nodiscard]] auto cross(const vec<T, 3>& a, const vec<T, 3>& b) noexcept {
		return vec<T, 3>(
			a.y() * b.z() - a.z() * b.y(),
			a.z() * b.x() - a.x() * b.z(),
			a.x() * b.y() - a.y() * b.x()
		);
	}

	/** Computes the euclidean length of this vector */
	template <typename T, std::size_t N>
	[[nodiscard]] T length(const vec<T, N>& v) noexcept {
		return sqrt(dot(v, v));
	}

	/** Normalizes the vector to have a length of 1 */
	template <typename T, std::size_t N>
	[[nodiscard]] auto normalize(const vec<T, N>& v) noexcept {
		return v / length(v);
	}

	using s8vec2 = vec<std::int8_t, 2>;
	using s8vec3 = vec<std::int8_t, 3>;
	using s8vec4 = vec<std::int8_t, 4>;
	using u8vec2 = vec<std::uint8_t, 2>;
	using u8vec3 = vec<std::uint8_t, 3>;
	using u8vec4 = vec<std::uint8_t, 4>;

	using s16vec2 = vec<std::int16_t, 2>;
	using s16vec3 = vec<std::int16_t, 3>;
	using s16vec4 = vec<std::int16_t, 4>;
	using u16vec2 = vec<std::uint16_t, 2>;
	using u16vec3 = vec<std::uint16_t, 3>;
	using u16vec4 = vec<std::uint16_t, 4>;

	using s32vec2 = vec<std::int32_t, 2>;
	using s32vec3 = vec<std::int32_t, 3>;
	using s32vec4 = vec<std::int32_t, 4>;
	using u32vec2 = vec<std::uint32_t, 2>;
	using u32vec3 = vec<std::uint32_t, 3>;
	using u32vec4 = vec<std::uint32_t, 4>;

	using fvec2 = vec<float, 2>;
	using fvec3 = vec<float, 3>;
	using fvec4 = vec<float, 4>;
	using dvec2 = vec<double, 2>;
	using dvec3 = vec<double, 3>;
	using dvec4 = vec<double, 4>;

	/** A quaternion */
	template <typename T>
	class quat {
		static_assert(std::is_floating_point_v<T>);

		std::array<T, 4> data;

	public:
		constexpr explicit quat() noexcept : data{0.f, 0.f, 0.f, 1.f} {}

		/** Creates a new quaternion from 4 floats in the order X, Y, Z, W */
		template <typename... Args, std::enable_if_t<sizeof...(Args) == 4, bool> = true>
		constexpr explicit quat(Args... args) noexcept : data { std::forward<Args>(args)... } {}

		[[nodiscard]] constexpr std::size_t size() const noexcept {
			return data.size();
		}
		[[nodiscard]] constexpr std::size_t size_bytes() const noexcept {
			return size() * sizeof(T);
		}

		[[nodiscard]] constexpr decltype(auto) operator[](std::size_t idx) noexcept {
			return data[idx];
		}
		[[nodiscard]] constexpr decltype(auto) operator[](std::size_t idx) const noexcept {
			return data[idx];
		}

		[[nodiscard]] constexpr decltype(auto) x() noexcept {
			return data[0];
		}
		[[nodiscard]] constexpr decltype(auto) y() noexcept {
			return data[1];
		}
		[[nodiscard]] constexpr decltype(auto) z() noexcept {
			return data[2];
		}
		[[nodiscard]] constexpr decltype(auto) w() noexcept {
			return data[3];
		}
		[[nodiscard]] constexpr decltype(auto) x() const noexcept {
			return data[0];
		}
		[[nodiscard]] constexpr decltype(auto) y() const noexcept {
			return data[1];
		}
		[[nodiscard]] constexpr decltype(auto) z() const noexcept {
			return data[2];
		}
		[[nodiscard]] constexpr decltype(auto) w() const noexcept {
			return data[3];
		}

		[[nodiscard]] constexpr T* value_ptr() noexcept {
			return data.data();
		}
		[[nodiscard]] constexpr const T* value_ptr() const noexcept {
			return data.data();
		}

		[[nodiscard]] constexpr bool operator==(const quat<T>& other) const noexcept {
			for (std::size_t i = 0; i < data.size(); ++i)
				if ((*this)[i] != other[i])
					return false;
			return true;
		}
		[[nodiscard]] constexpr bool operator!=(const quat<T>& other) const noexcept {
			return !(*this == other);
		}
	};

	/**  Converts the given quaternion into a 3x3 rotation matrix */
	template <typename T>
	[[nodiscard]] auto asMatrix(const quat<T>& rot) noexcept {
		vec<T, 4> c1(
			T(1) - T(2) * (rot.y() * rot.y() + rot.z() * rot.z()),
			T(2) * (rot.x() * rot.y() + rot.w() * rot.z()),
			T(2) * (rot.x() * rot.z() - rot.w() * rot.y()),
			0.f
		);
		vec<T, 4> c2(
			T(2) * (rot.x() * rot.y() - rot.w() * rot.z()),
			T(1) - T(2) * (rot.x() * rot.x() + rot.z() * rot.z()),
			T(2) * (rot.y() * rot.z() + rot.w() * rot.x()),
			0.f
		);
		vec<T, 4> c3(
			T(2) * (rot.x() * rot.z() + rot.w() * rot.y()),
			T(2) * (rot.y() * rot.z() - rot.w() * rot.x()),
			T(1) - T(2) * (rot.x() * rot.x() + rot.y() * rot.y()),
			0.f
		);
		// TODO: Return a mat<T, 3, 3> from here and implement the conversion elsewhere.
		vec<T, 4> c4(0.f, 0.f, 0.f, 1.f);
		return mat<T, 4, 4>(c1, c2, c3, c4);
	}

	using fquat = quat<float>;
	using dquat = quat<double>;

	/**
	 * A matrix stored in column major order.
	 *
	 * @param N Number of rows, or the length of a single column.
	 * @param M Number of columns, or the length of a single row.
	 */
	template <typename T, std::size_t N, std::size_t M>
	class mat {
		static_assert(N >= 2 && N <= 4);
		static_assert(M >= 2 && M <= 4);

		// Every vec<> here is a column, with M being the column count.
		std::array<vec<T, N>, M> data;

		template <typename... Args, std::size_t... i>
		constexpr void copy_floats(const std::tuple<Args...>& tuple, std::integer_sequence<std::size_t, i...>) noexcept {
			(..., (data[i / M][i % N] = std::get<i>(std::move(tuple))));
		}

	public:
		/** Initialises a identity matrix with a specified value. */
		constexpr explicit mat(T value = T(1)) noexcept : data() {
			for (std::size_t i = 0U; i < fastgltf::min(N, M); ++i)
				data[i][i] = value;
		}

		/** Creates a matrix from M vectors */
		template <typename... Args, std::enable_if_t<sizeof...(Args) == M, bool> = true>
		constexpr explicit mat(Args... args) noexcept : data { std::forward<Args>(args)... } {}

		/** Creates a matrix from N * M floats */
		template <typename... Args, std::enable_if_t<sizeof...(Args) == M * N, bool> = true>
		constexpr explicit mat(Args... args) noexcept {
			const auto tuple = std::forward_as_tuple(args...);
			copy_floats(tuple, std::make_integer_sequence<std::size_t, sizeof...(Args)>());
		}

		[[nodiscard]] constexpr std::size_t columns() const noexcept {
			return M;
		}
		[[nodiscard]] constexpr std::size_t rows() const noexcept {
			return N;
		}
		[[nodiscard]] constexpr std::size_t size() const noexcept {
			return rows() * columns();
		}
		[[nodiscard]] constexpr std::size_t size_bytes() const noexcept {
			return size() * sizeof(T);
		}

		/** Returns the column vector at the given index. */
		[[nodiscard]] constexpr decltype(auto) operator[](std::size_t idx) noexcept {
			return col(idx);
		}
		[[nodiscard]] constexpr decltype(auto) operator[](std::size_t idx) const noexcept {
			return col(idx);
		}

		[[nodiscard]] constexpr decltype(auto) col(std::size_t idx) noexcept {
			return data[idx];
		}
		[[nodiscard]] constexpr decltype(auto) col(std::size_t idx) const noexcept {
			return data[idx];
		}

		/** Returns the row vector at the given index. Note that this is always a copy. */
		template <typename Indices = std::make_integer_sequence<std::size_t, M>>
		[[nodiscard]] constexpr auto row(std::size_t idx) const noexcept {
			return row(idx, Indices{});
		}

	private:
		template <std::size_t... Indices>
		[[nodiscard]] constexpr auto row(std::size_t idx, [[maybe_unused]] std::integer_sequence<std::size_t, Indices...> seq) const noexcept {
			return vec<T, M>(col(Indices)[idx]...);
		}

	public:
		[[nodiscard]] constexpr bool operator==(const mat<T, N, M>& other) const noexcept {
			for (std::size_t i = 0; i < N; ++i)
				if (col(i) != other.col(i))
					return false;
			return true;
		}
		[[nodiscard]] constexpr bool operator!=(const mat<T, N, M>& other) const noexcept {
			return !(*this == other);
		}

		constexpr auto operator*(vec<T, M> other) const noexcept {
			vec<T, M> ret(T(0));
			for (std::size_t i = 0; i < M; ++i)
				ret += col(i) * other[i];
			return ret;
		}

		template <std::size_t P, std::size_t Q, std::enable_if_t<M == P, bool> = true>
		constexpr auto operator*(const mat<T, P, Q>& other) const noexcept {
			mat<T, N, Q> ret;
			for (std::size_t i = 0; i < other.columns(); ++i)
				for (std::size_t j = 0; j < rows(); ++j)
					ret.col(i)[j] = dot(row(j), other.col(i));
			return ret;
		}
	};

	/** Translates a given transform matrix by the world space translation vector */
	template <typename T>
	[[nodiscard]] auto translate(const mat<T, 4, 4>& m, const vec<T, 3>& translation) noexcept {
		mat<T, 4, 4> ret = m;
		ret.col(3) = m.col(0) * translation.x() + m.col(1) * translation.y() + m.col(2) * translation.z() + m.col(3);
		return ret;
	}

	/** Scales a given matrix by the three dimensional scale vector. */
	template <typename T>
	[[nodiscard]] auto scale(const mat<T, 4, 4>& m, const vec<T, 3>& scale) noexcept {
		mat<T, 4, 4> ret;
		ret.col(0) = m.col(0) * scale.x();
		ret.col(1) = m.col(1) * scale.y();
		ret.col(2) = m.col(2) * scale.z();
		ret.col(3) = m.col(3);
		return ret;
	}

	/** Rotates the given matrix using the given quaternion */
	template <typename T>
	[[nodiscard]] auto rotate(const mat<T, 4, 4>& m, const quat<T>& rot) noexcept {
		return m * asMatrix(rot);
	}

	template <typename T, std::size_t N, std::size_t M>
	[[nodiscard]] auto transpose(const mat<T, N, M>& m) noexcept {
		mat<T, M, N> ret;
		for (std::size_t i = 0; i < N; ++i)
			for (std::size_t j = 0; j < M; ++j)
				ret[i][j] = m[j][i];
		return ret;
	}

	using fmat2x2 = mat<float, 2, 2>;
	using fmat3x3 = mat<float, 3, 3>;
	using fmat4x4 = mat<float, 4, 4>;
	using dmat2x2 = mat<double, 2, 2>;
	using dmat3x3 = mat<double, 3, 3>;
	using dmat4x4 = mat<double, 4, 4>;

	/**
	 * Decomposes a transform matrix into the translation, rotation, and scale components. This
	 * function does not support skew, shear, or perspective. This currently uses a quick algorithm
	 * to calculate the quaternion from the rotation matrix, which might occasionally loose some
	 * precision, though we try to use doubles here.
	 */
	inline void decomposeTransformMatrix(fmat4x4 matrix, fvec3& scale, fquat& rotation, fvec3& translation) {
		// Extract the translation. We zero the translation out, as we reuse the matrix as
		// the rotation matrix at the end.
		translation = matrix.col(3);
		matrix.col(3) = fvec4(0.f, 0.f, 0.f, matrix.col(3)[3]);

		// Extract the scale. We calculate the euclidean length of the columns.
		// We then construct a vector with those lengths.
		scale = math::fvec3(
			length(matrix.col(0)),
			length(matrix.col(1)),
			length(matrix.col(2))
		);

		// Remove the scaling from the matrix, leaving only the rotation.
		// matrix is now the rotation matrix.
		matrix.col(0) /= scale.x();
		matrix.col(1) /= scale.y();
		matrix.col(2) /= scale.z();

		// Construct the quaternion. This algo is copied from here:
		// https://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/christian.htm.
		// glTF orders the components as x,y,z,w
		rotation = math::fquat(
			max(.0f, 1.f + matrix[0][0] - matrix[1][1] - matrix[2][2]),
			max(.0f, 1.f - matrix[0][0] + matrix[1][1] - matrix[2][2]),
			max(.0f, 1.f - matrix[0][0] - matrix[1][1] + matrix[2][2]),
			max(.0f, 1.f + matrix[0][0] + matrix[1][1] + matrix[2][2])
		);
		rotation.x() = static_cast<float>(sqrt(static_cast<double>(rotation.x()))) / 2;
		rotation.y() = static_cast<float>(sqrt(static_cast<double>(rotation.y()))) / 2;
		rotation.z() = static_cast<float>(sqrt(static_cast<double>(rotation.z()))) / 2;
		rotation.w() = static_cast<float>(sqrt(static_cast<double>(rotation.w()))) / 2;

		rotation.x() = std::copysignf(rotation.x(), matrix[1][2] - matrix[2][1]);
		rotation.y() = std::copysignf(rotation.y(), matrix[2][0] - matrix[0][2]);
		rotation.z() = std::copysignf(rotation.z(), matrix[0][1] - matrix[1][0]);
	}
} // namespace fastgltf::math
