// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;

import com.mojang.serialization.Codec;
import java.util.Optional;
import java.util.OptionalInt;
import net.minecraft.util.StringRepresentable;

public record BiomeSpecialEffects(int waterColor, Optional<Integer> foliageColorOverride, Optional<Integer> dryFoliageColorOverride, Optional<Integer> grassColorOverride, GrassColorModifier grassColorModifier) {
    public static final Codec<BiomeSpecialEffects> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(BiomeSpecialEffects.class, "CODEC");

    public static class Builder {
        private OptionalInt waterColor = OptionalInt.empty();
        private Optional<Integer> foliageColorOverride = Optional.empty();
        private Optional<Integer> dryFoliageColorOverride = Optional.empty();
        private Optional<Integer> grassColorOverride = Optional.empty();
        private GrassColorModifier grassColorModifier = GrassColorModifier.NONE;

        public Builder() {}

        public Builder waterColor(int waterColor) {
            this.waterColor = OptionalInt.of(waterColor);
            return this;
        }

        public Builder foliageColorOverride(int foliageColor) {
            foliageColorOverride = Optional.of(foliageColor);
            return this;
        }

        public Builder dryFoliageColorOverride(int dryFoliageColor) {
            dryFoliageColorOverride = Optional.of(dryFoliageColor);
            return this;
        }

        public Builder grassColorOverride(int grassColor) {
            grassColorOverride = Optional.of(grassColor);
            return this;
        }

        public Builder grassColorModifier(GrassColorModifier grassModifier) {
            this.grassColorModifier = grassModifier;
            return this;
        }

        public BiomeSpecialEffects build() {
            return new BiomeSpecialEffects(waterColor.orElseThrow(() -> new IllegalStateException("Missing 'water' color.")),
                foliageColorOverride, dryFoliageColorOverride, grassColorOverride, grassColorModifier);
        }
    }

    public enum GrassColorModifier implements StringRepresentable {
        NONE, DARK_FOREST, SWAMP;
        public static final Codec<GrassColorModifier> CODEC = StringRepresentable.fromEnum(GrassColorModifier::values);

        public int modifyColor(double x, double z, int baseColor) {
            return switch (this) {
                case NONE -> baseColor;
                case DARK_FOREST -> 0xFF000000 | (((baseColor & 0xFEFEFE) + 0x28340A) >> 1);
                case SWAMP -> Biome.BIOME_INFO_NOISE.get(x * 0.0225, z * 0.0225) < -0.1 ? 0xFF4C763C : 0xFF6A7039;
            };
        }

        public String getName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}
