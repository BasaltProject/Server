// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureProcessor;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
import net.minecraft.core.Direction;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructurePlaceSettings;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureProcessorList;
public record TemplateFeature(WeightedList<TemplateEntry> templates, Optional<Holder<StructureProcessorList>> processors) implements Feature {
    private Vec3i shiftFor(Rotation rotation, Direction.Axis axis, StructureTemplate template) {
        return rotation.rotate(axis.getNegative()).getUnitVec3i().multiply(template.getSize().get(axis) / 2);
    }
    public static final MapCodec<TemplateFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.TemplateFeature.class, "CODEC");
    public TemplateFeature(WeightedList<TemplateEntry> templates) {
        this(templates, Optional.empty());
    }
    public static TemplateFeature simple(Identifier id, Holder<StructureProcessorList> processors) {
        return new TemplateFeature(WeightedList.of(TemplateEntry.of(id)), Optional.of(processors));
    }
    public static TemplateFeature simple(Identifier id) {
        return new TemplateFeature(WeightedList.of(TemplateEntry.of(id)), Optional.empty());
    }
    public MapCodec<TemplateFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        TemplateEntry entry = templates.getRandomOrThrow(random);
        List<Rotation> turns = entry.rotations();
        Rotation rotation = turns.get(random.nextInt(turns.size()));
        StructureTemplateManager manager = level.getLevel().getServer().getStructureTemplateManager();
        StructureTemplate template = manager.getOrCreate(entry.template());
        BlockPos pos = origin.offset(shiftFor(rotation, Direction.Axis.X, template)).offset(shiftFor(rotation, Direction.Axis.Z, template));
        StructurePlaceSettings settings = new StructurePlaceSettings().setRotation(rotation).setRandom(random);
        processors.ifPresent(list -> {
            for (StructureProcessor processor : list.value().list()) {
                settings.addProcessor(processor);
            }
        });
        return template.placeInWorld(level, pos, pos, settings, random, 3);
    }
    public static record TemplateEntry(Identifier template, List<Rotation> rotations) {
        public static final Codec<TemplateEntry> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.TemplateFeature.TemplateEntry.class, "CODEC");
        public static TemplateEntry of(Identifier template) {
            return new TemplateEntry(template, List.of(Rotation.values()));


        }
    }
}
