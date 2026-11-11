// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.grower;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.OptionalInt;
import net.minecraft.core.BlockPos;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.feature.Feature;
public final class TreeGrower {
    private final String kindName;
    private TreeGrower(String name) {
        this.kindName = name;
    }
    public static final Codec<TreeGrower> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.grower.TreeGrower.class, "CODEC");
    public static final TreeGrower OAK = new TreeGrower("oak");
    public static final TreeGrower SPRUCE = new TreeGrower("spruce");
    public static final TreeGrower MANGROVE = new TreeGrower("mangrove");
    public static final TreeGrower AZALEA = new TreeGrower("azalea");
    public static final TreeGrower BIRCH = new TreeGrower("birch");
    public static final TreeGrower JUNGLE = new TreeGrower("jungle");
    public static final TreeGrower ACACIA = new TreeGrower("acacia");
    public static final TreeGrower CHERRY = new TreeGrower("cherry");
    public static final TreeGrower DARK_OAK = new TreeGrower("dark_oak");
    public static final TreeGrower PALE_OAK = new TreeGrower("pale_oak");
    public static final TreeGrower POPLAR = new TreeGrower("poplar");
    public TreeGrower(String name, WeightedList<ResourceKey<Feature>> trees, WeightedList<ResourceKey<Feature>> megaTrees, WeightedList<ResourceKey<Feature>> flowerTrees, ResourceKey<Feature> shortestTreeType) {
        this.kindName = name;
    }
    public boolean growTree(ServerLevel level, ChunkGenerator generator, BlockPos pos, BlockState state, RandomSource random) {
        return org.basaltmc.bukkit.WorldEnvNatives.growTree(level.getWorld().getName(), pos.getX(), pos.getY(), pos.getZ(), kindName + "_sapling");
    }
    public OptionalInt getMinimumHeight(ServerLevel level) {
        return OptionalInt.empty();
    }
    public boolean canGrow(ServerLevel level, BlockPos pos, BlockState state) {
        return true;


    }
    static record TwoByTwoSaplingPos(int offsetX, int offsetZ, List<Pair<BlockState, BlockPos>> groundLevelSurroundingBlocks) {
    }
}
