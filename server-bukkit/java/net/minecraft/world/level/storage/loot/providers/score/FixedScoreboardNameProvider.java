// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.score;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.scores.ScoreHolder;
public record FixedScoreboardNameProvider(String name) implements ScoreboardNameProvider {
    public static final MapCodec<FixedScoreboardNameProvider> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.score.FixedScoreboardNameProvider.class, "MAP_CODEC");
    public static ScoreboardNameProvider forName(String name) {
        return new FixedScoreboardNameProvider(name);
    }
    public MapCodec<FixedScoreboardNameProvider> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public ScoreHolder getScoreHolder(LootContext context) {
        return ScoreHolder.forNameOnly(name);
    }
}
