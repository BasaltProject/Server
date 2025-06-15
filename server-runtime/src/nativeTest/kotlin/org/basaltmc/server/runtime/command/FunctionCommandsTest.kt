package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.command.commands.CommandStorage
import org.basaltmc.server.runtime.command.commands.DataAccess
import org.basaltmc.server.runtime.command.commands.DataCommand
import org.basaltmc.server.runtime.command.commands.DataPackCommand
import org.basaltmc.server.runtime.command.commands.FunctionCommand
import org.basaltmc.server.runtime.command.commands.FunctionRunner
import org.basaltmc.server.runtime.command.commands.FunctionSchedule
import org.basaltmc.server.runtime.command.commands.ReturnCommand
import org.basaltmc.server.runtime.command.commands.ScheduleCommand
import org.basaltmc.server.runtime.function.FunctionLibrary
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.io.FileIO
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

private class FunctionConsole : CommandSource {
    val messages: MutableList<String> = mutableListOf()

    override val name: String = "Server"
    override val displayName: Component = Component.text("Server")
    override val permissionLevel: Int = PermissionLevel.OWNERS
    override val position: Triple<Double, Double, Double> = Triple(0.0, 0.0, 0.0)
    override val player: PlayerSession? = null

    override fun hasPermission(node: String?): Boolean = true

    override fun sendSystemMessage(message: Component) {
        messages += PlainTextSerializer.serialize(message)
    }
}

/** Functions read from a datapack laid out exactly as vanilla expects one. */
class FunctionCommandsTest {
    private val root = "build/test-data/function-commands/datapacks"

    private val library: FunctionLibrary

    init {
        FileIO.createDirectories("$root/testpack/data/basalt/function/nested")
        FileIO.writeText("$root/testpack/pack.mcmeta", """{"pack":{"pack_format":81,"description":"test"}}""")
        FileIO.writeText(
            "$root/testpack/data/basalt/function/counter.mcfunction",
            """
            # a comment, which is skipped

            data modify storage basalt:test counter set value 1
            data modify storage basalt:test counter set value 2
            """.trimIndent(),
        )
        FileIO.writeText(
            "$root/testpack/data/basalt/function/early.mcfunction",
            """
            data modify storage basalt:test early set value 1
            return 7
            data modify storage basalt:test early set value 99
            """.trimIndent(),
        )
        FileIO.writeText(
            "$root/testpack/data/basalt/function/nested/inner.mcfunction",
            "data modify storage basalt:test nested set value 5",
        )
        FileIO.writeText(
            "$root/testpack/data/basalt/function/macro.mcfunction",
            "\$data modify storage basalt:test macro set value \$(amount)",
        )
        library = FunctionLibrary(root, Logger("FunctionCommandsTest").apply { minLevel = LogLevel.ERROR })
        library.reload()
    }

    private val storage = CommandStorage()
    private val runner = FunctionRunner(library)
    private val schedule = FunctionSchedule()

    private val dispatcher =
        CommandDispatcher(
            CommandEnvironment(
                players = PlayerRegistry(),
                groupsProvider = { testGroups() },
                worldSeed = 0L,
                maxPlayers = 20,
            ),
        ).also { it.register(DataCommand(storage)) }
            .also { it.register(FunctionCommand(library, runner, DataAccess(storage))) }
            .also { it.register(ReturnCommand(runner)) }
            .also { it.register(ScheduleCommand(library, schedule)) }
            .also { it.register(DataPackCommand(library)) }

    private val source = FunctionConsole()
    private val id = Identifier.parse("basalt:test")

    private fun run(command: String): Int = dispatcher.execute(source, command)

    private fun stored(key: String): Int? = (storage[id][key] as? NbtInt)?.value

    @Test
    fun aFunctionRunsEveryLineAndSkipsCommentsAndBlanks() {
        run("function basalt:counter")
        assertEquals(2, stored("counter"))
    }

    /** parity: a `$` line takes its `$(name)` from the arguments the call supplies. */
    @Test
    fun aMacroLineTakesItsArgumentsFromTheCall() {
        run("""function basalt:macro {amount:42}""")

        assertEquals(42, stored("macro"))
    }

    /** parity: `with storage`, which reads the arguments out of a holder instead of the line. */
    @Test
    fun aMacroCanReadItsArgumentsFromStorage() {
        run("""data modify storage basalt:args amount set value 7""")
        run("""function basalt:macro with storage basalt:args""")

        assertEquals(7, stored("macro"))
    }

    @Test
    fun aMacroWithoutArgumentsIsRefused() {
        assertEquals(0, run("function basalt:macro"))
        assertTrue(source.messages.any { "missing_arguments" in it })
    }

    @Test
    fun nestedDirectoriesBecomePartOfTheId() {
        run("function basalt:nested/inner")
        assertEquals(5, stored("nested"))
    }

    @Test
    fun returnStopsTheRestOfTheFunction() {
        assertEquals(7, run("function basalt:early"))
        assertEquals(1, stored("early"))
    }

    @Test
    fun returnOutsideAFunctionIsRefused() {
        run("return 3")
        assertTrue(source.messages.any { "/return only works inside a function" in it })
    }

    @Test
    fun anUnknownFunctionIsRefusedByName() {
        run("function basalt:missing")
        assertTrue(source.messages.any { "arguments.function.unknown" in it })
    }

    @Test
    fun scheduledFunctionsComeDueOnTheirTick() {
        run("schedule function basalt:counter 3")
        assertTrue(schedule.tick().isEmpty())
        assertTrue(schedule.tick().isEmpty())
        assertEquals(listOf(Identifier.parse("basalt:counter")), schedule.tick())
    }

    @Test
    fun scheduleClearForgetsIt() {
        run("schedule function basalt:counter 10")
        assertEquals(1, run("schedule clear basalt:counter"))
        assertTrue(schedule.tick().isEmpty())
    }

    @Test
    fun disablingAPackHidesItsFunctions() {
        assertEquals(1, run("datapack list enabled"))
        run("datapack disable testpack")
        run("function basalt:counter")
        assertTrue(source.messages.any { "arguments.function.unknown" in it })
        run("datapack enable testpack")
        assertEquals(1, run("datapack list enabled"))
    }

    @Test
    fun theStorageItselfIsACompound() {
        run("function basalt:counter")
        assertTrue(storage[id] is NbtCompound)
    }
}
