// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import com.mojang.serialization.MapCodec;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.enchantment.EnchantedItemInUse;
import net.minecraft.world.item.enchantment.LevelBasedValue;
import net.minecraft.world.phys.Vec3;
public record ChangeItemDamage(LevelBasedValue amount) implements EnchantmentEntityEffect {
    public static final MapCodec<ChangeItemDamage> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.ChangeItemDamage.class, "CODEC");
    public void apply(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position) {
        var stack = item.itemStack();
        if (!stack.has(net.minecraft.core.component.DataComponents.MAX_DAMAGE) || !stack.has(net.minecraft.core.component.DataComponents.DAMAGE)) {
            return;
        }
        var holder = item.owner() instanceof net.minecraft.server.level.ServerPlayer sp ? sp : null;
        stack.hurtAndBreak((int) amount.calculate(enchantmentLevel), serverLevel, holder, item.onBreak());
    }
    public MapCodec<ChangeItemDamage> codec() {
        return (com.mojang.serialization.MapCodec) CODEC;
    }
}
