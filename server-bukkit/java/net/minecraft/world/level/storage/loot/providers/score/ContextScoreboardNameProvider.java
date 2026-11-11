// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.score;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.Set;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.scores.ScoreHolder;
public record ContextScoreboardNameProvider(LootContext.EntityTarget target) implements ScoreboardNameProvider {
    public static final MapCodec<ContextScoreboardNameProvider> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.score.ContextScoreboardNameProvider.class, "MAP_CODEC");
    public static final Codec<ContextScoreboardNameProvider> INLINE_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.score.ContextScoreboardNameProvider.class, "INLINE_CODEC");
    public static ScoreboardNameProvider forTarget(LootContext.EntityTarget target) {
        return new ContextScoreboardNameProvider(target);
    }
    public MapCodec<ContextScoreboardNameProvider> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public ScoreHolder getScoreHolder(LootContext context) {
        return (ScoreHolder) context.getOptional(target.contextParam());
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return java.util.Set.of(target.contextParam());
    }
}
