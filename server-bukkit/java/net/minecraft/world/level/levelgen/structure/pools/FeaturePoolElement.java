// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.pools;
import net.minecraft.resources.ResourceKey;
import net.minecraft.core.registries.Registries;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.resources.Identifier;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.Vec3i;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.templatesystem.LiquidSettings;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
public class FeaturePoolElement extends StructurePoolElement {
    private final Holder<PlacedFeature> feature;
    public static final MapCodec<FeaturePoolElement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.pools.FeaturePoolElement.class, "CODEC");
    protected FeaturePoolElement(Holder<PlacedFeature> feature, StructureTemplatePool.Projection projection) {
        super(projection);
        this.feature = feature;
    }
    public Vec3i getSize(StructureTemplateManager structureTemplateManager, Rotation rotation) {
        return Vec3i.ZERO;
    }
    public List<StructureTemplate.JigsawBlockInfo> getShuffledJigsawBlocks(StructureTemplateManager structureTemplateManager, BlockPos position, Rotation rotation, RandomSource random) {
        return List.of(new StructureTemplate.JigsawBlockInfo(position,
            org.basaltmc.bukkit.facade.StateProps.with(Blocks.JIGSAW.defaultBlockState(), "orientation", "down_south"),
            net.minecraft.world.level.block.entity.JigsawBlockEntity.JointType.ROLLABLE, null,
            ResourceKey.<StructureTemplatePool>create((ResourceKey<? extends net.minecraft.core.Registry<StructureTemplatePool>>) (ResourceKey<?>) Registries.TEMPLATE_POOL, Identifier.withDefaultNamespace("empty")), Identifier.withDefaultNamespace("empty"), 0, 0));
    }
    public BoundingBox getBoundingBox(StructureTemplateManager structureTemplateManager, BlockPos position, Rotation rotation) {
        Vec3i size = getSize(structureTemplateManager, rotation);
        return new BoundingBox(position.getX(), position.getY(), position.getZ(), position.getX() + size.getX(), position.getY() + size.getY(), position.getZ() + size.getZ());
    }
    public boolean place(StructureTemplateManager structureTemplateManager, WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, BlockPos position, BlockPos referencePos, Rotation rotation, BoundingBox chunkBB, RandomSource random, LiquidSettings liquidSettings, boolean keepJigsaws) {
        return feature.value().place(level, generator, random, position);
    }
    public StructurePoolElementType<?> getType() {
        return StructurePoolElementType.FEATURE;
    }
    public String toString() {
        return super.toString();
    }
}
