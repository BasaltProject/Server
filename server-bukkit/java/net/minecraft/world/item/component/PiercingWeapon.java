// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
public record PiercingWeapon(boolean dealsKnockback, boolean dismounts, Optional<Holder<SoundEvent>> sound, Optional<Holder<SoundEvent>> hitSound) {
    public static final Codec<PiercingWeapon> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.PiercingWeapon.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, PiercingWeapon> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.PiercingWeapon.class, "STREAM_CODEC");
    public void makeSound(Entity causer) {
        sound.ifPresent(event -> causer.level().playSound(causer, causer.getX(), causer.getY(), causer.getZ(), event, causer.getSoundSource(), 1.0F, 1.0F));
    }
    public void makeHitSound(Entity causer) {
        hitSound.ifPresent(event -> causer.level().playSound(null, causer.getX(), causer.getY(), causer.getZ(), event, causer.getSoundSource(), 1.0F, 1.0F));
    }
    public static boolean canHitEntity(Entity jabber, Entity target) {
        if (target.isInvulnerableToPiercingWeapon() || !target.isAlive()) {
            return false;
        }
        if (target instanceof net.minecraft.world.entity.Interaction) {
            return true;
        }
        if (!target.canBeHitByProjectile()) {
            return false;
        }
        if (target instanceof net.minecraft.world.entity.player.Player victim
            && jabber instanceof net.minecraft.world.entity.player.Player jabbingPlayer && !jabbingPlayer.canHarmPlayer(victim)) {
            return false;
        }
        return !jabber.isPassengerOfSameVehicle(target);
    }
    public void attack(LivingEntity attacker, EquipmentSlot hand) {
        float damage = (float) attacker.getAttributeValue(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE);
        net.minecraft.world.item.ItemStack weapon = attacker.getItemBySlot(hand);
        AttackRange range = attacker.getAttackRangeWith(weapon);
        boolean hitSomething = false;
        var along = net.minecraft.world.entity.projectile.ProjectileUtil.getHitEntitiesAlong(attacker, range,
            target -> canHitEntity(attacker, target), net.minecraft.world.level.ClipContext.Block.COLLIDER);
        for (var hit : along.right().orElse(java.util.List.of())) {
            hitSomething |= attacker.stabAttack(hand, hit.getEntity(), damage, true, dealsKnockback, dismounts);
        }
        attacker.onAttack();
        attacker.postPiercingAttack();
        if (hitSomething) {
            makeHitSound(attacker);
        }
        makeSound(attacker);
        attacker.swingAndResetAttackStrength(net.minecraft.world.InteractionHand.MAIN_HAND, weapon.getAttackAnimation(), false);
    }
}
