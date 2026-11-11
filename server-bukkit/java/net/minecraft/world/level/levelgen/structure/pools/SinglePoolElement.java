// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.pools;
import java.util.Comparator;
import java.util.function.Function;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureProcessorType;
import net.minecraft.world.level.levelgen.structure.templatesystem.BlockIgnoreProcessor;
import net.minecraft.world.level.levelgen.structure.templatesystem.JigsawReplacementProcessor;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.nbt.CompoundTag;
import com.mojang.datafixers.util.Either;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.Vec3i;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.templatesystem.LiquidSettings;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructurePlaceSettings;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureProcessorList;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
public class SinglePoolElement extends StructurePoolElement {
    private static <T> com.mojang.serialization.DataResult<T> encodeTemplate(Either<Identifier, StructureTemplate> tpl, com.mojang.serialization.DynamicOps<T> ops, T prefix) {
        return tpl.left().isEmpty() ? com.mojang.serialization.DataResult.error(() -> "Can not serialize a runtime pool element")
            : Identifier.CODEC.encode(tpl.left().get(), ops, prefix);
    }
    private static final Comparator<StructureTemplate.JigsawBlockInfo> kHighestPriorityFirst = Comparator.comparingInt(StructureTemplate.JigsawBlockInfo::selectionPriority).reversed();
    private StructureTemplate resolve(StructureTemplateManager manager) {
        return template.map(manager::getOrCreate, Function.identity());
    }
    public static final MapCodec<SinglePoolElement> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.pools.SinglePoolElement.class, "CODEC");
    protected final Either<Identifier, StructureTemplate> template;
    protected final Holder<StructureProcessorList> processors;
    protected final Optional<LiquidSettings> overrideLiquidSettings;
    protected static <E extends SinglePoolElement> RecordCodecBuilder<E, Holder<StructureProcessorList>> processorsCodec() {
        return StructureProcessorType.LIST_CODEC.fieldOf("processors").forGetter(t -> t.processors);
    }
    protected static <E extends SinglePoolElement> RecordCodecBuilder<E, Optional<LiquidSettings>> overrideLiquidSettingsCodec() {
        return LiquidSettings.CODEC.optionalFieldOf("override_liquid_settings").forGetter(t -> t.overrideLiquidSettings);
    }
    protected static <E extends SinglePoolElement> RecordCodecBuilder<E, Either<Identifier, StructureTemplate>> templateCodec() {
        return com.mojang.serialization.Codec.<Either<Identifier, StructureTemplate>>of(SinglePoolElement::encodeTemplate,
            Identifier.CODEC.map(Either::left)).fieldOf("location").forGetter(t -> t.template);
    }
    protected SinglePoolElement(Either<Identifier, StructureTemplate> template, Holder<StructureProcessorList> processors, StructureTemplatePool.Projection projection, Optional<LiquidSettings> overrideLiquidSettings) {
        super(projection);
        this.template = template;
        this.processors = processors;
        this.overrideLiquidSettings = overrideLiquidSettings;
    }
    public Vec3i getSize(StructureTemplateManager structureTemplateManager, Rotation rotation) {
        return resolve(structureTemplateManager).getSize(rotation);
    }
    public List<StructureTemplate.StructureBlockInfo> getDataMarkers(StructureTemplateManager structureTemplateManager, BlockPos position, Rotation rotation, boolean absolute) {
        List<StructureTemplate.StructureBlockInfo> blocks = resolve(structureTemplateManager)
            .filterBlocks(position, new StructurePlaceSettings().setRotation(rotation), Blocks.STRUCTURE_BLOCK, absolute);
        List<StructureTemplate.StructureBlockInfo> markers = new java.util.ArrayList<>();
        for (StructureTemplate.StructureBlockInfo info : blocks) {
            CompoundTag nbt = info.nbt();
            if (nbt != null && nbt.read("mode", net.minecraft.world.level.block.state.properties.StructureMode.LEGACY_CODEC).orElseThrow()
                == net.minecraft.world.level.block.state.properties.StructureMode.DATA) {
                markers.add(info);
            }
        }
        return markers;
    }
    public List<StructureTemplate.JigsawBlockInfo> getShuffledJigsawBlocks(StructureTemplateManager structureTemplateManager, BlockPos position, Rotation rotation, RandomSource random) {
        List<StructureTemplate.JigsawBlockInfo> jigsaws = resolve(structureTemplateManager).getJigsaws(position, rotation);
        net.minecraft.util.Util.shuffle(jigsaws, random);
        sortBySelectionPriority(jigsaws);
        return jigsaws;
    }
    static void sortBySelectionPriority(List<StructureTemplate.JigsawBlockInfo> blocks) {
        blocks.sort(kHighestPriorityFirst);
    }
    public BoundingBox getBoundingBox(StructureTemplateManager structureTemplateManager, BlockPos position, Rotation rotation) {
        return resolve(structureTemplateManager).getBoundingBox(new StructurePlaceSettings().setRotation(rotation), position);
    }
    public boolean place(StructureTemplateManager structureTemplateManager, WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, BlockPos position, BlockPos referencePos, Rotation rotation, BoundingBox chunkBB, RandomSource random, LiquidSettings liquidSettings, boolean keepJigsaws) {
        StructureTemplate tpl = resolve(structureTemplateManager);
        StructurePlaceSettings settings = getSettings(rotation, chunkBB, liquidSettings, keepJigsaws);
        if (!tpl.placeInWorld(level, position, referencePos, settings, random, 18)) {
            return false;
        }
        for (StructureTemplate.StructureBlockInfo marker : StructureTemplate.processBlockInfos(level, position, referencePos, settings,
            getDataMarkers(structureTemplateManager, position, rotation, false))) {
            handleDataMarker(level, marker, position, rotation, random, chunkBB);
        }
        return true;
    }
    protected StructurePlaceSettings getSettings(Rotation rotation, BoundingBox chunkBB, LiquidSettings liquidSettings, boolean keepJigsaws) {
        StructurePlaceSettings settings = new StructurePlaceSettings();
        settings.setBoundingBox(chunkBB);
        settings.setRotation(rotation);
        settings.setKnownShape(true);
        settings.setIgnoreEntities(false);
        settings.addProcessor(BlockIgnoreProcessor.STRUCTURE_BLOCK);
        settings.setFinalizeEntities(true);
        settings.setLiquidSettings(overrideLiquidSettings.orElse(liquidSettings));
        if (!keepJigsaws) {
            settings.addProcessor(JigsawReplacementProcessor.INSTANCE);
        }
        processors.value().list().forEach(settings::addProcessor);
        getProjection().getProcessors().forEach(settings::addProcessor);
        return settings;
    }
    public StructurePoolElementType<?> getType() {
        return StructurePoolElementType.SINGLE;
    }
    public String toString() {
        return super.toString();
    }
    public Identifier getTemplateLocation() {
        return template.orThrow();
    }
}
