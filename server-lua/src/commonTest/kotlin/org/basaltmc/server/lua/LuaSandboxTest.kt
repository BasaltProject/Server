package org.basaltmc.server.lua

import net.blueva.luak.Varargs
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

class LuaSandboxTest {
    private fun run(
        sandbox: LuaSandbox,
        source: String,
        chunkName: String = "test",
    ): LuaFailure? {
        var failure: LuaFailure? = null
        val fn = sandbox.bind(sandbox.compile(source, chunkName))
        sandbox.calls().start(fn) { failure = it }
        return failure
    }

    @Test
    fun `the ways out of the sandbox are not there`() {
        val sandbox = LuaSandbox("test")
        val missing = mutableListOf<String>()
        sandbox.globals.set(
            "report",
            HostFunction("report") { args ->
                missing += args.tojstring(1)
                Lua.NONE
            },
        )
        val failure =
            run(
                sandbox,
                """
                for _, name in ipairs({ "io", "debug", "dofile", "loadfile", "collectgarbage" }) do
                    if _G[name] == nil then report(name) end
                end
                for _, name in ipairs({ "execute", "exit", "getenv", "remove", "rename" }) do
                    if os[name] == nil then report("os." .. name) end
                end
                """.trimIndent(),
            )
        assertNull(failure)
        assertEquals(10, missing.size, "still reachable: $missing")
    }

    @Test
    fun `what a plugin does need is still there`() {
        val sandbox = LuaSandbox("test")
        var answer = ""
        sandbox.globals.set(
            "answer",
            HostFunction("answer") { args ->
                answer = args.tojstring(1)
                Lua.NONE
            },
        )
        val failure =
            run(
                sandbox,
                """
                local parts = { string.upper("ok"), #table.concat({ "a", "b" }), math.max(1, 2), utf8.len("hey") }
                local co = coroutine.create(function() coroutine.yield("y") end)
                local _, y = coroutine.resume(co)
                answer(table.concat(parts, ",") .. "," .. y .. "," .. tostring(os.time() > 0))
                """.trimIndent(),
            )
        assertNull(failure)
        assertEquals("OK,2,2,3,y,true", answer)
    }

    @Test
    fun `an endless loop runs out of instructions instead of the server`() {
        val sandbox = LuaSandbox("test", LuaLimits(instructionsPerTurn = 100_000))
        val failure = run(sandbox, "while true do end")
        assertNotNull(failure)
        assertTrue(failure.outOfBudget, failure.report)
    }

    @Test
    fun `pcall is not a way around an exhausted budget`() {
        val sandbox = LuaSandbox("test", LuaLimits(instructionsPerTurn = 100_000))
        val failure =
            run(
                sandbox,
                """
                while true do
                    pcall(function() while true do end end)
                end
                """.trimIndent(),
            )
        assertNotNull(failure)
        assertTrue(failure.outOfBudget, failure.report)
    }

    @Test
    fun `the next turn gets its instructions back`() {
        val sandbox = LuaSandbox("test", LuaLimits(instructionsPerTurn = 200_000))
        assertNotNull(run(sandbox, "while true do end"))
        var counted = 0
        sandbox.globals.set(
            "tick",
            HostFunction("tick") {
                counted++
                Lua.NONE
            },
        )
        assertNull(run(sandbox, "for i = 1, 10 do tick() end"))
        assertEquals(10, counted)
    }

    /**
     * Luak charges what it allocates to whichever state is "current", and a state is made current
     * by its own synchronous entry points. Entering through `invokeSuspend`, which is what lets a
     * plugin wait, goes around them, so what a plugin allocates is charged to nobody and the
     * ceiling never fires. Asked upstream; the day it lands this test starts failing and says so.
     */
    @Test
    fun `the memory ceiling does not reach code entered as a coroutine yet`() {
        val sandbox = LuaSandbox("test", LuaLimits(instructionsPerTurn = 50_000_000, memoryBytes = 64L * 1024))
        val failure =
            run(
                sandbox,
                """
                local t = {}
                for i = 1, 100000 do t[i] = { i, i, i } end
                """.trimIndent(),
            )
        assertNull(failure, "the ceiling fires now: tighten this test and the sandbox with it")
    }

