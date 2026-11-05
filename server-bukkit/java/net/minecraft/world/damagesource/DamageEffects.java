// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.damagesource;
import com.mojang.serialization.Codec;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.StringRepresentable;
public enum DamageEffects implements StringRepresentable {
    HURT("entity.generic.hurt"), THORNS("enchant.thorns.hit"), DROWNING("entity.generic.drown"), BURNING("entity.generic.burn"),
    POKING("block.sweet_berry_bush.hurt"), FREEZING("entity.player.hurt_freeze");
    private final String soundId;
    DamageEffects(String soundId) {
        this.soundId = soundId;
    }
    public static final Codec<DamageEffects> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.damagesource.DamageEffects.class, "CODEC");
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
    public SoundEvent sound() {
        return net.minecraft.sounds.SoundEvents.byId(soundId);
    }
}
