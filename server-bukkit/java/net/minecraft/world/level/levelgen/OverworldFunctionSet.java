// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;
import java.util.function.Function;
public record OverworldFunctionSet<T>(T temperature, T vegetation, T continents, T erosion, T offset, T factor, T jaggedness, T depth, T slopedCheese, T preliminarySurfaceLevel, T chunkSurfaceLevel, T finalDensity) {
    public <U> OverworldFunctionSet<U> map(Function<T, U> function) {
        return new OverworldFunctionSet<U>(function.apply(temperature), function.apply(vegetation),
            function.apply(continents), function.apply(erosion), function.apply(offset), function.apply(factor),
            function.apply(jaggedness), function.apply(depth), function.apply(slopedCheese),
            function.apply(preliminarySurfaceLevel), function.apply(chunkSurfaceLevel), function.apply(finalDensity));
    }
}
