/**************************************************************************/
/*  SAFPathTest.kt                                                        */
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

import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(manifest = Config.NONE, sdk = [24, 36])
class SAFPathTest {
	private val root = "content://provider/tree/opaque%2Fid%3Awith%25encoding"

	@Test fun preservesOpaqueBaseAndAddsRootFragment() {
		assertEquals("$root#", SAFPath.parse(root).toString())
		assertEquals(root, SAFPath.parse("$root#a/../b").baseUri.toString())
		assertEquals("$root#b", SAFPath.parse("$root#a/../b").toString())
	}

	@Test fun preservesExistingOnceDecodedAbsoluteFragments() {
		assertEquals(listOf("a", "b"), SAFPath.parse("$root#a%2Fb").parts)
		assertEquals(listOf("a b", "100%", "#", "中文😀", "%2F"),
			SAFPath.parse("$root#a%20b/100%25/%23/%E4%B8%AD%E6%96%87%F0%9F%98%80/%252F").parts)
	}

	@Test fun rawRelativeDisplayNamesEncodeExactlyOnce() {
		val path = SAFPath.resolve("$root#folder", "100%/%2F/#/中文😀")
		assertEquals(listOf("folder", "100%", "%2F", "#", "中文😀"), path.parts)
		assertEquals(path, SAFPath.parse(path.toString()))
		assertEquals("$root#folder/100%25/%252F/%23/%E4%B8%AD%E6%96%87%F0%9F%98%80", path.toString())
	}

	@Test fun normalizesOnlyRelativeComponents() {
		assertEquals("$root#", SAFPath.resolve("$root#a/b", "../..").toString())
		assertEquals(listOf("a\\b"), SAFPath.resolve(root, "a\\b").parts)
		assertEquals(listOf("b"), SAFPath.parse("$root#/a//./../b/").parts)
	}

	@Test fun rejectsTraversalAboveAnchor() {
		for (path in listOf("$root#..", "$root#a/../../b", "$root#%2E%2E/x", "$root#a%2F..%2F..")) {
			assertThrows(IllegalArgumentException::class.java) { SAFPath.parse(path) }
		}
		assertThrows(IllegalArgumentException::class.java) { SAFPath.resolve(root, "../a") }
	}

	@Test fun retainsTreeDocumentAnchor() {
		val anchor = "$root/document/other%2Fopaque%3Aid"
		assertEquals("$anchor#child", SAFPath.parse("$anchor#child").toString())
		assertEquals(anchor, SAFPath.parse(anchor).documentUri.toString())
		assertThrows(IllegalArgumentException::class.java) { SAFPath.resolve(anchor, "..") }
	}

	@Test fun standaloneDocumentsHaveNoNavigableParent() {
		val file = "content://provider/document/id%2Fnot-a-path"
		assertEquals(file, SAFPath.parse(file).toString())
		assertThrows(IllegalArgumentException::class.java) { SAFPath.parse("$file#child") }
		assertThrows(IllegalArgumentException::class.java) { SAFPath.resolve(file, "child") }
	}

	@Test fun rejectsInvalidPaths() {
		for (path in listOf("file:///tmp/a", "content:///tree/id", "$root/extra", "$root#bad%00name")) {
			assertThrows(IllegalArgumentException::class.java) { SAFPath.parse(path) }
		}
	}
	@Test fun directoryEntryNamesCannotBeAbsolutePathsOrNavigation() {
		for (name in listOf("", ".", "..", "/tmp", "a/b", "\\tmp", "C:\\tmp", "bad\u0000name")) {
			assertFalse(SAFPath.isValidDirectoryEntryName(name))
		}
		for (name in listOf("%2F", "#", "中文😀", "a\\b", "a:b")) {
			assertTrue(SAFPath.isValidDirectoryEntryName(name))
		}
	}

}
