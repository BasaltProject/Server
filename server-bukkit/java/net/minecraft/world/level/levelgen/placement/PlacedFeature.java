package net.minecraft.world.level.levelgen.placement;

import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import org.basaltmc.bukkit.Unsupported;
public record PlacedFeature(Holder feature, List placement) {
    public boolean place(WorldGenLevel level, ChunkGenerator generator, RandomSource random, BlockPos origin) {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.placement.PlacedFeature.place");
    }
}
