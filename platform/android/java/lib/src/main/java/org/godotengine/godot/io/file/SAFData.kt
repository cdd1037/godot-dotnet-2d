/**************************************************************************/
/*  SAFData.kt                                                            */
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

package org.godotengine.godot.io.file

import android.content.Context
import android.os.ParcelFileDescriptor
import android.provider.DocumentsContract.Document
import android.util.Log
import org.godotengine.godot.io.SAFDocumentAccess
import org.godotengine.godot.io.SAFPath
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException
import java.nio.channels.FileChannel

/**
 * Implementation of [DataAccess] which handles file access via a content URI obtained using the Android
 * Storage Access Framework (SAF).
 */
internal class SAFData(context: Context, private val path: String, accessFlag: FileAccessFlags) :
	DataAccess.FileChannelDataAccess(path) {

	companion object {
		private val TAG = SAFData::class.java.simpleName

		fun fileExists(context: Context, path: String): Boolean = try {
			val parsed = SAFPath.parse(path)
			if (parsed.isTree) {
				SAFDocumentAccess(context).resolve(parsed)?.isDirectory == false
			} else {
				// Keep standalone content URI compatibility: non-document providers need
				// only implement the openable display-name column, not SAF flags/IDs.
				context.contentResolver.query(parsed.documentUri, arrayOf(Document.COLUMN_DISPLAY_NAME), null, null, null)
					?.use { it.moveToFirst() } == true && context.contentResolver.getType(parsed.documentUri) != Document.MIME_TYPE_DIR
			}
		} catch (e: Exception) {
			Log.d(TAG, "Error checking file existence", e)
			false
		}

		fun fileLastModified(context: Context, path: String): Long = try {
			val parsed = SAFPath.parse(path)
			val value = if (parsed.isTree) SAFDocumentAccess(context).resolve(parsed)?.lastModified ?: 0L
			else queryNumber(context, parsed, Document.COLUMN_LAST_MODIFIED, 0L)
			value / 1000L
		} catch (e: Exception) {
			Log.d(TAG, "Error reading last modified", e)
			0L
		}

		fun fileSize(context: Context, path: String): Long = try {
			val parsed = SAFPath.parse(path)
			if (parsed.isTree) SAFDocumentAccess(context).resolve(parsed)?.size ?: -1L
			else queryNumber(context, parsed, Document.COLUMN_SIZE, -1L)
		} catch (e: Exception) {
			Log.d(TAG, "Error reading file size", e)
			-1L
		}

		private fun queryNumber(context: Context, path: SAFPath, column: String, fallback: Long): Long {
			return context.contentResolver.query(path.documentUri, arrayOf(column), null, null, null)?.use {
				val index = it.getColumnIndex(column)
				if (it.moveToFirst() && index >= 0 && !it.isNull(index)) it.getLong(index) else fallback
			} ?: fallback
		}

		fun delete(context: Context, path: String): Boolean = try {
			SAFDocumentAccess(context).remove(SAFPath.parse(path), false)
		} catch (e: Exception) {
			Log.d(TAG, "Error deleting file", e)
			false
		}

		fun rename(context: Context, from: String, to: String): Boolean = try {
			SAFDocumentAccess(context).rename(SAFPath.parse(from), SAFPath.parse(to))
		} catch (e: Exception) {
			Log.d(TAG, "Error renaming file", e)
			false
		}
	}

	override val fileChannel: FileChannel
	val parcelFileDescriptor: ParcelFileDescriptor
	init {
		val uri = SAFDocumentAccess(context).openFile(SAFPath.parse(path), accessFlag != FileAccessFlags.READ)
		parcelFileDescriptor = context.contentResolver.openFileDescriptor(uri, accessFlag.getMode())
			?: throw IllegalStateException("Unable to access file descriptor")
		fileChannel = if (accessFlag == FileAccessFlags.READ) {
			FileInputStream(parcelFileDescriptor.fileDescriptor).channel
		} else {
			FileOutputStream(parcelFileDescriptor.fileDescriptor).channel
		}

		if (accessFlag.shouldTruncate()) {
			fileChannel.truncate(0)
		}
	}

	override fun close() {
		try {
			fileChannel.close()
		} catch (e: IOException) {
			Log.w(TAG, "Exception when closing file $path.", e)
		} finally {
			try {
				parcelFileDescriptor.close()
			} catch (e: IOException) {
				Log.w(TAG, "Exception when closing ParcelFileDescriptor for $path.", e)
			}
		}
	}
}
