// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import com.mojang.serialization.MapCodec;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.enchantment.EnchantedItemInUse;
import net.minecraft.world.item.enchantment.LevelBasedValue;
import net.minecraft.world.phys.Vec3;
public record ApplyExhaustion(LevelBasedValue amount) implements EnchantmentEntityEffect {
    public static final MapCodec<ApplyExhaustion> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.ApplyExhaustion.class, "CODEC");
    public void apply(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position) {
        if (entity instanceof net.minecraft.world.entity.player.Player player) {
            player.causeFoodExhaustion(amount.calculate(enchantmentLevel));
        }
    }
    public MapCodec<ApplyExhaustion> codec() {
        return (com.mojang.serialization.MapCodec) CODEC;
    }
}
