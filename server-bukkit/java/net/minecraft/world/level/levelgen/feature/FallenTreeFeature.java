package net.minecraft.world.level.levelgen.feature;

import java.util.List;
import net.minecraft.core.Holder;
public record FallenTreeFeature(Holder trunkProvider, Object logLength, List stumpDecorators, List logDecorators) implements Feature {
}
