package org.basaltmc.io

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

class FileIOTest {
    @Test
    fun readsBackWhatItWrote() {
        val path = "basalt_fileio_test_roundtrip.txt"
        FileIO.writeText(path, "hello\nbasalt")
        assertEquals("hello\nbasalt", FileIO.readText(path))
        removeTestFile(path)
    }

    @Test
    fun reportsExistence() {
        val path = "basalt_fileio_test_exists.txt"
        assertFalse(FileIO.exists(path))
        FileIO.writeText(path, "x")
        assertTrue(FileIO.exists(path))
        removeTestFile(path)
    }

    @Test
    fun readTextReturnsNullForAMissingFile() {
        assertNull(FileIO.readText("basalt_fileio_test_does_not_exist.txt"))
    }

    @Test
    fun writeTextOverwritesExistingContent() {
        val path = "basalt_fileio_test_overwrite.txt"
        FileIO.writeText(path, "first, and much longer than what follows")
        FileIO.writeText(path, "second")
        assertEquals("second", FileIO.readText(path))
        removeTestFile(path)
    }

    @Test
    fun readsAnEmptyFile() {
        val path = "basalt_fileio_test_empty.txt"
        FileIO.writeText(path, "")
        assertEquals("", FileIO.readText(path))
        removeTestFile(path)
    }
}

private fun removeTestFile(path: String) {
    platform.posix.remove(path)
}
