// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.sheep;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.util.random.WeightedList;
import net.minecraft.tags.BiomeTags;
public class SheepColorSpawnRules {
    @FunctionalInterface
    private interface ColorSource {
        DyeColor get(RandomSource random);
    }
    private static final ColorSource kTemperate = mix(pick(DyeColor.BLACK, 5), pick(DyeColor.GRAY, 5), pick(DyeColor.LIGHT_GRAY, 5), pick(DyeColor.BROWN, 3), common(DyeColor.WHITE, 82));
    private static final ColorSource kWarm = mix(pick(DyeColor.GRAY, 5), pick(DyeColor.LIGHT_GRAY, 5), pick(DyeColor.WHITE, 5), pick(DyeColor.BLACK, 3), common(DyeColor.BROWN, 82));
    private static final ColorSource kCold = mix(pick(DyeColor.LIGHT_GRAY, 5), pick(DyeColor.GRAY, 5), pick(DyeColor.WHITE, 5), pick(DyeColor.BROWN, 3), common(DyeColor.BLACK, 82));
    private record Weighted(ColorSource source, int weight) {
    }
    private static Weighted pick(DyeColor color, int weight) {
        return new Weighted(random -> color, weight);
    }
    private static Weighted common(DyeColor color, int weight) {
        return new Weighted(mix(pick(color, 499), pick(DyeColor.PINK, 1)), weight);
    }
    private static ColorSource mix(Weighted... entries) {
        WeightedList.Builder<ColorSource> builder = WeightedList.builder();
        for (Weighted entry : entries) {
            builder.add(entry.source, entry.weight);
        }
        WeightedList<ColorSource> list = builder.build();
        return random -> list.getRandomOrThrow(random).get(random);
    }
    public static DyeColor getSheepColor(Holder<Biome> biome, RandomSource random) {
        ColorSource source = biome.is(BiomeTags.SPAWNS_WARM_VARIANT_FARM_ANIMALS) ? kWarm
            : biome.is(BiomeTags.SPAWNS_COLD_VARIANT_FARM_ANIMALS) ? kCold : kTemperate;
        return source.get(random);
    }
}
