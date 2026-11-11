// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import com.mojang.serialization.MapCodec;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.enchantment.EnchantedItemInUse;
import net.minecraft.world.item.enchantment.LevelBasedValue;
import net.minecraft.world.phys.Vec3;
public record ApplyEntityImpulse(Vec3 direction, Vec3 coordinateScale, LevelBasedValue magnitude) implements EnchantmentEntityEffect {
    public static final MapCodec<ApplyEntityImpulse> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.ApplyEntityImpulse.class, "CODEC");
    public void apply(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position) {
        var turn = entity.getLookQuaternion();
        var push = turn.transform(direction.x, direction.y, direction.z, new org.joml.Vector3d())
            .mul(coordinateScale.x, coordinateScale.y, coordinateScale.z).mul((double) magnitude.calculate(enchantmentLevel));
        entity.addDeltaMovement(new net.minecraft.world.phys.Vec3(push.x, push.y, push.z));
        entity.syncVelocity = !(entity instanceof net.minecraft.server.level.ServerPlayer);
        if (entity instanceof net.minecraft.world.entity.LivingEntity living) {
            living.applyPostImpulseGraceTime(10);
        }

    }
    public MapCodec<ApplyEntityImpulse> codec() {
        return (com.mojang.serialization.MapCodec) CODEC;
    }
}
