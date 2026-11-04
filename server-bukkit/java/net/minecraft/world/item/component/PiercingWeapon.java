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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.PiercingWeapon#canHitEntity");
    }
    public void attack(LivingEntity attacker, EquipmentSlot hand) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.PiercingWeapon#attack");
    }
}
