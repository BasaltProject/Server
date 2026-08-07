package org.basaltmc.assets

import org.basaltmc.common.logging.Logger
import org.basaltmc.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertTrue

class EulaFileTest {
    private val logger = Logger("EulaFileTest").apply { minLevel = org.basaltmc.common.logging.LogLevel.ERROR }

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
        assertTrue(content != null && content.contains("eula=false"))
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
    fun aCommentedEulaTrueLineDoesNotCount() {
        FileIO.writeText(EulaFile.PATH, "#eula=true\neula=false")
        assertEquals(false, EulaFile.isAccepted("#eula=true\neula=false"))
    }
}

private fun removeEulaFile() {
    platform.posix.remove(EulaFile.PATH)
}
