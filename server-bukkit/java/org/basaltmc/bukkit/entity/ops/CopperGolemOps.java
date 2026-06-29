package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.CopperGolem;

public interface CopperGolemOps extends CopperGolem, Cored {
    default io.papermc.paper.world.WeatheringCopperState getWeatheringState() {
        return io.papermc.paper.world.WeatheringCopperState.values()[Math.max(0,
            Math.min(3, core().integer("copper_weather")))];
    }

    default void setWeatheringState(io.papermc.paper.world.WeatheringCopperState state) {
        core().set("copper_weather", state.ordinal());
    }

    default CopperGolem.State getGolemState() {
        return CopperGolem.State.values()[Math.max(0, Math.min(CopperGolem.State.values().length - 1,
            core().integer("copper_pose")))];
    }

    default void setGolemState(CopperGolem.State state) {
        core().set("copper_pose", state.ordinal());
    }

    default CopperGolem.Oxidizing getOxidizing() {
        Object stored = core().property("copper_weathering_tick");
        long tick = stored instanceof Number number ? number.longValue() : -1L;
        return tick == -2L ? CopperGolem.Oxidizing.waxed() : tick < 0L ? CopperGolem.Oxidizing.unset()
            : CopperGolem.Oxidizing.atTime(tick);
    }

    default void setOxidizing(CopperGolem.Oxidizing oxidizing) {
        long tick = oxidizing instanceof CopperGolem.Oxidizing.AtTime at ? at.time()
            : oxidizing instanceof CopperGolem.Oxidizing.Waxed ? -2L : -1L;
        core().set("copper_weathering_tick", tick);
    }
}
