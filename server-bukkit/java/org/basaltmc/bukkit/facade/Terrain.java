package org.basaltmc.bukkit.facade;

import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.NoiseRouter;
import net.minecraft.world.level.levelgen.NoiseSettings;
import org.basaltmc.bukkit.LeftoverNatives;

/** The terrain shape of the game data's noise settings, with router functions the native density code evaluates. */
public final class Terrain {
    private static final Map<Identifier, Optional<NoiseGeneratorSettings>> kSettings = new ConcurrentHashMap<>();

    private Terrain() {}

    /** The settings of a game data noise settings id, or null when the data has none. */
    public static NoiseGeneratorSettings settings(Identifier id) {
        return Memo.get(kSettings, id, Terrain::build).orElse(null);
    }

    /** The game data id the settings were built from, the overworld when they are not native ones. */
    public static String nameOf(NoiseGeneratorSettings settings) {
        return settings.noiseRouter() != null && settings.noiseRouter().finalDensity() instanceof NativeDensityFunction function
            ? function.settings() : "minecraft:overworld";
    }

    private static Optional<NoiseGeneratorSettings> build(Identifier id) {
        String name = id.toString();
        int[] shape = LeftoverNatives.terrainShape(name);
        if (shape == null) {
            return Optional.empty();
        }
        NoiseRouter router = new NoiseRouter(function(name, "temperature"), function(name, "vegetation"),
            function(name, "continents"), function(name, "erosion"), function(name, "depth"), function(name, "ridges"),
            function(name, "preliminary_surface_level"), function(name, "final_density"));
        return Optional.of(new NoiseGeneratorSettings(NoiseSettings.create(shape[0], shape[1]),
            net.minecraft.world.level.block.Block.stateById(shape[3]), Block.stateById(shape[4]), router, null,
            List.of(), shape[2], shape[6] != 0, Optional.empty(), shape[5] != 0, NoiseGeneratorSettings.DebugFunctions.EMPTY));
    }

    private static NativeDensityFunction function(String settings, String name) {
        return new NativeDensityFunction(settings, 0L, name);
    }
}
