// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.variant;
import com.mojang.serialization.MapCodec;
import net.minecraft.advancements.predicates.MinMaxBounds;
public record MoonBrightnessCheck(MinMaxBounds.Doubles range) implements SpawnCondition {
    public static final MapCodec<MoonBrightnessCheck> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.entity.variant.MoonBrightnessCheck.class, "MAP_CODEC");
    public boolean test(SpawnContext context) {
        return range.matches(((net.minecraft.server.level.ServerLevel) context.level().getLevel()).getMoonBrightness(context.pos()));
    }
    public MapCodec<MoonBrightnessCheck> codec() {
        return MAP_CODEC;
    }
}
