// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.valueproviders.FloatProvider;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.enchantment.EnchantedItemInUse;
import net.minecraft.world.phys.Vec3;
public record PlaySoundEffect(List<Holder<SoundEvent>> soundEvents, FloatProvider volume, FloatProvider pitch) implements EnchantmentEntityEffect {
    public static final MapCodec<PlaySoundEffect> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.PlaySoundEffect.class, "CODEC");
    public void apply(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position) {
        if (entity.isSilent()) {
            return;
        }
        var random = entity.getRandom();
        int pick = net.minecraft.util.Mth.clamp(enchantmentLevel - 1, 0, soundEvents.size() - 1);
        serverLevel.playSound(null, position.x(), position.y(), position.z(), soundEvents.get(pick), entity.getSoundSource(),
            volume.sample(random), pitch.sample(random));
    }
    public MapCodec<PlaySoundEffect> codec() {
        return (com.mojang.serialization.MapCodec) CODEC;
    }
}
