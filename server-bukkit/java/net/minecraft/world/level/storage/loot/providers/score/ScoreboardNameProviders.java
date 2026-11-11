// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.score;
import com.mojang.datafixers.util.Either;
import net.minecraft.core.registries.BuiltInRegistries;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Registry;
public class ScoreboardNameProviders {
    public static final Codec<ScoreboardNameProvider> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.score.ScoreboardNameProviders.class, "CODEC");
    public static MapCodec<? extends ScoreboardNameProvider> bootstrap(Registry<MapCodec<? extends ScoreboardNameProvider>> registry) {
        Registry.register(registry, "fixed", FixedScoreboardNameProvider.MAP_CODEC);
        return Registry.register(registry, "context", ContextScoreboardNameProvider.MAP_CODEC);
    }
}
