// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import com.mojang.serialization.Codec;

public interface VerticalAnchor {
    Codec<VerticalAnchor> CODEC = Codec.xor(Absolute.CODEC, Codec.xor(AboveBottom.CODEC, Codec.xor(BelowTop.CODEC, RelativeToSeaLevel.CODEC)))
        .xmap(VerticalAnchor::joined, VerticalAnchor::split);
    VerticalAnchor BOTTOM = aboveBottom(0);
    VerticalAnchor TOP = belowTop(0);

    static VerticalAnchor absolute(int value) {
        return new Absolute(value);
    }

    static VerticalAnchor aboveBottom(int offset) {
        return new AboveBottom(offset);
    }

    static VerticalAnchor belowTop(int offset) {
        return new BelowTop(offset);
    }

    static VerticalAnchor bottom() {
        return BOTTOM;
    }

    static VerticalAnchor top() {
        return TOP;
    }

    static VerticalAnchor relativeToSeaLevel(int offset) {
        return new RelativeToSeaLevel(offset);
    }

    static VerticalAnchor seaLevel() {
        return relativeToSeaLevel(0);
    }

    int resolveY(WorldGenerationContext heightAccessor);

    private static VerticalAnchor joined(com.mojang.datafixers.util.Either<Absolute, com.mojang.datafixers.util.Either<AboveBottom, com.mojang.datafixers.util.Either<BelowTop, RelativeToSeaLevel>>> either) {
        return either.map(a -> a, rest -> rest.map(b -> b, tail -> tail.map(t -> t, r -> r)));
    }

    private static com.mojang.datafixers.util.Either<Absolute, com.mojang.datafixers.util.Either<AboveBottom, com.mojang.datafixers.util.Either<BelowTop, RelativeToSeaLevel>>> split(VerticalAnchor anchor) {
        return switch (anchor) {
            case Absolute a -> com.mojang.datafixers.util.Either.left(a);
            case AboveBottom b -> com.mojang.datafixers.util.Either.right(com.mojang.datafixers.util.Either.left(b));
            case BelowTop t -> com.mojang.datafixers.util.Either.right(com.mojang.datafixers.util.Either.right(com.mojang.datafixers.util.Either.left(t)));
            case RelativeToSeaLevel r -> com.mojang.datafixers.util.Either.right(com.mojang.datafixers.util.Either.right(com.mojang.datafixers.util.Either.right(r)));
            default -> throw new IllegalArgumentException("Unknown anchor " + anchor);
        };
    }

    public static record Absolute(int y) implements VerticalAnchor {
        public static final Codec<Absolute> CODEC = Codec.intRange(net.minecraft.world.level.dimension.DimensionType.MIN_Y, net.minecraft.world.level.dimension.DimensionType.MAX_Y).fieldOf("absolute").xmap(Absolute::new, Absolute::y).codec();

        public int resolveY(WorldGenerationContext heightAccessor) {
            return y;
        }
    }

    public static record AboveBottom(int offset) implements VerticalAnchor {
        public static final Codec<AboveBottom> CODEC = Codec.intRange(net.minecraft.world.level.dimension.DimensionType.MIN_Y, net.minecraft.world.level.dimension.DimensionType.MAX_Y).fieldOf("above_bottom").xmap(AboveBottom::new, AboveBottom::offset).codec();

        public int resolveY(WorldGenerationContext heightAccessor) {
            return heightAccessor.getMinGenY() + offset;
        }
    }

    public static record BelowTop(int offset) implements VerticalAnchor {
        public static final Codec<BelowTop> CODEC = Codec.intRange(net.minecraft.world.level.dimension.DimensionType.MIN_Y, net.minecraft.world.level.dimension.DimensionType.MAX_Y).fieldOf("below_top").xmap(BelowTop::new, BelowTop::offset).codec();

        public int resolveY(WorldGenerationContext heightAccessor) {
            return heightAccessor.getMinGenY() + heightAccessor.getGenDepth() - 1 - offset;
        }
    }

    public static record RelativeToSeaLevel(int offset) implements VerticalAnchor {
        public static final Codec<RelativeToSeaLevel> CODEC = Codec.intRange(net.minecraft.world.level.dimension.DimensionType.MIN_Y, net.minecraft.world.level.dimension.DimensionType.MAX_Y).fieldOf("relative_to_sea_level").xmap(RelativeToSeaLevel::new, RelativeToSeaLevel::offset).codec();

        public int resolveY(WorldGenerationContext heightAccessor) {
            return heightAccessor.seaLevel() + offset;
        }
    }
}
