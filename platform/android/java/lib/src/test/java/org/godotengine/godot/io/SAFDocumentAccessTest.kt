/**************************************************************************/
/*  SAFDocumentAccessTest.kt                                              */
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
import android.provider.DocumentsContract.Document
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.IOException

@RunWith(RobolectricTestRunner::class)
@Config(manifest = Config.NONE, sdk = [24, 36])
class SAFDocumentAccessTest {
	private val root = "content://provider/tree/opaque%3Aroot"
	private val flags = Document.FLAG_DIR_SUPPORTS_CREATE or Document.FLAG_SUPPORTS_WRITE or
		Document.FLAG_SUPPORTS_DELETE or Document.FLAG_SUPPORTS_RENAME or Document.FLAG_SUPPORTS_MOVE

	private inner class FakeProvider : SAFDocumentAccess.Provider {
		val entries = mutableMapOf<Uri, SAFDocumentAccess.Entry>()
		val contents = mutableMapOf<Uri, MutableList<Uri>>()
		val rootUri = SAFPath.parse(root).documentUri
		var queries = 0
		var listings = 0
		var creates = 0
		var deletes = 0
		var renames = 0
		var moves = 0
		var readAllowed = true
		var writeAllowed = true
		var failListing = false
		var serial = 0

		init { entries[rootUri] = SAFDocumentAccess.Entry(rootUri, "root", Document.MIME_TYPE_DIR, flags) }
		fun add(parent: Uri = rootUri, name: String, directory: Boolean = false, capabilities: Int = flags): Uri {
			val uri = Uri.parse("content://provider/tree/opaque%3Aroot/document/random-${++serial}%2Fopaque")
			entries[uri] = SAFDocumentAccess.Entry(uri, name, if (directory) Document.MIME_TYPE_DIR else "text/plain", capabilities, 42L, 123000L)
			contents.getOrPut(parent) { mutableListOf() }.add(uri)
			return uri
		}
		override fun query(uri: Uri): SAFDocumentAccess.Entry? { queries++; return entries[uri] }
		override fun children(uri: Uri): List<SAFDocumentAccess.Entry> {
			listings++
			if (failListing) throw IOException("Offline or incomplete provider listing")
			return contents[uri].orEmpty().mapNotNull { entries[it] }
		}
		override fun canRead(uri: Uri) = readAllowed
		override fun canWrite(uri: Uri) = writeAllowed
		override fun create(parent: Uri, mimeType: String, name: String): Uri { creates++; return add(parent, name, mimeType == Document.MIME_TYPE_DIR) }
		override fun delete(uri: Uri): Boolean {
			deletes++
			contents.values.forEach { it.remove(uri) }
			return entries.remove(uri) != null
		}
		override fun rename(uri: Uri, name: String): Uri? {
			renames++
			val entry = entries.remove(uri) ?: return null
			val replacement = Uri.parse("content://provider/tree/opaque%3Aroot/document/renamed-${++serial}")
			entries[replacement] = entry.copy(uri = replacement, name = name)
			contents.remove(uri)?.let { contents[replacement] = it }
			contents.values.forEach { list -> val i = list.indexOf(uri); if (i >= 0) list[i] = replacement }
			return replacement
		}
		override fun move(uri: Uri, oldParent: Uri, newParent: Uri): Uri? {
			moves++
			if (contents[oldParent]?.remove(uri) != true) return null
			contents.getOrPut(newParent) { mutableListOf() }.add(uri)
			return uri
		}
	}

	private fun path(suffix: String = "") = SAFPath.parse("$root#$suffix")

	@Test fun rootEnumerationUsesOneBatchQueryAndNoPerChildQueries() {
		val p = FakeProvider()
		repeat(1000) { p.add(name = "file$it", directory = it == 0) }
		val entries = SAFDocumentAccess(p).list(path())!!
		assertEquals(1000, entries.size)
		assertTrue(entries[0].isDirectory)
		assertFalse(entries[1].isDirectory)
		assertEquals(1, p.queries)
		assertEquals(1, p.listings)
		assertEquals(0, p.creates)
	}

