package org.basaltmc.server.command

import kotlin.test.Test
import kotlin.test.assertEquals

class ConsoleCommandRegistryTest {
    @Test
    fun dispatchesToTheRegisteredCommandWithItsArguments() {
        val registry = ConsoleCommandRegistry()
        var received: List<String>? = null
        registry.register("say") { args -> received = args }

        registry.dispatch("say hello world")

        assertEquals(listOf("hello", "world"), received)
    }

    @Test
    fun commandNamesAreCaseInsensitive() {
        val registry = ConsoleCommandRegistry()
        var ran = false
        registry.register("stop") { ran = true }

        registry.dispatch("STOP")

        assertEquals(true, ran)
    }

    @Test
    fun unknownCommandsAndBlankLinesDoNotThrow() {
        val registry = ConsoleCommandRegistry()
        registry.dispatch("frobnicate")
        registry.dispatch("")
        registry.dispatch("   ")
    }
}
