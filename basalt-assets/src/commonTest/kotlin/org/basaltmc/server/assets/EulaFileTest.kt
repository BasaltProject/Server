package org.basaltmc.server.assets

import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertTrue

class EulaFileTest {
    private val logger = Logger("EulaFileTest").apply { minLevel = org.basaltmc.wire.common.logging.LogLevel.ERROR }

    @AfterTest
    fun cleanup() {
        removeEulaFile()
    }

    @Test
    fun acceptFlagBypassesTheFileEntirely() {
        removeEulaFile()
        assertEquals(EulaFile.Result.Accepted, EulaFile.check(acceptFlag = true, logger = logger))
        assertEquals(null, FileIO.readText(EulaFile.PATH))
    }

    @Test
    fun missingFileGetsCreatedAndRejected() {
        removeEulaFile()
        val result = EulaFile.check(acceptFlag = false, logger = logger)
        assertIs<EulaFile.Result.Rejected>(result)
        assertTrue(result.justCreated)
        val content = FileIO.readText(EulaFile.PATH)
        assertTrue(content != null && content.contains("eula = false"), "generated file was: $content")
    }

    @Test
    fun fileWithEulaFalseIsRejectedButNotRecreated() {
        FileIO.writeText(EulaFile.PATH, "eula=false")
        val result = EulaFile.check(acceptFlag = false, logger = logger)
        assertIs<EulaFile.Result.Rejected>(result)
        assertEquals(false, result.justCreated)
    }

    @Test
    fun fileWithEulaTrueIsAccepted() {
        FileIO.writeText(EulaFile.PATH, "eula=true")
        assertEquals(EulaFile.Result.Accepted, EulaFile.check(acceptFlag = false, logger = logger))
    }

    @Test
    fun acceptedIsCaseInsensitiveAndToleratesWhitespace() {
        FileIO.writeText(EulaFile.PATH, "eula = TRUE")
        assertEquals(EulaFile.Result.Accepted, EulaFile.check(acceptFlag = false, logger = logger))
    }

    @Test
    fun anAcceptedLegacyFileIsHonoredAndMigrated() {
        removeEulaFile()
        FileIO.writeText(EulaFile.LEGACY_PATH, "eula=true")
        try {
            // Hosting panels write eula.txt themselves, and anyone arriving from vanilla already
            // has one: refusing to start over the file name would be a migration trap.
            assertEquals(EulaFile.Result.Accepted, EulaFile.check(acceptFlag = false, logger = logger))
            val migrated = FileIO.readText(EulaFile.PATH)
            assertTrue(migrated != null && migrated.contains("eula = true"), "migrated file was: $migrated")
        } finally {
            platform.posix.remove(EulaFile.LEGACY_PATH)
        }
    }

    @Test
    fun aLegacyFileThatWasNeverAcceptedStillRejects() {
        removeEulaFile()
        FileIO.writeText(EulaFile.LEGACY_PATH, "eula=false")
        try {
            val result = EulaFile.check(acceptFlag = false, logger = logger)
            assertIs<EulaFile.Result.Rejected>(result)
            assertTrue(result.justCreated)
        } finally {
            platform.posix.remove(EulaFile.LEGACY_PATH)
        }
    }

    @Test
    fun theNewFileWinsOverAStaleLegacyOne() {
        FileIO.writeText(EulaFile.PATH, "eula = false")
        FileIO.writeText(EulaFile.LEGACY_PATH, "eula=true")
        try {
            assertIs<EulaFile.Result.Rejected>(EulaFile.check(acceptFlag = false, logger = logger))
        } finally {
            platform.posix.remove(EulaFile.LEGACY_PATH)
        }
    }

    @Test
    fun aCommentedEulaTrueLineDoesNotCount() {
        FileIO.writeText(EulaFile.PATH, "#eula=true\neula=false")
        assertEquals(false, EulaFile.isAccepted("#eula=true\neula=false"))
    }
}

private fun removeEulaFile() {
    platform.posix.remove(EulaFile.PATH)
}
