package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.TimeArgument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.world.WorldWeather
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.MiniMessage

/**
 * parity: `WeatherCommand`. A duration of -1 means "pick one", which vanilla does with the same
 * ranges as the ordinary weather cycle.
 */
public class WeatherCommand : Command(name = "weather", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        for (kind in listOf("clear", "rain", "thunder")) {
            root.literal(kind) {
                executes { context -> set(context, kind, null) }
                argument("duration", TimeArgument(min = 1)) {
                    executes { context -> set(context, kind, context.get("duration")) }
                }
            }
        }
    }

    private fun set(
        context: CommandContext,
        kind: String,
        duration: Int?,
    ): Int {
        val world = context.world()
        val weather = world.weather
        // parity: each form draws from a different range when no duration was given, and `clear`
        // sets the clear countdown rather than a weather one.
        val ticks =
            duration ?: weather.randomDuration(
                when (kind) {
                    "clear" -> WorldWeather.RAIN_DELAY
                    "rain" -> WorldWeather.RAIN_DURATION
                    else -> WorldWeather.THUNDER_DURATION
                },
            )
        val change =
            context.environment.pluginEvents.emit(PluginEventTypes.WORLD_WEATHER_CHANGE) { event ->
                event.set(0, world.name)
                event.set(1, kind != "clear")
                event.set(2, kind == "thunder")
                event.set(3, weather.raining)
                event.set(4, weather.thundering)
            }
        if (change?.cancelled == true) {
            change.cancelMessage?.let { context.source.sendSystemMessage(MiniMessage.parse(it)) }
            return 0
        }
        when (kind) {
            "clear" -> weather.set(clearTime = ticks, weatherTime = 0, raining = false, thundering = false)
            "rain" -> weather.set(clearTime = 0, weatherTime = ticks, raining = true, thundering = false)
            else -> weather.set(clearTime = 0, weatherTime = ticks, raining = true, thundering = true)
        }
        context.environment.syncWeather(world)
        context.success(Component.translatable("commands.weather.set.$kind"), broadcastToAdmins = true)
        return duration ?: -1
    }
}
