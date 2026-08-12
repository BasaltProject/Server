package org.basaltmc.server.runtime.config

import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.io.FileIO
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

private fun silent() = Logger("GroupsConfigTest").apply { minLevel = LogLevel.ERROR }

private fun load(
    name: String,
    contents: String? = null,
): GroupsConfig {
    val directory = ConfigDirectory("build/test-data/groups_$name", silent())
    val path = directory.pathOf(GroupsConfig.FILE_NAME)
    platform.posix.remove(path)
    if (contents != null) {
        FileIO.createDirectories(directory.root)
        FileIO.writeText(path, contents)
    }
    return GroupsConfig.load(directory, silent())
}

class GroupsConfigTest {
    @Test
    fun theGeneratedFileShipsWithADefaultAndAnOperatorGroup() {
        val groups = load("generated")

        assertEquals("default", groups.defaultGroup.name)
        assertEquals("operator", groups.operatorGroup.name)
        // The point of shipping both written out: opening or closing something for everyone is
        // editing a group that is already there, not inventing the format from scratch.
        assertEquals(true, groups.resolve("default", "basalt.command.help"))
        assertEquals(false, groups.resolve("default", "basalt.command.stop"))
        assertEquals(true, groups.resolve("operator", "basalt.command.stop"))
    }

    @Test
    fun aWildcardGrantsEverythingBelowIt() {
        val groups = load("wildcard")
        assertEquals(true, groups.resolve("operator", "basalt.command.anything"))
        assertEquals(true, groups.resolve("operator", "basalt.something.else.entirely"))
    }

    @Test
    fun aMoreSpecificDenialBeatsABroaderGrantWhateverTheOrder() {
        // Written grant-first here; the resolution must not depend on that.
        val groups =
            load(
                "denial",
                """
                [groups.default]
                default = true

                [groups.operator]
                operator = true
                permissions = ["basalt.*", "-basalt.command.stop"]
                """.trimIndent(),
            )

        assertEquals(true, groups.resolve("operator", "basalt.command.kick"))
        assertEquals(false, groups.resolve("operator", "basalt.command.stop"))
    }

    @Test
    fun aGroupOverridesWhatItInherits() {
        val groups =
            load(
                "inheritance",
                """
                [groups.default]
                default = true
                permissions = ["basalt.command.help", "-basalt.command.kick"]

                [groups.operator]
                operator = true
                permissions = ["*"]

                [groups.moderator]
                inherits = "default"
                permissions = ["basalt.command.kick"]
                """.trimIndent(),
            )

        // Same specificity as the inherited denial, so the group's own rule has to win or a child
        // could never take back something its parent denied.
        assertEquals(true, groups.resolve("moderator", "basalt.command.kick"))
        assertEquals(true, groups.resolve("moderator", "basalt.command.help"))
    }

    @Test
    fun aNodeNobodyGrantedIsDenied() {
        // No hidden ranking behind the groups: what a group can do is exactly what its file says.
        assertEquals(false, load("silent").resolve("default", "basalt.command.gamemode"))
    }

    @Test
    fun theOperatorGroupCoversPermissionsThatAreNotBasaltsOwn() {
        assertEquals(true, load("star").resolve("operator", "someplugin.admin.reload"))
    }

    @Test
    fun anUnknownGroupFallsBackToTheDefaultOne() {
        val groups = load("unknown_group")
        assertEquals(groups.defaultGroup, groups["does-not-exist"])
        assertEquals(groups.defaultGroup, groups[null])
    }

    @Test
    fun aFileMissingItsDefaultOrOperatorGroupIsRejected() {
        val failure =
            assertFailsWith<TomlFormatException> {
                load("no_default", "[groups.a]\npermissions = []\n[groups.b]\noperator = true\n")
            }
        assertTrue(failure.message.orEmpty().contains("default"))

        assertFailsWith<TomlFormatException> {
            load("two_defaults", "[groups.a]\ndefault = true\noperator = true\n[groups.b]\ndefault = true\n")
        }
    }

    @Test
    fun inheritanceThatLoopsOrPointsNowhereIsRejectedRatherThanLoopingForever() {
        assertFailsWith<TomlFormatException> {
            load(
                "cycle",
                """
                [groups.default]
                default = true
                inherits = "operator"

                [groups.operator]
                operator = true
                inherits = "default"
                """.trimIndent(),
            )
        }
        assertFailsWith<TomlFormatException> {
            load(
                "missing_parent",
                """
                [groups.default]
                default = true
                inherits = "ghost"

                [groups.operator]
                operator = true
                """.trimIndent(),
            )
        }
    }
}