	@Test fun resolvesOpaqueIdsWithOneChildQueryPerLevel() {
		val p = FakeProvider()
		val a = p.add(name = "a", directory = true)
		val b = p.add(a, "b", true)
		val file = p.add(b, "save.dat")
		assertEquals(file, SAFDocumentAccess(p).resolve(path("a/b/save.dat"))?.uri)
		assertEquals(1, p.queries)
		assertEquals(3, p.listings)
	}

	@Test fun missingReadsNeverCreateAnything() {
		val p = FakeProvider(); val access = SAFDocumentAccess(p)
		assertNull(access.resolve(path("missing/subdir/file")))
		assertNull(access.list(path("missing")))
		assertThrows(IOException::class.java) { access.openFile(path("missing/file"), false) }
		assertFalse(access.remove(path("missing"), true))
		assertEquals(0, p.creates)
		assertEquals(0, p.deletes)
	}

	@Test fun singleAndRecursiveDirectoryCreationDiffer() {
		val p = FakeProvider(); val access = SAFDocumentAccess(p)
		assertFalse(access.makeDirectory(path("a/b"), false))
		assertEquals(0, p.creates)
		assertTrue(access.makeDirectory(path("a/b"), true))
		assertEquals(2, p.creates)
		assertTrue(access.makeDirectory(path("a/b"), false))
		assertEquals(2, p.creates)
		assertTrue(access.resolve(path("a/b"))!!.isDirectory)
	}

	@Test fun explicitWriteOpenRetainsParentCreationCompatibility() {
		val p = FakeProvider(); val access = SAFDocumentAccess(p)
		val uri = access.openFile(path("a/b/file"), true)
		assertEquals(3, p.creates)
		assertFalse(p.entries[uri]!!.isDirectory)
		assertEquals(uri, access.openFile(path("a/b/file"), false))
		assertEquals(3, p.creates)
	}

	@Test fun fileAsParentAndDirectoryAsFileFailWithoutMutation() {
		val p = FakeProvider(); val access = SAFDocumentAccess(p)
		p.add(name = "file")
		p.add(name = "dir", directory = true)
		assertFalse(access.makeDirectory(path("file/child"), true))
		assertThrows(IOException::class.java) { access.openFile(path("file/child"), true) }
		assertThrows(IOException::class.java) { access.openFile(path("dir"), true) }
		assertThrows(IOException::class.java) { access.openFile(path(), true) }
		assertEquals(0, p.creates)
	}

	@Test fun deletionNeverDeletesNonemptyDirectoryOrSelectedRoot() {
		val p = FakeProvider(); val access = SAFDocumentAccess(p)
		val dir = p.add(name = "dir", directory = true)
		p.add(dir, "file")
		assertFalse(access.remove(path(), true))
		assertFalse(access.remove(path("dir"), true))
		assertFalse(access.remove(path("dir"), false))
		assertEquals(0, p.deletes)
		assertTrue(access.remove(path("dir/file"), true))
		assertTrue(access.remove(path("dir"), true))
		assertEquals(2, p.deletes)
	}

	@Test fun failedListingCannotBeMistakenForEmptyDirectory() {
		val p = FakeProvider(); val access = SAFDocumentAccess(p)
		p.add(name = "empty", directory = true)
		p.failListing = true
		assertThrows(IOException::class.java) { access.remove(path("empty"), true) }
		assertEquals(0, p.deletes)
	}

	@Test fun flagsAndRevokedPermissionsPreventMutation() {
		val p = FakeProvider(); val access = SAFDocumentAccess(p)
		p.add(name = "locked", capabilities = 0)
		assertFalse(access.isWritable(path("locked")))
		assertFalse(access.remove(path("locked"), true))
		assertFalse(access.rename(path("locked"), path("renamed")))
		p.writeAllowed = false
		assertThrows(IOException::class.java) { access.makeDirectory(path("new"), false) }
		assertFalse(access.isWritable(path()))
		p.readAllowed = false
		assertFalse(access.isReadable(path()))
		assertEquals(0, p.creates + p.deletes + p.renames)
	}

