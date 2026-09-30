/**************************************************************************/
/*  SAFDocumentAccess.kt                                                  */
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

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.database.Cursor
import android.net.Uri
import android.provider.DocumentsContract
import android.provider.DocumentsContract.Document
import java.io.FileNotFoundException
import java.io.IOException

/**
 * Shared, uncached SAF resolution for FileAccess and DirAccess. Read operations
 * never create documents. Document IDs are opaque and always supplied by the provider.
 */
internal class SAFDocumentAccess(private val provider: Provider) {
	constructor(context: Context) : this(AndroidProvider(context))

	internal data class Entry(val uri: Uri, val name: String, val mimeType: String, val flags: Int,
		val size: Long = -1L, val lastModified: Long = 0L) {
		val isDirectory: Boolean
			get() = mimeType == Document.MIME_TYPE_DIR
		fun supports(flag: Int) = flags and flag != 0
	}

	/** Kept separate from traversal so provider failures and mutations can be tested. */
	internal interface Provider {
		fun query(uri: Uri): Entry?
		fun children(uri: Uri): List<Entry>
		fun canRead(uri: Uri): Boolean
		fun canWrite(uri: Uri): Boolean
		fun create(parent: Uri, mimeType: String, name: String): Uri?
		fun delete(uri: Uri): Boolean
		fun rename(uri: Uri, name: String): Uri?
		fun move(uri: Uri, oldParent: Uri, newParent: Uri): Uri?
	}

	fun resolve(path: SAFPath): Entry? {
		var entry = provider.query(path.documentUri) ?: return null
		for (part in path.parts) {
			if (!entry.isDirectory) return null
			entry = findChild(entry, part) ?: return null
		}
		return entry
	}

	fun list(path: SAFPath): List<Entry>? {
		if (!path.isTree) return null
		val entry = resolve(path) ?: return null
		return if (entry.isDirectory) provider.children(entry.uri) else null
	}

	private fun findChild(parent: Entry, name: String): Entry? {
		// A single projection query replaces DocumentFile.findFile's per-child queries.
		// Do not cache across calls: grants and provider contents can change externally.
		val matches = provider.children(parent.uri).filter { it.name == name }
		if (matches.size > 1) throw IOException("Ambiguous document name")
		return matches.firstOrNull()
	}

	private fun create(parent: Entry, name: String, mimeType: String): Entry {
		if (!parent.isDirectory || !parent.supports(Document.FLAG_DIR_SUPPORTS_CREATE) || !provider.canWrite(parent.uri)) {
			throw IOException("Directory does not support creating documents")
		}
		val uri = provider.create(parent.uri, mimeType, name) ?: throw IOException("Unable to create document")
		val entry = provider.query(uri) ?: throw IOException("Unable to query created document")
		if (entry.name != name || entry.isDirectory != (mimeType == Document.MIME_TYPE_DIR)) {
			throw IOException("Provider changed the requested document name or type")
		}
		return entry
	}

	fun makeDirectory(path: SAFPath, recursive: Boolean): Boolean {
		if (!path.isTree) return false
		var entry = provider.query(path.documentUri) ?: return false
		for ((index, part) in path.parts.withIndex()) {
			if (!entry.isDirectory) return false
			val next = findChild(entry, part)
			entry = next ?: if (recursive || index == path.parts.lastIndex) {
				create(entry, part, Document.MIME_TYPE_DIR)
			} else {
				return false
			}
		}
		return entry.isDirectory
	}

	/** Preserve legacy SAF FileAccess write-open parent creation, explicitly. */
	fun openFile(path: SAFPath, createIfMissing: Boolean): Uri {
		if (!path.isTree) return path.documentUri
		var entry = provider.query(path.documentUri) ?: throw FileNotFoundException("Missing tree root")
		for ((index, part) in path.parts.withIndex()) {
			if (!entry.isDirectory) throw IOException("Parent is not a directory")
			val next = findChild(entry, part)
			entry = next ?: if (createIfMissing) {
				create(entry, part, if (index == path.parts.lastIndex) "application/octet-stream" else Document.MIME_TYPE_DIR)
			} else {
				throw FileNotFoundException("Document does not exist")
			}
		}
		if (entry.isDirectory) throw IOException("Cannot open a directory as a file")
		return entry.uri
	}

	fun isReadable(path: SAFPath): Boolean {
		val entry = resolve(path) ?: return false
		return provider.canRead(entry.uri)
	}

	fun isWritable(path: SAFPath): Boolean {
		val entry = resolve(path) ?: return false
		return provider.canWrite(entry.uri) && entry.supports(if (entry.isDirectory) Document.FLAG_DIR_SUPPORTS_CREATE else Document.FLAG_SUPPORTS_WRITE)
	}

	fun remove(path: SAFPath, allowDirectory: Boolean): Boolean {
		// Never delete the user's selected tree/anchor itself.
		val entry = resolve(path) ?: return false
		if (path.isTree && path.parts.isEmpty() && entry.isDirectory) return false
		if (!entry.supports(Document.FLAG_SUPPORTS_DELETE) || !provider.canWrite(entry.uri)) return false
		if (entry.isDirectory && (!allowDirectory || provider.children(entry.uri).isNotEmpty())) return false
		// SAF has no atomic rmdir-if-empty. Check immediately before deletion and fail
		// closed on incomplete/failed queries; never deliberately delete a nonempty tree.
		return provider.delete(entry.uri)
	}

