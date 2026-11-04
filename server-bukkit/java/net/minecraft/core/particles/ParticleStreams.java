// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;

import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;

final class ParticleStreams {
    private ParticleStreams() {}

    @SuppressWarnings({"unchecked", "rawtypes"})
    static StreamCodec<? super RegistryFriendlyByteBuf, ?> of(ParticleType<?> type) {
        return switch (type.key()) {
            case "block", "block_marker", "falling_dust", "dust_pillar", "block_crumble" -> BlockParticleOption.streamCodec((ParticleType) type);
            case "dust" -> DustParticleOptions.STREAM_CODEC;
            case "dust_color_transition" -> DustColorTransitionOptions.STREAM_CODEC;
            case "effect", "instant_effect" -> SpellParticleOption.streamCodec((ParticleType) type);
            case "entity_effect", "tinted_leaves", "flash" -> ColorParticleOption.streamCodec((ParticleType) type);
            case "dragon_breath" -> PowerParticleOption.streamCodec((ParticleType) type);
            case "sculk_charge" -> SculkChargeParticleOptions.STREAM_CODEC;
            case "shriek" -> ShriekParticleOption.STREAM_CODEC;
            case "geyser", "geyser_plume" -> GeyserParticleOptions.streamCodec((ParticleType) type);
            case "geyser_base", "geyser_poof" -> GeyserBaseParticleOptions.streamCodec((ParticleType) type);
            case "trail" -> TrailParticleOption.STREAM_CODEC;
            case "item" -> ItemParticleOption.streamCodec((ParticleType) type);
            case "vibration" -> VibrationParticleOption.STREAM_CODEC;
            default -> throw new UnsupportedOperationException("Particle " + type.key() + " has no network codec in the Basalt facade");
        };
    }
}
