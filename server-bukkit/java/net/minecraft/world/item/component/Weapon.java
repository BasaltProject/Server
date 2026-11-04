// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
public record Weapon(int itemDamagePerAttack, float disableBlockingForSeconds) {
    public static final float AXE_DISABLES_BLOCKING_FOR_SECONDS = 5.0F;
    public static final Codec<Weapon> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.Weapon.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Weapon> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.Weapon.class, "STREAM_CODEC");
    public Weapon(int damagePerAttack) {
        this(damagePerAttack, 0.0F);
    }
}
