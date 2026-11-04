// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.consume_effects;
import net.minecraft.sounds.SoundSource;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.util.Mth;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
public record TeleportRandomlyConsumeEffect(float diameter, boolean directionalParticles) implements ConsumeEffect {
    public static final MapCodec<TeleportRandomlyConsumeEffect> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.consume_effects.TeleportRandomlyConsumeEffect.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, TeleportRandomlyConsumeEffect> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.consume_effects.TeleportRandomlyConsumeEffect.class, "STREAM_CODEC");
    public TeleportRandomlyConsumeEffect() {
        this(16.0F, false);
    }
    public ConsumeEffect.Type<TeleportRandomlyConsumeEffect> getType() {
        return ConsumeEffect.Type.TELEPORT_RANDOMLY;
    }
    public boolean apply(Level level, ItemStack stack, LivingEntity user) {
        for (int attempt = 0; attempt < 16; attempt++) {
            double x = user.getX() + (user.getRandom().nextDouble() - 0.5) * diameter;
            double y = Mth.clamp(user.getY() + (user.getRandom().nextDouble() - 0.5) * diameter, level.getMinY(), level.getMinY() + level.getHeight() - 1);
            double z = user.getZ() + (user.getRandom().nextDouble() - 0.5) * diameter;
            if (user.isPassenger()) {
                user.stopRiding();
            }
            if (user.randomTeleport(x, y, z, true, net.minecraft.tags.BlockTags.CONSUMABLE_DOES_NOT_TELEPORT_TO)) {
                boolean fox = user instanceof net.minecraft.world.entity.animal.fox.Fox;
                level.playSound(null, user.getX(), user.getY(), user.getZ(), fox ? SoundEvents.FOX_TELEPORT : SoundEvents.CHORUS_FRUIT_TELEPORT,
                    fox ? SoundSource.NEUTRAL : SoundSource.PLAYERS);
                user.resetFallDistance();
                return true;
            }
        }
        return true;
    }
}
