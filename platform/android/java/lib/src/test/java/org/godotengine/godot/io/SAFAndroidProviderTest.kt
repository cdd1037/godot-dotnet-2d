/**************************************************************************/
/*  SAFAndroidProviderTest.kt                                             */
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

import android.content.ContentProvider
import android.content.ContentValues
import android.net.Uri
import android.content.Context
import android.content.ContextWrapper
import android.content.pm.PackageManager
import android.content.pm.ProviderInfo
import android.database.Cursor
import android.database.MatrixCursor
import android.os.Bundle
import android.os.CancellationSignal
import android.os.ParcelFileDescriptor
import android.provider.DocumentsContract
import android.provider.DocumentsContract.Document
import android.provider.DocumentsProvider
import org.godotengine.godot.io.directory.DirectoryAccessHandler
import org.godotengine.godot.io.file.SAFData
import org.godotengine.godot.io.file.FileAccessFlags
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import org.robolectric.shadows.ShadowContentResolver
import java.io.IOException
import java.io.File
import java.nio.ByteBuffer

/** Exercises the real ContentResolver/DocumentsContract adapter using a local test provider. */
@RunWith(RobolectricTestRunner::class)
@Config(manifest = Config.NONE, sdk = [28, 36])
class SAFAndroidProviderTest {
	private val authority = "org.godotengine.test.saf"
	private val root = "content://$authority/tree/opaque%3Aroot"
	private lateinit var context: Context
	private lateinit var provider: TestDocumentsProvider

	class TestDocumentsProvider : DocumentsProvider() {
		data class Row(val id: String, val name: String, val type: String, val flags: Int)
		val rows = mutableListOf(Row("opaque:root", "root", Document.MIME_TYPE_DIR, Document.FLAG_DIR_SUPPORTS_CREATE),
			Row("unrelated-id-1", "save%2F#😀", "text/plain", Document.FLAG_SUPPORTS_DELETE or Document.FLAG_SUPPORTS_RENAME),
			Row("unrelated-id-2", ".hidden", Document.MIME_TYPE_DIR, Document.FLAG_DIR_SUPPORTS_CREATE))
		var documentQueries = 0
		var childQueries = 0
		var loading = false
		var fail = false
		var nullCursor = false
		var deletes = 0
		var closedCursors = 0
		var creates = 0
		var renames = 0
		override fun onCreate() = true
		override fun queryRoots(projection: Array<out String>?) = MatrixCursor(projection ?: emptyArray())
		private fun cursor(projection: Array<out String>?, selected: List<Row>): Cursor? {
			if (nullCursor) return null
			if (fail) throw SecurityException("Grant revoked")
			val columns = projection ?: arrayOf(Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME, Document.COLUMN_MIME_TYPE, Document.COLUMN_FLAGS)
			val cursor = object : MatrixCursor(columns) {
				override fun close() { super.close(); closedCursors++ }
			}
			for (row in selected) {
				cursor.addRow(columns.map { when (it) {
					Document.COLUMN_DOCUMENT_ID -> row.id
					Document.COLUMN_DISPLAY_NAME -> row.name
					Document.COLUMN_MIME_TYPE -> row.type
					Document.COLUMN_FLAGS -> row.flags
					Document.COLUMN_SIZE -> 42L
					Document.COLUMN_LAST_MODIFIED -> 123000L
					else -> null
				} })
			}
			if (loading) cursor.extras = Bundle().apply { putBoolean(DocumentsContract.EXTRA_LOADING, true) }
			return cursor
		}
		override fun queryDocument(documentId: String, projection: Array<out String>?): Cursor? {
			documentQueries++
			return cursor(projection, rows.filter { it.id == documentId })
		}
		override fun queryChildDocuments(parentDocumentId: String, projection: Array<out String>?, sortOrder: String?): Cursor? {
			childQueries++
			return cursor(projection, if (parentDocumentId == "opaque:root") rows.drop(1) else emptyList())
		}
		override fun isChildDocument(parentDocumentId: String, documentId: String) = parentDocumentId == "opaque:root" && rows.any { it.id == documentId }
		override fun openDocument(documentId: String, mode: String, signal: CancellationSignal?): ParcelFileDescriptor? {
			val file = File(context!!.cacheDir, documentId.replace(':', '_'))
			return ParcelFileDescriptor.open(file, ParcelFileDescriptor.parseMode(mode))
		}
		override fun deleteDocument(documentId: String) { deletes++; rows.removeAll { it.id == documentId } }
		override fun createDocument(parentDocumentId: String, mimeType: String, displayName: String): String {
			val id = "created-${++creates}"
			rows.add(Row(id, displayName, mimeType, Document.FLAG_SUPPORTS_DELETE or Document.FLAG_SUPPORTS_RENAME))
			return id
		}
		override fun renameDocument(documentId: String, displayName: String): String {
			val index = rows.indexOfFirst { it.id == documentId }
			val id = "renamed-${++renames}"
			rows[index] = rows[index].copy(id = id, name = displayName)
			return id
		}
	}