	@Test fun renameHandlesChangedDocumentIdsAndDirectoryChildren() {
		val p = FakeProvider(); val access = SAFDocumentAccess(p)
		val dir = p.add(name = "before", directory = true)
		val file = p.add(dir, "child")
		assertTrue(access.rename(path("before"), path("after")))
		assertNull(access.resolve(path("before")))
		assertEquals(file, access.resolve(path("after/child"))?.uri)
		assertEquals(1, p.renames)
		assertEquals(0, p.moves + p.deletes + p.creates)
	}

	@Test fun renameRejectsCollisionCrossProviderAndRootMutation() {
		val p = FakeProvider(); val access = SAFDocumentAccess(p)
		p.add(name = "a"); p.add(name = "b")
		assertFalse(access.rename(path("a"), path("b")))
		assertFalse(access.rename(path("a"), SAFPath.parse("content://elsewhere/tree/other#b")))
		assertFalse(access.rename(path(), path("new")))
		assertFalse(access.rename(SAFPath.parse("content://provider/document/file"), path("new")))
		assertTrue(access.rename(path("a"), path("a")))
		assertEquals(0, p.renames + p.moves + p.deletes)
	}

	@Test fun moveUsesOneProviderOperationAndRejectsMovePlusRename() {
		val p = FakeProvider(); val access = SAFDocumentAccess(p)
		val source = p.add(name = "source", directory = true)
		p.add(name = "target", directory = true)
		val file = p.add(source, "file")
		assertFalse(access.rename(path("source/file"), path("target/new")))
		assertEquals(0, p.moves + p.renames)
		assertTrue(access.rename(path("source/file"), path("target/file")))
		assertEquals(file, access.resolve(path("target/file"))?.uri)
		assertEquals(1, p.moves)
		assertEquals(0, p.creates + p.deletes + p.renames)
	}

	@Test fun moveRejectsDescendantsAndMissingCapabilities() {
		val p = FakeProvider(); val access = SAFDocumentAccess(p)
		val source = p.add(name = "source", directory = true)
		p.add(source, "child", true)
		p.add(name = "target", directory = true)
		p.add(source, "file", capabilities = Document.FLAG_SUPPORTS_RENAME)
		assertFalse(access.rename(path("source"), path("source/child/source")))
		assertFalse(access.rename(path("source/file"), path("target/file")))
		assertEquals(0, p.moves + p.renames)
	}

	@Test fun duplicateNamesFailClosedRatherThanChooseArbitrarily() {
		val p = FakeProvider(); val access = SAFDocumentAccess(p)
		p.add(name = "same"); p.add(name = "same")
		assertThrows(IOException::class.java) { access.resolve(path("same")) }
		assertThrows(IOException::class.java) { access.remove(path("same"), true) }
		assertEquals(0, p.deletes)
	}

	@Test fun noPersistentCacheObservesExternalChanges() {
		val p = FakeProvider(); val access = SAFDocumentAccess(p)
		val file = p.add(name = "file")
		assertNotNull(access.resolve(path("file")))
		p.entries.remove(file)
		assertNull(access.resolve(path("file")))
		assertEquals(2, p.listings)
	}

	@Test fun encodedPathsReachLiteralDisplayNames() {
		val p = FakeProvider(); val access = SAFDocumentAccess(p)
		for (name in listOf("%2F", "100%", "hash#", "中文😀", "a\\b")) {
			val uri = p.add(name = name)
			val canonical = SAFPath.resolve(root, name).toString()
			assertEquals(uri, access.resolve(SAFPath.parse(canonical))?.uri)
		}
	}
}
