/**************************************************************************/
/*  SAFDirectoryAccess.kt                                                 */
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

package org.godotengine.godot.io.directory

import android.content.Context
import android.util.Log
import org.godotengine.godot.io.SAFDocumentAccess
import org.godotengine.godot.io.SAFPath
import org.godotengine.godot.io.file.SAFData
import org.godotengine.godot.io.directory.DirectoryAccessHandler.Companion.INVALID_DIR_ID
import org.godotengine.godot.io.directory.DirectoryAccessHandler.Companion.STARTING_DIR_ID

/** Directory operations within a SAF tree selected by the user. */
internal class SAFDirectoryAccess(private val context: Context) : DirectoryAccessHandler.DirectoryAccess {
	private val documents = SAFDocumentAccess(context)

	companion object {
		private val TAG = SAFDirectoryAccess::class.java.simpleName
	}

	private data class DirData(val entries: List<SAFDocumentAccess.Entry>, var next: Int = 0, var current: Int = -1)
	private val dirs = mutableMapOf<Int, DirData>()
	private var lastDirId = STARTING_DIR_ID

	private inline fun <T> attempt(fallback: T, block: () -> T): T = try {
		block()
	} catch (e: Exception) {
		Log.d(TAG, "SAF directory operation failed", e)
		fallback
	}

	override fun dirOpen(path: String): Int = attempt(INVALID_DIR_ID) {
		// Only this directory is snapshotted. Name/type/hidden checks need no more
		// provider calls, and reopening always observes fresh provider data.
		val entries = documents.list(SAFPath.parse(path)) ?: return@attempt INVALID_DIR_ID
		// Native callers may pass a returned name back as a relative path. A
		// provider must not be able to turn it into navigation or an absolute path.
		if (entries.any { !SAFPath.isValidDirectoryEntryName(it.name) }) return@attempt INVALID_DIR_ID
		val id = ++lastDirId
		dirs[id] = DirData(entries)
		id
	}

	override fun dirNext(dirId: Int): String {
		val data = dirs[dirId] ?: return ""
		if (data.next >= data.entries.size) {
			data.current = -1
			return ""
		}
		data.current = data.next++
		return data.entries[data.current].name
	}

	override fun dirClose(dirId: Int) { dirs.remove(dirId) }
	override fun hasDirId(dirId: Int) = dirs.containsKey(dirId)
	private fun current(dirId: Int) = dirs[dirId]?.let { it.entries.getOrNull(it.current) }
	override fun dirIsDir(dirId: Int) = current(dirId)?.isDirectory == true
	override fun isCurrentHidden(dirId: Int) = current(dirId)?.name?.startsWith('.') == true
	override fun dirExists(path: String) = attempt(false) {
		val parsed = SAFPath.parse(path)
		parsed.isTree && documents.resolve(parsed)?.isDirectory == true
	}
	override fun fileExists(path: String) = SAFData.fileExists(context, path)
	override fun makeDir(dir: String) = attempt(false) { documents.makeDirectory(SAFPath.parse(dir), false) }
	fun makeDirRecursive(dir: String) = attempt(false) { documents.makeDirectory(SAFPath.parse(dir), true) }
	override fun remove(filename: String) = attempt(false) { documents.remove(SAFPath.parse(filename), true) }
	override fun rename(from: String, to: String) = attempt(false) { documents.rename(SAFPath.parse(from), SAFPath.parse(to)) }
	fun isReadable(path: String) = attempt(false) { documents.isReadable(SAFPath.parse(path)) }
	fun isWritable(path: String) = attempt(false) { documents.isWritable(SAFPath.parse(path)) }

	// A selected SAF tree is not a mounted filesystem. No volume or free-space
	// information is available from the DocumentsProvider contract.
	override fun getDriveCount() = 0
	override fun getDrive(drive: Int) = ""
	override fun getSpaceLeft() = 0L
}
