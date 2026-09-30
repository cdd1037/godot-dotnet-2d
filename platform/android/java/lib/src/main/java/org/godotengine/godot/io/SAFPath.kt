/**************************************************************************/
/*  SAFPath.kt                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

package org.godotengine.godot.io

import android.net.Uri
import android.provider.DocumentsContract

/**
 * A SAF URI followed by an optional URI-encoded, tree-relative path.
 * Document IDs and the base URI are opaque: only the fragment is normalized.
 */
internal data class SAFPath(val baseUri: Uri, val parts: List<String>) {
	val isTree: Boolean
		get() = DocumentsContract.isTreeUri(baseUri)

	val documentUri: Uri
		get() = if (isTree && baseUri.pathSegments.size == 2) {
			DocumentsContract.buildDocumentUriUsingTree(baseUri, DocumentsContract.getTreeDocumentId(baseUri))
		} else {
			baseUri
		}

	fun parent() = copy(parts = parts.dropLast(1))

	override fun toString(): String {
		return if (isTree || parts.isNotEmpty()) {
			// Keep '/' as the path separator, encoding each display name independently.
			"$baseUri#" + parts.joinToString("/") { Uri.encode(it) }
		} else {
			baseUri.toString()
		}
	}

	companion object {
		fun isValidDirectoryEntryName(name: String): Boolean {
			return name.isNotEmpty() && name != "." && name != ".." && !name.contains('/') &&
				!name.contains('\u0000') && !name.startsWith('\\') && !name.contains(":\\")
		}

		fun parse(path: String): SAFPath {
			val fragmentStart = path.indexOf('#')
			val base = if (fragmentStart < 0) path else path.substring(0, fragmentStart)
			val uri = Uri.parse(base)
			require(uri.scheme == "content" && !uri.authority.isNullOrEmpty()) { "Invalid content URI" }
			val tree = DocumentsContract.isTreeUri(uri)
			if (tree) {
				val segments = uri.pathSegments
				require(segments.size == 2 || (segments.size == 4 && segments[2] == "document")) { "Invalid tree URI" }
			}
			require(fragmentStart < 0 || tree) { "Relative paths require a tree URI" }
			// Preserve FileAccess's existing once-decoded fragment syntax, including %2F
			// as a separator. Relative DirAccess arguments are raw names instead (below).
			val relative = if (fragmentStart < 0) "" else Uri.decode(path.substring(fragmentStart + 1))
			return SAFPath(uri, normalize(emptyList(), relative))
		}

		fun resolve(current: String, path: String): SAFPath {
			if (path.startsWith("content://")) {
				return parse(path)
			}
			require(!path.startsWith('/') && !path.contains("://")) { "Expected a relative path" }
			val base = parse(current)
			require(base.isTree) { "Relative paths require a tree URI" }
			return base.copy(parts = normalize(base.parts, path))
		}

		private fun normalize(base: List<String>, path: String): List<String> {
			require(!path.contains('\u0000')) { "Invalid document name" }
			val result = base.toMutableList()
			for (part in path.split('/')) {
				when (part) {
					"", "." -> Unit
					".." -> {
						require(result.isNotEmpty()) { "Path escapes the selected directory" }
						result.removeAt(result.lastIndex)
					}
					else -> result.add(part)
				}
			}
			return result
		}
	}
}
