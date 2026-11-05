// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.damagesource;
import com.mojang.serialization.Codec;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
public record DamageType(String msgId, DamageScaling scaling, float exhaustion, DamageEffects effects, DeathMessageType deathMessageType) {
    public static final Codec<DamageType> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.damagesource.DamageType.class, "DIRECT_CODEC");
    public static final Codec<Holder<DamageType>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.damagesource.DamageType.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<DamageType>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.damagesource.DamageType.class, "STREAM_CODEC");
    public DamageType(String msgdId, DamageScaling scaling, float exhaustion) {
        this(msgdId, scaling, exhaustion, DamageEffects.HURT, DeathMessageType.DEFAULT);
    }
    public DamageType(String msgdId, DamageScaling scaling, float exhaustion, DamageEffects effects) {
        this(msgdId, scaling, exhaustion, effects, DeathMessageType.DEFAULT);
    }
    public DamageType(String msgdId, float exhaustion, DamageEffects effects) {
        this(msgdId, DamageScaling.WHEN_CAUSED_BY_LIVING_NON_PLAYER, exhaustion, effects, DeathMessageType.DEFAULT);
    }
    public DamageType(String msgdId, float exhaustion) {
        this(msgdId, DamageScaling.WHEN_CAUSED_BY_LIVING_NON_PLAYER, exhaustion, DamageEffects.HURT, DeathMessageType.DEFAULT);
    }
}
