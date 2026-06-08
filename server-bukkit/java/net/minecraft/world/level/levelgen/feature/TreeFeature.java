package net.minecraft.world.level.levelgen.feature;

import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
public record TreeFeature(Holder trunkProvider, Object trunkPlacer, Holder foliageProvider, Object foliagePlacer, Optional rootPlacer, Object minimumSize, List decorators, boolean ignoreVines, Holder belowTrunkProvider) implements Feature {
}
