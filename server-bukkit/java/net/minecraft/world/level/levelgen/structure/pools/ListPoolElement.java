// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.pools;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Vec3i;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.templatesystem.LiquidSettings;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
public class ListPoolElement extends StructurePoolElement {
    private final List<StructurePoolElement> elements;
    private void projectAll(StructureTemplatePool.Projection projection) {
        elements.forEach(e -> e.setProjection(projection));
    }
    public static final MapCodec<ListPoolElement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.pools.ListPoolElement.class, "CODEC");
    public ListPoolElement(List<StructurePoolElement> elements, StructureTemplatePool.Projection projection) {
        super(projection);
        if (elements.isEmpty()) {
            throw new IllegalArgumentException("Elements are empty");
        }
        this.elements = elements;
        projectAll(projection);
    }
    public Vec3i getSize(StructureTemplateManager structureTemplateManager, Rotation rotation) {
        int x = 0;
        int y = 0;
        int z = 0;
        for (StructurePoolElement part : elements) {
            Vec3i size = part.getSize(structureTemplateManager, rotation);
            x = Math.max(x, size.getX());
            y = Math.max(y, size.getY());
            z = Math.max(z, size.getZ());
        }
        return new Vec3i(x, y, z);
    }
    public List<StructureTemplate.JigsawBlockInfo> getShuffledJigsawBlocks(StructureTemplateManager structureTemplateManager, BlockPos position, Rotation rotation, RandomSource random) {
        return elements.get(0).getShuffledJigsawBlocks(structureTemplateManager, position, rotation, random);
    }
    public BoundingBox getBoundingBox(StructureTemplateManager structureTemplateManager, BlockPos position, Rotation rotation) {
        Iterable<BoundingBox> boxes = elements.stream().filter(e -> e != EmptyPoolElement.INSTANCE)
            .map(e -> e.getBoundingBox(structureTemplateManager, position, rotation)).toList();
        return BoundingBox.encapsulatingBoxes(boxes).orElseThrow(() -> new IllegalStateException("Unable to calculate boundingbox for ListPoolElement"));
    }
    public boolean place(StructureTemplateManager structureTemplateManager, WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, BlockPos position, BlockPos referencePos, Rotation rotation, BoundingBox chunkBB, RandomSource random, LiquidSettings liquidSettings, boolean keepJigsaws) {
        for (StructurePoolElement part : elements) {
            if (!part.place(structureTemplateManager, level, structureManager, generator, position, referencePos, rotation, chunkBB, random, liquidSettings, keepJigsaws)) {
                return false;
            }
        }
        return true;
    }
    public StructurePoolElementType<?> getType() {
        return StructurePoolElementType.LIST;
    }
    public StructurePoolElement setProjection(StructureTemplatePool.Projection projection) {
        super.setProjection(projection);
        projectAll(projection);
        return this;
    }
    public String toString() {
        return super.toString();
    }
    public List<StructurePoolElement> getElements() {
        return elements;


    }
}
