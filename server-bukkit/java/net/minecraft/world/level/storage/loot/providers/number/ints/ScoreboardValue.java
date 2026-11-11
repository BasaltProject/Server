// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.scores.ReadOnlyScoreInfo;
import net.minecraft.world.scores.ScoreHolder;
import net.minecraft.world.scores.Scoreboard;
import net.minecraft.world.scores.Objective;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.providers.score.ScoreboardNameProvider;
public record ScoreboardValue(ScoreboardNameProvider target, String score, Holder<ContextIntProvider> fallback) implements ContextIntProvider {
    public static final MapCodec<ScoreboardValue> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.ints.ScoreboardValue.class, "MAP_CODEC");
    public MapCodec<ScoreboardValue> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public void validate(ValidationContext context) {
        Validatable.validate(context, "target", target);
        Validatable.validateHolder(context, "fallback", fallback);
    }
    public int getIntUnsafe(LootContext context) {
        ScoreHolder holder = target.getScoreHolder(context);
        ReadOnlyScoreInfo info = null;
        if (holder != null) {
            Scoreboard board = context.getLevel().getScoreboard();
            Objective objective = board.getObjective(score);
            info = objective == null ? null : board.getPlayerScoreInfo(holder, objective);
        }
        return info != null ? info.value() : fallback.value().getIntUnsafe(context);
    }
}
