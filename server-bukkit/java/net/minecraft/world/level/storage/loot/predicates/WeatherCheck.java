// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import net.minecraft.server.level.ServerLevel;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.world.level.storage.loot.LootContext;
public record WeatherCheck(Optional<Boolean> isRaining, Optional<Boolean> isThundering) implements LootItemCondition {
    public static final MapCodec<WeatherCheck> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.WeatherCheck.class, "MAP_CODEC");
    public MapCodec<WeatherCheck> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public boolean test(LootContext context) {
        ServerLevel world = context.getLevel();
        if (isRaining.isPresent() && isRaining.get() != world.isRaining()) {
            return false;
        }
        return isThundering.isEmpty() || isThundering.get() == world.isThundering();
    }
    public static Builder weather() {
        return new Builder();
    }
    public static class Builder implements LootItemCondition.Builder {
        private Optional<Boolean> isRaining = Optional.empty();
        private Optional<Boolean> isThundering = Optional.empty();
        public Builder setRaining(boolean raining) {
            this.isRaining = Optional.of(raining);
            return this;
        }
        public Builder setThundering(boolean thundering) {
            this.isThundering = Optional.of(thundering);
            return this;
        }
        public WeatherCheck build() {
            return new WeatherCheck(isRaining, isThundering);
        }
    }
}