    @Test
    fun `binary chunks are refused`() {
        val sandbox = LuaSandbox("test")
        var message: String? = null
        sandbox.globals.set(
            "report",
            HostFunction("report") { args ->
                message = args.tojstring(1)
                Lua.NONE
            },
        )
        val failure =
            run(
                sandbox,
                """
                local chunk = string.dump(function() return 1 end)
                local f, err = load(chunk)
                report(tostring(f) .. "|" .. tostring(err))
                """.trimIndent(),
            )
        assertNull(failure)
        assertTrue(message?.startsWith("nil|") == true, "load took a binary chunk: $message")
    }

    @Test
    fun `require sees the package and nothing else`() {
        val files = mapOf("kits.lua" to "return { starter = 'stone' }".encodeToByteArray())
        val sandbox = LuaSandbox("test", sources = { name -> files[name] })
        var answer: String? = null
        sandbox.globals.set(
            "answer",
            HostFunction("answer") { args ->
                answer = args.tojstring(1)
                Lua.NONE
            },
        )
        assertNull(run(sandbox, "answer(require('kits').starter)"))
        assertEquals("stone", answer)

        val outside = run(sandbox, "require('secrets')")
        assertNotNull(outside)
        assertTrue(outside.report.contains("not found"), outside.report)
    }

    @Test
    fun `print goes to the plugin log and not the console`() {
        val lines = mutableListOf<String>()
        val sandbox = LuaSandbox("test", printLine = { lines += it })
        assertNull(run(sandbox, "print('hello', 2)"))
        assertEquals(listOf("hello\t2"), lines)
    }

    @Test
    fun `a plugin waits without holding the turn`() {
        val sandbox = LuaSandbox("test")
        val pending = mutableListOf<() -> Unit>()
        val marks = mutableListOf<String>()
        sandbox.globals.set("wait", TestWait(sandbox, pending))
        sandbox.globals.set(
            "mark",
            HostFunction("mark") { args ->
                marks += args.tojstring(1)
                Lua.NONE
            },
        )
        var failure: LuaFailure? = null
        var finished = false
        val fn =
            sandbox.bind(
                sandbox.compile(
                    """
                    mark("one")
                    wait(1)
                    mark("two")
                    wait(1)
                    mark("three")
                    """.trimIndent(),
                    "test",
                ),
            )
        sandbox.calls().start(fn) {
            failure = it
            finished = true
        }

        assertEquals(listOf("one"), marks, "the turn should have ended at the first wait")
        assertFalse(finished)
        pending.removeFirst().invoke()
        assertEquals(listOf("one", "two"), marks)
        pending.removeFirst().invoke()
        assertEquals(listOf("one", "two", "three"), marks)
        assertTrue(finished)
        assertNull(failure)
    }

    @Test
    fun `waiting where waiting is not allowed says so`() {
        val sandbox = LuaSandbox("test")
        sandbox.globals.set("wait", TestWait(sandbox, mutableListOf()))
        val failure =
            run(
                sandbox,
                """
                local t = { 3, 1, 2 }
                table.sort(t, function(a, b) wait(1) return a < b end)
                """.trimIndent(),
            )
        assertNotNull(failure)
        // Lua's own wording for it, which is what a script that handles the error already expects.
        assertTrue(failure.report.contains("attempt to yield across"), failure.report)
    }

    @Test
    fun `an error carries the plugin's own file and line`() {
        val sandbox = LuaSandbox("arena")
        val failure = run(sandbox, "local x = nil\nreturn x.field", "@arena/src/main.lua")
        assertNotNull(failure)
        assertTrue(failure.report.startsWith("[arena] "), failure.report)
        assertTrue(failure.report.contains("arena/src/main.lua:2"), failure.report)
    }

    /** Stands in for the real `wait`: parks the plugin and hands the host something to resume. */
    private class TestWait(
        private val sandbox: LuaSandbox,
        private val pending: MutableList<() -> Unit>,
    ) : WaitingFunction("wait") {
        override suspend fun waiting(args: Varargs): Varargs {
            pauseUntil { resume ->
                pending +=
                    {
                        sandbox.refill()
                        resume()
                    }
            }
            return Lua.NONE
        }
    }
}
