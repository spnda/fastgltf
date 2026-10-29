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

#ifndef FASTGLTF_CORE_HPP
#define FASTGLTF_CORE_HPP

#include <fastgltf/parser.hpp>
#include <fastgltf/exporter.hpp>

namespace fastgltf {
	/**
	 * This function further validates all the input more strictly that is parsed from the glTF.
	 * Realistically, this should not be necessary in Release applications, but could be helpful
	 * when debugging an asset related issue.
	*/
	FASTGLTF_EXPORT [[nodiscard]] Error validate(const Asset& asset);

	/**
	 * Checks if the given accessor points at a valid range in a valid buffer view that points at a valid range
	 * in a valid buffer. This guarantees that the accessor can be used to read a range of data in the asset.
	 *
	 * This is useful when loading vertices, indices, ... through accessors using the functions in tools.hpp
	 * to not read out of bounds. Note that if the buffer's data is not yet loaded or is a custom buffer,
	 * then this only checks against the buffer's byteLength.
	 */
	FASTGLTF_EXPORT [[nodiscard]] Error validateAccessorBounds(const Asset& asset, const Accessor& accessor);
}

#endif
