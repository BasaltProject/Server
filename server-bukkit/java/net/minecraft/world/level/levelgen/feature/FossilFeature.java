// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureProcessor;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
import org.apache.commons.lang3.mutable.MutableInt;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructurePlaceSettings;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureProcessorList;
public record FossilFeature(List<Identifier> fossilStructures, List<Identifier> overlayStructures, Holder<StructureProcessorList> fossilProcessors, Holder<StructureProcessorList> overlayProcessors, int maxEmptyCornersAllowed) implements Feature {
    private static int emptyCorners(WorldGenLevel level, BoundingBox box) {
        MutableInt count = new MutableInt(0);
        box.forAllCorners(pos -> {
            BlockState state = level.getBlockState(pos);
            if (state.isAir() || state.is(Blocks.LAVA) || state.is(Blocks.WATER)) {
                count.add(1);
            }
        });
        return count.intValue();
    }


    public static final MapCodec<FossilFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.FossilFeature.class, "CODEC");
    public MapCodec<FossilFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        Rotation rotation = Rotation.getRandom(random);
        int pick = random.nextInt(fossilStructures.size());
        StructureTemplateManager manager = level.getLevel().getServer().getStructureTemplateManager();
        StructureTemplate body = manager.getOrCreate(fossilStructures.get(pick));
        StructureTemplate overlay = manager.getOrCreate(overlayStructures.get(pick));
        ChunkPos chunk = ChunkPos.containing(origin);
        BoundingBox window = new BoundingBox(chunk.getMinBlockX() - 16, level.getMinY(), chunk.getMinBlockZ() - 16,
            chunk.getMaxBlockX() + 16, level.getMaxY(), chunk.getMaxBlockZ() + 16);
        StructurePlaceSettings settings = new StructurePlaceSettings().setRotation(rotation).setBoundingBox(window).setRandom(random);
        Vec3i size = body.getSize(rotation);
        BlockPos corner = origin.offset(-size.getX() / 2, 0, -size.getZ() / 2);
        int lowest = origin.getY();
        for (int x = 0; x < size.getX(); x++) {
            for (int z = 0; z < size.getZ(); z++) {
                lowest = Math.min(lowest, level.getHeight(Heightmap.Types.OCEAN_FLOOR_WG, corner.getX() + x, corner.getZ() + z));
            }
        }
        int targetY = Math.max(lowest - 15 - random.nextInt(10), level.getMinY() + 10);
        BlockPos target = body.getZeroPositionWithTransform(corner.atY(targetY), Mirror.NONE, rotation);
        if (emptyCorners(level, body.getBoundingBox(settings, target)) > maxEmptyCornersAllowed) {
            return false;
        }
        settings.clearProcessors();
        for (StructureProcessor processor : fossilProcessors.value().list()) {
            settings.addProcessor(processor);
        }
        body.placeInWorld(level, target, target, settings, random, 260);
        settings.clearProcessors();
        for (StructureProcessor processor : overlayProcessors.value().list()) {
            settings.addProcessor(processor);
        }
        overlay.placeInWorld(level, target, target, settings, random, 260);
        return true;
    }
}
