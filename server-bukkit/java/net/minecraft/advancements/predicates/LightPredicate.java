// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
public record LightPredicate(MinMaxBounds.Ints composite) {
    public static final Codec<LightPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        MinMaxBounds.Ints.CODEC.optionalFieldOf("light", MinMaxBounds.Ints.ANY).forGetter(LightPredicate::composite)).apply(i, LightPredicate::new));
    public boolean matches(ServerLevel level, BlockPos pos) {
        return level.isLoaded(pos) && composite.matches(level.getMaxLocalRawBrightness(pos));
    }
    public static class Builder {
        private MinMaxBounds.Ints composite = MinMaxBounds.Ints.ANY;
        public static Builder light() {
            return new Builder();
        }
        public Builder setComposite(MinMaxBounds.Ints composite) {
            this.composite = composite;
            return this;
        }
        public LightPredicate build() {
            return new LightPredicate(composite);
        }
    }
}
