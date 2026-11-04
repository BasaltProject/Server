// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
public record UseCooldown(float seconds, Optional<Identifier> cooldownGroup) {
    public static final Codec<UseCooldown> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.UseCooldown.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, UseCooldown> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.UseCooldown.class, "STREAM_CODEC");
    public UseCooldown(float seconds) {
        this(seconds, Optional.empty());
    }
    public int ticks() {
        return (int) (seconds * 20.0F);
    }
    public void apply(ItemStack stack, LivingEntity user) {
        if (user instanceof net.minecraft.world.entity.player.Player player && player.getBukkitEntity() instanceof org.bukkit.entity.Player bukkit) {
            bukkit.setCooldown(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(stack), ticks());
        }
    }
}
