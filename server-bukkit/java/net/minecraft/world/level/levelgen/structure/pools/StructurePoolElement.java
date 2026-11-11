// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.pools;
import com.mojang.datafixers.util.Either;
import java.util.Optional;
import java.util.stream.Collectors;
import net.minecraft.resources.Identifier;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.List;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.Vec3i;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.templatesystem.LiquidSettings;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureProcessorList;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
public abstract class StructurePoolElement {
    private static final Holder<StructureProcessorList> kNoProcessors = Holder.direct(new StructureProcessorList(List.of()));
    private volatile StructureTemplatePool.Projection projection;
    public static final Codec<StructurePoolElement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.pools.StructurePoolElement.class, "CODEC");
    protected static <E extends StructurePoolElement> RecordCodecBuilder<E, StructureTemplatePool.Projection> projectionCodec() {
        return StructureTemplatePool.Projection.CODEC.fieldOf("projection").forGetter(StructurePoolElement::getProjection);
    }
    protected StructurePoolElement(StructureTemplatePool.Projection projection) {
        this.projection = projection;
    }
    public abstract Vec3i getSize(StructureTemplateManager structureTemplateManager, Rotation rotation);
    public abstract List<StructureTemplate.JigsawBlockInfo> getShuffledJigsawBlocks(StructureTemplateManager structureTemplateManager, BlockPos position, Rotation rotation, RandomSource random);
    public abstract BoundingBox getBoundingBox(StructureTemplateManager structureTemplateManager, BlockPos position, Rotation rotation);
    public abstract boolean place(StructureTemplateManager structureTemplateManager, WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, BlockPos position, BlockPos referencePos, Rotation rotation, BoundingBox chunkBB, RandomSource random, LiquidSettings liquidSettings, boolean keepJigsaws);
    public abstract StructurePoolElementType<?> getType();
    public void handleDataMarker(LevelAccessor level, StructureTemplate.StructureBlockInfo dataMarker, BlockPos position, Rotation rotation, RandomSource random, BoundingBox chunkBB) {
        return;
    }
    public StructurePoolElement setProjection(StructureTemplatePool.Projection projection) {
        this.projection = projection;
        return this;
    }
    public StructureTemplatePool.Projection getProjection() {
        StructureTemplatePool.Projection current = projection;
        if (current == null) {
            throw new IllegalStateException();
        }
        return current;
    }
    public int getGroundLevelDelta() {
        return 1;
    }
    public static Function<StructureTemplatePool.Projection, EmptyPoolElement> empty() {
        return p -> EmptyPoolElement.INSTANCE;
    }
    public static Function<StructureTemplatePool.Projection, LegacySinglePoolElement> legacy(String location) {
        return p -> new LegacySinglePoolElement(Either.left(Identifier.parse(location)), kNoProcessors, p, Optional.empty());
    }
    public static Function<StructureTemplatePool.Projection, LegacySinglePoolElement> legacy(String location, Holder<StructureProcessorList> processors) {
        return p -> new LegacySinglePoolElement(Either.left(Identifier.parse(location)), processors, p, Optional.empty());
    }
    public static Function<StructureTemplatePool.Projection, SinglePoolElement> single(String location) {
        return p -> new SinglePoolElement(Either.left(Identifier.parse(location)), kNoProcessors, p, Optional.empty());
    }
    public static Function<StructureTemplatePool.Projection, SinglePoolElement> single(String location, Holder<StructureProcessorList> processors) {
        return p -> new SinglePoolElement(Either.left(Identifier.parse(location)), processors, p, Optional.empty());
    }
    public static Function<StructureTemplatePool.Projection, SinglePoolElement> single(String location, LiquidSettings overrideLiquidSettings) {
        return p -> new SinglePoolElement(Either.left(Identifier.parse(location)), kNoProcessors, p, Optional.of(overrideLiquidSettings));
    }
    public static Function<StructureTemplatePool.Projection, SinglePoolElement> single(String location, Holder<StructureProcessorList> processors, LiquidSettings overrideLiquidSettings) {
        return p -> new SinglePoolElement(Either.left(Identifier.parse(location)), processors, p, Optional.of(overrideLiquidSettings));
    }
    public static Function<StructureTemplatePool.Projection, FeaturePoolElement> feature(Holder<PlacedFeature> feature) {
        return p -> new FeaturePoolElement(feature, p);
    }
    public static Function<StructureTemplatePool.Projection, ListPoolElement> list(List<Function<StructureTemplatePool.Projection, ? extends StructurePoolElement>> elements) {
        return p -> new ListPoolElement(elements.stream().map(e -> (StructurePoolElement) e.apply(p)).collect(Collectors.toList()), p);
    }
}
