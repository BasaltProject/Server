// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.function.Function;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.enchantment.EnchantedItemInUse;
import net.minecraft.world.phys.Vec3;
public interface AllOf {
    static <T, A extends T> MapCodec<A> codec(Codec<T> topLevelCodec, Function<List<T>, A> constructor, Function<A, List<T>> accessor) {
        return RecordCodecBuilder.<A>mapCodec(instance -> instance.group(topLevelCodec.listOf().fieldOf("effects").forGetter(accessor)).apply(instance, constructor));
    }
    static EntityEffects entityEffects(EnchantmentEntityEffect... effects) {
        return new EntityEffects(List.of(effects));
    }
    static LocationBasedEffects locationBasedEffects(EnchantmentLocationBasedEffect... effects) {
        return new LocationBasedEffects(List.of(effects));
    }
    static ValueEffects valueEffects(EnchantmentValueEffect... effects) {
        return new ValueEffects(List.of(effects));
    }
    public static record EntityEffects(List<EnchantmentEntityEffect> effects) implements EnchantmentEntityEffect {
        public static final MapCodec<EntityEffects> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.AllOf.EntityEffects.class, "CODEC");
        public void apply(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position) {
            for (EnchantmentEntityEffect each : effects) {
                each.apply(serverLevel, enchantmentLevel, item, entity, position);
            }
        }
        public MapCodec<EntityEffects> codec() {
            return (com.mojang.serialization.MapCodec) CODEC;
        }
    }
    public static record LocationBasedEffects(List<EnchantmentLocationBasedEffect> effects) implements EnchantmentLocationBasedEffect {
        public static final MapCodec<LocationBasedEffects> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.AllOf.LocationBasedEffects.class, "CODEC");
        public void onChangedBlock(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position, boolean becameActive) {
            for (EnchantmentLocationBasedEffect each : effects) {
                each.onChangedBlock(serverLevel, enchantmentLevel, item, entity, position, becameActive);
            }
        }
        public void onDeactivated(EnchantedItemInUse item, Entity entity, Vec3 position, int level) {
            for (EnchantmentLocationBasedEffect each : effects) {
                each.onDeactivated(item, entity, position, level);
            }
        }
        public MapCodec<LocationBasedEffects> codec() {
            return (com.mojang.serialization.MapCodec) CODEC;
        }
    }
    public static record ValueEffects(List<EnchantmentValueEffect> effects) implements EnchantmentValueEffect {
        public static final MapCodec<ValueEffects> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.AllOf.ValueEffects.class, "CODEC");
        public float process(int enchantmentLevel, RandomSource random, float value) {
            for (EnchantmentValueEffect each : effects) {
                value = each.process(enchantmentLevel, random, value);
            }
            return value;


        }
        public MapCodec<ValueEffects> codec() {
            return (com.mojang.serialization.MapCodec) CODEC;
        }
    }
}
