// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import net.minecraft.world.scores.Objective;
import net.minecraft.world.scores.ReadOnlyScoreInfo;
import net.minecraft.world.scores.Scoreboard;
import net.minecraft.world.entity.Entity;
import com.mojang.serialization.MapCodec;
import java.util.Map;
import java.util.Set;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.level.storage.loot.IntRangePredicate;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
public record EntityHasScoreCondition(Map<String, IntRangePredicate> scores, LootContext.EntityTarget entityTarget) implements LootItemCondition {
    public static final MapCodec<EntityHasScoreCondition> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.EntityHasScoreCondition.class, "MAP_CODEC");
    public MapCodec<EntityHasScoreCondition> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return java.util.Set.of(entityTarget.contextParam());
    }
    public void validate(ValidationContext context) {
        LootItemCondition.super.validate(context);
        scores.forEach((score, value) -> value.validate(context.forMapField("scores", score)));
    }
    public boolean test(LootContext context) {
        Entity entity = context.getOptional(entityTarget.contextParam());
        if (entity == null) {
            return false;
        }
        Scoreboard board = context.getLevel().getScoreboard();
        for (Map.Entry<String, IntRangePredicate> entry : scores.entrySet()) {
            Objective objective = board.getObjective(entry.getKey());
            ReadOnlyScoreInfo info = objective == null ? null : board.getPlayerScoreInfo(entity, objective);
            if (info == null || !entry.getValue().test(context, info.value())) {
                return false;
            }
        }
        return true;
    }
    public static Builder hasScores(LootContext.EntityTarget target) {
        return new Builder(target);
    }
    public static class Builder implements LootItemCondition.Builder {
        private final Map<String, IntRangePredicate> collected = new java.util.LinkedHashMap<>();
        private final LootContext.EntityTarget entityTarget;
        public Builder(LootContext.EntityTarget entityTarget) {
            this.entityTarget = entityTarget;
            
        }
        public Builder withScore(String score, IntRangePredicate bounds) {
            collected.put(score, bounds);
            return this;
        }
        public LootItemCondition build() {
            return new EntityHasScoreCondition(Map.copyOf(collected), entityTarget);
        }
    }
}