	@Before fun setUp() {
		val application = RuntimeEnvironment.getApplication()
		context = object : ContextWrapper(application) {
			override fun checkCallingOrSelfUriPermission(uri: android.net.Uri, modeFlags: Int) = PackageManager.PERMISSION_GRANTED
		}
		provider = TestDocumentsProvider()
		provider.attachInfo(application, ProviderInfo().apply {
			authority = this@SAFAndroidProviderTest.authority
			exported = true
			grantUriPermissions = true
			readPermission = "android.permission.MANAGE_DOCUMENTS"
			writePermission = "android.permission.MANAGE_DOCUMENTS"
		})
		// ShadowContentResolver calls the legacy query overload directly rather than
		// going through Android's binder transport, which translates it to Bundle args.
		val transport = object : ContentProvider() {
			override fun onCreate() = true
			override fun query(uri: Uri, projection: Array<out String>?, selection: String?, selectionArgs: Array<out String>?, sortOrder: String?): Cursor? =
				provider.query(uri, projection, Bundle(), null)
			override fun getType(uri: Uri) = provider.getType(uri)
			override fun insert(uri: Uri, values: ContentValues?): Uri? = null
			override fun update(uri: Uri, values: ContentValues?, selection: String?, selectionArgs: Array<out String>?) = 0
			override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?) = 0
			override fun call(method: String, arg: String?, extras: Bundle?): Bundle? = provider.call(method, arg, extras)
			override fun openFile(uri: Uri, mode: String): ParcelFileDescriptor? = provider.openDocument(DocumentsContract.getDocumentId(uri), mode, null)
		}
		transport.attachInfo(application, ProviderInfo().apply { authority = this@SAFAndroidProviderTest.authority })
		ShadowContentResolver.registerProviderInternal(authority, transport)
	}

	@Test fun routeAndEnumerateWithNoPerEntryQueries() {
		val access = DirectoryAccessHandler(context)
		val id = access.dirOpen(2, root)
		assertTrue(id > 0)
		assertFalse(access.dirIsDir(id))
		assertEquals("save%2F#😀", access.dirNext(id))
		assertFalse(access.dirIsDir(id))
		assertFalse(access.isCurrentHidden(id))
		assertEquals(".hidden", access.dirNext(id))
		assertTrue(access.dirIsDir(id))
		assertTrue(access.isCurrentHidden(id))
		assertEquals("", access.dirNext(id))
		assertFalse(access.dirIsDir(id))
		assertFalse(access.isCurrentHidden(id))
		assertEquals(1, provider.documentQueries)
		assertEquals(1, provider.childQueries)
		assertEquals(2, provider.closedCursors)
		access.dirClose(id)
		assertEquals("", access.dirNext(id))
	}

	@Test fun emptyDirectoryAndFileAreDistinguished() {
		val access = DirectoryAccessHandler(context)
		assertTrue(access.dirExists(2, root))
		assertFalse(access.fileExists(2, root))
		val hidden = "$root#.hidden"
		assertTrue(access.dirExists(2, hidden))
		assertFalse(access.fileExists(2, hidden))
		val id = access.dirOpen(2, hidden)
		assertTrue(id > 0)
		assertEquals("", access.dirNext(id))
		access.dirClose(id)
	}

	@Test fun resolverAndFileMetadataShareTheEncodedPathContract() {
		val access = DirectoryAccessHandler(context)
		val path = access.normalizePath(root, "save%2F#😀")!!
		assertTrue(access.fileExists(2, path))
		assertFalse(access.dirExists(2, path))
		assertTrue(SAFData.fileExists(context, path))
		assertEquals(42L, SAFData.fileSize(context, path))
		assertEquals(123L, SAFData.fileLastModified(context, path))
		assertFalse(SAFData.fileExists(context, "$root#missing/child"))
	}

	@Test fun loadingFailureAndNullCursorDoNotLookLikeEmptyDirectories() {
		val documents = SAFDocumentAccess(context)
		provider.loading = true
		assertThrows(IOException::class.java) { documents.list(SAFPath.parse(root)) }
		assertFalse(DirectoryAccessHandler(context).makeDir(2, "$root#new"))
		assertEquals(0, provider.creates)
		provider.loading = false
		provider.nullCursor = true
		assertThrows(IOException::class.java) { documents.list(SAFPath.parse(root)) }
		provider.nullCursor = false
		provider.fail = true
		assertFalse(DirectoryAccessHandler(context).dirExists(2, root))
		assertEquals(-1, DirectoryAccessHandler(context).dirOpen(2, root))
		assertEquals(0, provider.deletes)
	}

	@Test fun badOrCrossScopePathsNeverMutateProvider() {
		val access = DirectoryAccessHandler(context)
		assertNull(access.normalizePath(root, ".."))
		assertFalse(access.makeDir(2, "$root#../outside"))
		assertFalse(access.remove(2, root))
		assertFalse(access.rename(2, "$root#file", "/tmp/file"))
		assertFalse(access.rename(2, "/tmp/file", "$root#file"))
		assertEquals(0, provider.deletes)
	}
	@Test fun providerMutationCallsHandleChangedIds() {
		val access = DirectoryAccessHandler(context)
		val from = access.normalizePath(root, "save%2F#😀")!!
		val to = access.normalizePath(root, "renamed#😀")!!
		assertTrue(access.rename(2, from, to))
		assertFalse(access.fileExists(2, from))
		assertTrue(access.fileExists(2, to))
		assertEquals(1, provider.renames)
		assertTrue(access.remove(2, to))
		assertEquals(1, provider.deletes)
		assertTrue(access.makeDir(2, "$root#created"))
		assertTrue(access.dirExists(2, "$root#created"))
		assertEquals(1, provider.creates)
	}

	@Test fun unsafeProviderDisplayNamesFailListingBeforeAnyMutation() {
		provider.rows.add(TestDocumentsProvider.Row("unsafe", "/outside", "text/plain", Document.FLAG_SUPPORTS_DELETE))
		assertEquals(-1, DirectoryAccessHandler(context).dirOpen(2, root))
		assertEquals(0, provider.deletes)
	}

	@Test fun fileAccessWritesAndReadsThroughResolvedDocumentUri() {
		val path = "$root#new%25file%23%F0%9F%98%80"
		val bytes = "saved through SAF".toByteArray()
		val writer = SAFData(context, path, FileAccessFlags.WRITE)
		try {
			assertTrue(writer.write(ByteBuffer.wrap(bytes)))
		} finally {
			writer.close()
		}
		val reader = SAFData(context, path, FileAccessFlags.READ)
		try {
			val result = ByteBuffer.allocate(bytes.size)
			assertEquals(bytes.size, reader.read(result))
			assertArrayEquals(bytes, result.array())
		} finally {
			reader.close()
		}
		assertEquals(1, provider.creates)
	}

	@Test fun standaloneOpenableContentUrisKeepMinimalMetadataCompatibility() {
		val openable = object : ContentProvider() {
			override fun onCreate() = true
			override fun query(uri: Uri, projection: Array<out String>?, selection: String?, selectionArgs: Array<out String>?, sortOrder: String?): Cursor {
				val columns = projection ?: emptyArray()
				require(columns.all { it == Document.COLUMN_DISPLAY_NAME || it == Document.COLUMN_SIZE || it == Document.COLUMN_LAST_MODIFIED })
				return MatrixCursor(columns).apply { addRow(columns.map { if (it == Document.COLUMN_DISPLAY_NAME) "openable" else 42L }) }
			}
			override fun getType(uri: Uri) = "application/octet-stream"
			override fun insert(uri: Uri, values: ContentValues?): Uri? = null
			override fun update(uri: Uri, values: ContentValues?, selection: String?, selectionArgs: Array<out String>?) = 0
			override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?) = 0
		}
		openable.attachInfo(context, ProviderInfo().apply { authority = "org.godotengine.test.openable" })
		ShadowContentResolver.registerProviderInternal("org.godotengine.test.openable", openable)
		val uri = "content://org.godotengine.test.openable/arbitrary/opaque%2Fid"
		assertTrue(SAFData.fileExists(context, uri))
		assertEquals(42L, SAFData.fileSize(context, uri))
		assertTrue(DirectoryAccessHandler(context).fileExists(2, uri))
		assertFalse(DirectoryAccessHandler(context).dirExists(2, uri))
	}

	@Test fun nativeEntryPointsKeepExpectedSignatures() {
		val handler = DirectoryAccessHandler::class.java
		assertEquals(String::class.java, handler.getMethod("normalizePath", String::class.java, String::class.java).returnType)
		for (method in listOf("makeDirRecursive", "isReadable", "isWritable")) {
			assertEquals(Boolean::class.javaPrimitiveType, handler.getMethod(method, Int::class.javaPrimitiveType, String::class.java).returnType)
		}
	}

}
