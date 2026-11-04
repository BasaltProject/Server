// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.phys.Vec3;
public record KineticWeapon(int contactCooldownTicks, int delayTicks, Optional<Condition> dismountConditions, Optional<Condition> knockbackConditions, Optional<Condition> damageConditions, float forwardMovement, float damageMultiplier, Optional<Holder<SoundEvent>> sound, Optional<Holder<SoundEvent>> hitSound) {
    public static final int HIT_FEEDBACK_TICKS = 10;
    public static final Codec<KineticWeapon> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.KineticWeapon.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, KineticWeapon> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.KineticWeapon.class, "STREAM_CODEC");
    public static Vec3 getMotion(Entity livingEntity) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.KineticWeapon#getMotion");
    }
    public void makeSound(Entity causer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.KineticWeapon#makeSound");
    }
    public void makeLocalHitSound(Entity causer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.KineticWeapon#makeLocalHitSound");
    }
    public int computeDamageUseDuration() {
        return delayTicks + damageConditions.map(Condition::maxDurationTicks).orElse(0);
    }
    public void damageEntities(ItemStack stack, int ticksRemaining, LivingEntity livingEntity, EquipmentSlot equipmentSlot) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.KineticWeapon#damageEntities");
    }
    public static record Condition(int maxDurationTicks, float minSpeed, float minRelativeSpeed) {
        public static final Codec<Condition> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.KineticWeapon.Condition.class, "CODEC");
        public static final StreamCodec<ByteBuf, Condition> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.KineticWeapon.Condition.class, "STREAM_CODEC");
        public boolean test(int ticksUsed, double attackerSpeed, double relativeSpeed, double entityFactor) {
            return ticksUsed <= maxDurationTicks && attackerSpeed >= (double) minSpeed * entityFactor && relativeSpeed >= (double) minRelativeSpeed * entityFactor;
        }
        public static Optional<Condition> ofAttackerSpeed(int untilTicks, float minAttackerSpeed) {
            return Optional.of(new Condition(untilTicks, minAttackerSpeed, 0.0F));
        }
        public static Optional<Condition> ofRelativeSpeed(int untilTicks, float minRelativeSpeed) {
            return Optional.of(new Condition(untilTicks, 0.0F, minRelativeSpeed));
        }
    }
}
