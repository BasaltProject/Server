// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import net.minecraft.world.level.levelgen.feature.Feature;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
public class FeaturePlacer {
    private final WorldGenLevel level;
    private final ChunkGenerator generator;
    private boolean placeAll(PlacedFeature placed, RandomSource random, BlockPos origin, boolean biomeCheck) {
        Optional<PlacedFeature> top = biomeCheck ? Optional.of(placed) : Optional.empty();
        PlacementContext context = new PlacementContext(level, generator, top);
        Feature feature = (Feature) placed.feature().value();
        List<PlacementModifier> modifiers = (List<PlacementModifier>) placed.placement();
        java.util.ArrayList<BlockPos> stackPos = new java.util.ArrayList<>();
        java.util.ArrayList<Integer> stackStage = new java.util.ArrayList<>();
        stackPos.add(origin);
        stackStage.add(0);
        boolean any = false;
        List<BlockPos> produced = new java.util.ArrayList<>();
        while (!stackPos.isEmpty()) {
            BlockPos pos = stackPos.remove(stackPos.size() - 1);
            int stage = stackStage.remove(stackStage.size() - 1);
            if (stage >= modifiers.size()) {
                any |= feature.place(level, generator, random, pos);
                continue;
            }
            produced.clear();
            modifiers.get(stage).modify(context, random, pos, produced::add);
            for (int at = produced.size() - 1; at >= 0; at--) {
                stackPos.add(produced.get(at));
                stackStage.add(stage + 1);
            }
        }
        return any;
    }
    public FeaturePlacer(WorldGenLevel level, ChunkGenerator generator) {
        this.level = level;
        this.generator = generator;
    }
    public boolean place(PlacedFeature placedFeature, RandomSource random, BlockPos origin) {
        return placeAll(placedFeature, random, origin, false);
    }
    public boolean placeWithBiomeCheck(PlacedFeature placedFeature, RandomSource random, BlockPos origin) {
        return placeAll(placedFeature, random, origin, true);
    }
}
