package org.basaltmc.server.plugins

import kotlin.test.Test

/** The frozen Kotlin as the oracle for who is told about an event, in what order, and whether it goes ahead. */
class PluginEventsOracle {
    private val breakType =
        PluginEventType(
            name = "block.break",
            cancellable = true,
            fields = listOf("player", "world", "block", "x", "y", "z"),
            mutable = setOf("block"),
        )

    private val chatType =
        PluginEventType(name = "player.chat", cancellable = true, fields = listOf("player", "message"))

    private fun bus(): PluginEventBus =
        PluginEventBus().also {
            it.declare(breakType)
            it.declare(chatType)
        }

    private fun fill(
        event: PluginEvent,
        vararg values: Any?,
    ) {
        for ((index, value) in values.withIndex()) event.set(index, value)
    }

    @Test
    fun dump() {
        val bus = bus()
        println(
            "ORACLE declared ${bus.declared.map { "${it.name}/${it.cancellable}/${it.fields}/${it.mutable}" }}" +
                " of=${bus.typeOf("block.break")} unknown=${bus.typeOf("nope")}",
        )
        println(
            "ORACLE fields index=${breakType.indexOf("y")} missing=${breakType.indexOf("nope")}" +
                " listening=${bus.listening(breakType)}",
        )

        // Nobody listening: the fast path answers yes and leaves nothing behind.
        val event = bus.event(breakType)
        fill(event, "Whiron", "world", "minecraft:stone", 1L, 64L, -3L)
        println("ORACLE quiet ${bus.fire(event)} same=${bus.event(breakType) === event}")

        // The order handlers run in: deciders by priority, then observers.
        val order = mutableListOf<String>()
        val handles = mutableListOf<PluginHandle>()
        for (
        (name, priority, observer) in
        listOf(
            Triple("lateDecider", PluginEventPriority.LATE, false),
            Triple("observerFirst", PluginEventPriority.FIRST, true),
            Triple("firstDecider", PluginEventPriority.FIRST, false),
            Triple("normalDecider", PluginEventPriority.NORMAL, false),
            Triple("observerLast", PluginEventPriority.LAST, true),
        )
        ) {
            handles +=
                bus.add(
                    PluginEventHandler("p", 0, priority, observer, false, emptyMap()) { order += name },
                    breakType,
                )
        }
        fill(event, "Whiron", "world", "minecraft:stone", 1L, 64L, -3L)
        println("ORACLE order ${bus.fire(event)} $order listening=${bus.listening(breakType)}")

        // Two at the same priority run in the order they registered, not in some order the sort
        // happened to leave them in.
        val tied = bus()
        val tiedOrder = mutableListOf<String>()
        // Twenty, not three: a small list comes back in order from almost any sort, so a stable one
        // only proves itself past the size where the algorithm changes.
        for (name in (1..20).map { "h$it" }.shuffled(kotlin.random.Random(7))) {
            tied.add(
                PluginEventHandler("p", 0, PluginEventPriority.NORMAL, false, false, emptyMap()) { tiedOrder += name },
                breakType,
            )
        }
        val tiedEvent = tied.event(breakType)
        fill(tiedEvent, "Whiron", "world", "minecraft:stone", 1L, 64L, -3L)
        tied.fire(tiedEvent)
        println("ORACLE tied $tiedOrder")

        // What a handler sees of the event while it is running.
        val living = bus()
        val inside = mutableListOf<String>()
        living.add(
            PluginEventHandler("p", 0, PluginEventPriority.NORMAL, false, false, emptyMap()) {
                inside += "live=${it.live} generation=${it.generation}"
            },
            breakType,
        )
        val livingEvent = living.event(breakType)
        for (round in 1..3) {
            fill(livingEvent, "Whiron", "world", "minecraft:stone", 1L, 64L, -3L)
            living.fire(livingEvent)
        }
        println("ORACLE inside $inside after=${livingEvent.live}")

        // Removing one handle leaves the others alone.
        val partial = bus()
        val left = mutableListOf<String>()
        val goes =
            partial.add(
                PluginEventHandler("p", 0, PluginEventPriority.NORMAL, false, false, emptyMap()) { left += "goes" },
                breakType,
            )
        partial.add(
            PluginEventHandler("p", 0, PluginEventPriority.NORMAL, false, false, emptyMap()) { left += "stays" },
            breakType,
        )
        goes.cancel()
        val partialEvent = partial.event(breakType)
        fill(partialEvent, "Whiron", "world", "minecraft:stone", 1L, 64L, -3L)
        partial.fire(partialEvent)
        println("ORACLE partial $left listening=${partial.listening(breakType)}")

        // A handler that registers another one while the event is being dispatched.
        val growing = bus()
        val grown = mutableListOf<String>()
        growing.add(
            PluginEventHandler("p", 0, PluginEventPriority.FIRST, false, false, emptyMap()) {
                grown += "first"
                for (extra in 1..8) {
                    growing.add(
                        PluginEventHandler("p", 0, PluginEventPriority.LAST, false, false, emptyMap()) {
                            grown += "extra$extra"
                        },
                        breakType,
                    )
                }
            },
            breakType,
        )
        val growingEvent = growing.event(breakType)
        fill(growingEvent, "Whiron", "world", "minecraft:stone", 1L, 64L, -3L)
        growing.fire(growingEvent)
        println("ORACLE grown ${grown.size} first=${grown.first()}")

        // A decider that cancels: the ones after it are not told unless they asked to be.
        order.clear()
        handles +=
            bus.add(
                PluginEventHandler("p", 0, PluginEventPriority.EARLY, false, false, emptyMap()) {
                    order += "canceller"
                    it.cancel("not here")
                },
                breakType,
            )
        handles +=
            bus.add(
                PluginEventHandler("p", 0, PluginEventPriority.LAST, false, true, emptyMap()) {
                    order += "wantsCancelled"
                },
                breakType,
            )
        fill(event, "Whiron", "world", "minecraft:stone", 1L, 64L, -3L)
        val allowed = bus.fire(event)
        println("ORACLE cancelled $allowed $order message=${event.cancelMessage} flag=${event.cancelled}")

        // Firing again starts clean: the event is reused, so last time's answer must not linger.
        order.clear()
        for (handle in handles) handle.cancel()
        fill(event, "Whiron", "world", "minecraft:stone", 1L, 64L, -3L)
        println("ORACLE reused ${bus.fire(event)} cancelled=${event.cancelled} message=${event.cancelMessage}")

        // Filters: a handler that only wants one value is not woken for the others.
        val seen = mutableListOf<String>()
        bus.add(
            PluginEventHandler(
                "p",
                0,
                PluginEventPriority.NORMAL,
                false,
                false,
                mapOf(
                    "block" to "minecraft:diamond_ore",
                ),
            ) {
                seen += "diamond"
            },
            breakType,
        )
        bus.add(
            PluginEventHandler(
                "p",
                0,
                PluginEventPriority.NORMAL,
                false,
                false,
                mapOf("world" to "nether", "y" to 10L),
            ) {
                seen += "netherTen"
            },
            breakType,
        )
        for (block in listOf("minecraft:stone", "minecraft:diamond_ore")) {
            fill(event, "Whiron", "world", block, 1L, 64L, -3L)
            bus.fire(event)
        }
        fill(event, "Whiron", "nether", "minecraft:stone", 1L, 10L, -3L)
        bus.fire(event)
        println("ORACLE filtered $seen")

        // Unloading a plugin takes its handlers with it, and leaves the others alone.
        val busTwo = bus()
        val ran = mutableListOf<String>()
        busTwo.add(
            PluginEventHandler("arena", 0, PluginEventPriority.NORMAL, false, false, emptyMap()) {
                ran += "arena"
            },
            breakType,
        )
        busTwo.add(
            PluginEventHandler("shop", 0, PluginEventPriority.NORMAL, false, false, emptyMap()) {
                ran += "shop"
            },
            breakType,
        )
        busTwo.add(
            PluginEventHandler("arena", 0, PluginEventPriority.NORMAL, false, false, emptyMap()) {
                ran +=
                    "arenaChat"
            },
            chatType,
        )
        busTwo.removeAll("arena")
        val breakEvent = busTwo.event(breakType)
        fill(breakEvent, "Whiron", "world", "minecraft:stone", 1L, 64L, -3L)
        busTwo.fire(breakEvent)
        busTwo.fire(busTwo.event(chatType))
        println(
            "ORACLE unloaded $ran break=${busTwo.listening(breakType)} chat=${busTwo.listening(chatType)}",
        )

        // What a handler writes is what the server reads back.
        val busThree = bus()
        busThree.add(
            PluginEventHandler("p", 0, PluginEventPriority.NORMAL, false, false, emptyMap()) {
                it.set("block", "minecraft:cobblestone")
            },
            breakType,
        )
        val third = busThree.event(breakType)
        fill(third, "Whiron", "world", "minecraft:stone", 1L, 64L, -3L)
        busThree.fire(third)
        println(
            // No out-of-range index here: the frozen Kotlin reads its array unchecked and throws,
            // which is its own bug rather than anything to be faithful to. The C++ answers nil.
            "ORACLE mutated ${third.get("block")} byIndex=${third.get(2)}" +
                " missing=${third.get("nope")} live=${third.live}",
        )
        println(
            "ORACLE priorities ${PluginEventPriority.entries.map { it.name }} of=${PluginEventPriority.of(
                "early",
            )} unknown=${PluginEventPriority.of("monitor")}",
        )
    }
}