	fun rename(from: SAFPath, to: SAFPath): Boolean {
		// A bare document grant cannot return its replacement URI via Godot's boolean API.
		// Tree-relative paths remain resolvable even if the provider changes document IDs.
		if (!from.isTree || !to.isTree || from.baseUri != to.baseUri || from.parts.isEmpty() || to.parts.isEmpty()) return false
		val source = resolve(from) ?: return false
		if (from == to) return true
		if (source.isDirectory && to.parts.take(from.parts.size) == from.parts) return false
		val oldParent = resolve(from.parent()) ?: return false
		val newParent = resolve(to.parent()) ?: return false
		if (!newParent.isDirectory || !provider.canWrite(source.uri) || !provider.canWrite(newParent.uri)) return false
		if (findChild(newParent, to.parts.last()) != null) return false
		val result = if (oldParent.uri == newParent.uri) {
			if (!source.supports(Document.FLAG_SUPPORTS_RENAME)) return false
			provider.rename(source.uri, to.parts.last())
		} else {
			// A move plus rename would be two mutations with no atomic rollback. Only
			// support the single provider move operation when the name is unchanged.
			if (source.name != to.parts.last() || !source.supports(Document.FLAG_SUPPORTS_MOVE) ||
				!newParent.supports(Document.FLAG_DIR_SUPPORTS_CREATE) || !provider.canWrite(oldParent.uri)) return false
			provider.move(source.uri, oldParent.uri, newParent.uri)
		} ?: return false
		return provider.query(result)?.let { it.name == to.parts.last() && it.isDirectory == source.isDirectory } == true
	}

	private class AndroidProvider(private val context: Context) : Provider {
		private val resolver = context.contentResolver
		private val projection = arrayOf(Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME,
			Document.COLUMN_MIME_TYPE, Document.COLUMN_FLAGS, Document.COLUMN_SIZE, Document.COLUMN_LAST_MODIFIED)

		override fun query(uri: Uri): Entry? {
			val cursor = resolver.query(uri, projection, null, null, null) ?: throw IOException("Unable to query document")
			return cursor.use {
				checkComplete(it)
				if (it.moveToFirst()) readEntry(it, uri) else null
			}
		}

		override fun children(uri: Uri): List<Entry> {
			val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(uri, DocumentsContract.getDocumentId(uri))
			val cursor = resolver.query(childrenUri, projection, null, null, null) ?: throw IOException("Unable to query children")
			return cursor.use {
				checkComplete(it)
				val result = ArrayList<Entry>()
				while (it.moveToNext()) {
					val id = it.getString(it.getColumnIndexOrThrow(Document.COLUMN_DOCUMENT_ID)) ?: throw IOException("Missing document ID")
					val childUri = DocumentsContract.buildDocumentUriUsingTree(uri, id)
					result.add(readEntry(it, childUri))
				}
				result
			}
		}

		private fun checkComplete(cursor: Cursor) {
			if (cursor.extras.getBoolean(DocumentsContract.EXTRA_LOADING, false)) throw IOException("Document listing is still loading")
			if (cursor.extras.containsKey(DocumentsContract.EXTRA_ERROR)) throw IOException("Provider reported a query error")
		}

		private fun readEntry(cursor: Cursor, uri: Uri): Entry {
			val name = cursor.getString(cursor.getColumnIndexOrThrow(Document.COLUMN_DISPLAY_NAME)) ?: throw IOException("Missing document name")
			val mime = cursor.getString(cursor.getColumnIndexOrThrow(Document.COLUMN_MIME_TYPE)) ?: throw IOException("Missing document type")
			fun number(column: String, fallback: Long): Long {
				val index = cursor.getColumnIndex(column)
				return if (index < 0 || cursor.isNull(index)) fallback else cursor.getLong(index)
			}
			return Entry(uri, name, mime, number(Document.COLUMN_FLAGS, 0L).toInt(), number(Document.COLUMN_SIZE, -1L), number(Document.COLUMN_LAST_MODIFIED, 0L))
		}

		override fun canRead(uri: Uri) = context.checkCallingOrSelfUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION) == PackageManager.PERMISSION_GRANTED
		override fun canWrite(uri: Uri) = context.checkCallingOrSelfUriPermission(uri, Intent.FLAG_GRANT_WRITE_URI_PERMISSION) == PackageManager.PERMISSION_GRANTED
		override fun create(parent: Uri, mimeType: String, name: String) = DocumentsContract.createDocument(resolver, parent, mimeType, name)
		override fun delete(uri: Uri) = DocumentsContract.deleteDocument(resolver, uri)
		override fun rename(uri: Uri, name: String) = DocumentsContract.renameDocument(resolver, uri, name)
		override fun move(uri: Uri, oldParent: Uri, newParent: Uri) = DocumentsContract.moveDocument(resolver, uri, oldParent, newParent)
	}
}
